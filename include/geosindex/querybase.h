//
// Created by cguo51 on 2/14/24.
//

#ifndef GEOSINDEX_QUERYBASE_H
#define GEOSINDEX_QUERYBASE_H

#include <rocksdb/db.h>

#include <iostream>
#include <fstream>
#include <map>
#include <string>

#include "Constant.h"
#include "MetaDataForFile.pb.h"

namespace geosindex {

    struct QueryResult{
        size_t start;
        size_t end;

        QueryResult() = default;
        QueryResult(size_t start, size_t end): start(start), end(end) {}
    };

    struct QueryBlockResult{
        size_t start;
        size_t end;
        // q acts as slice result for each block
        // the key is the start offset of the slice, to remove redundant between moments and positions 
        std::map<std::string, QueryResult> q;
        
        QueryBlockResult() = default;
        QueryBlockResult(size_t start, size_t end): start(start), end(end) {}
        QueryBlockResult(size_t start, size_t end, std::map<std::string, QueryResult> q): start(start), end(end), q(q) {}
    };

    struct TracingResult{
        size_t start;
        size_t end;
        std::vector<uint64_t> id_data;

        TracingResult() = default;
        TracingResult(size_t start, size_t end): start(start), end(end) {}
        TracingResult(size_t start, size_t end, std::vector<uint64_t> id_data): start(start), end(end), id_data(id_data) {}
    };

    class QueryBase {
    protected:
        /* --- Read Index from Disk --- */
        // define storage backend, for now, only support "file" and "rocksdb"
        std::string storageBackend = "";
        std::string indexSavePath;

        std::fstream diskFile;

        std::map<std::string, MetaDataNodeForFile> metadataMap;
        // 100MB buffer size
        // int diskIndexBufferSizeLimit = 100 * 1024 * 1024;
        // std::string diskIndexBuffer;

        rocksdb::DB *rocksdb_pointer;

        /* --- Secondary Index --- */
        // secondary index type, for now, only support "none", "minmax", "rtree"
        std::string secondaryIndexType = "none";

        std::fstream diskFileSecondary;

    public:
        QueryBase(std::string indexSavePath, std::string storageBackend, std::string secondaryIndexType = "none") : indexSavePath(indexSavePath), storageBackend(storageBackend), secondaryIndexType(secondaryIndexType) {
            if (storageBackend == "file") {
                diskFile.open(indexSavePath + ".index", std::ios::in | std::ios::binary);
                if (!diskFile.is_open()) 
                    std::cerr << "Error opening the file!" << std::endl;
                
                loadFileMetadata(diskFile);

                // open secondary index file
                if (secondaryIndexType != "none") {
                    diskFileSecondary.open(indexSavePath + "_secondary_" + secondaryIndexType + ".index", std::ios::in | std::ios::binary);
                    if (!diskFileSecondary.is_open()) 
                        std::cerr << "Error opening the file!" << std::endl;

                    loadFileMetadata(diskFileSecondary);
                }

            } else if (storageBackend == "rocksdb") {
                rocksdb::Options options;
                options.create_if_missing = true;
                rocksdb::Status status = rocksdb::DB::Open(options, indexSavePath + "_rocksdb", &rocksdb_pointer);
                if (!status.ok()) {
                    std::cerr << "Open RocksDB failed! DB path: " << indexSavePath << " Status:" << status.ToString() << std::endl;
                    std::cerr << "Please build index before query" << std::endl;
                } else {
                    std::cout << "Open RocksDB successfully!" << std::endl;
                }
            } else {
                std::cerr << "Unsupported storage backend!" << std::endl;
            }
        }

        ~QueryBase() {
            if (storageBackend == "file") {
                diskFile.close();

                if (secondaryIndexType != "none") {
                    diskFileSecondary.close();
                }

            } else if (storageBackend == "rocksdb") {
                delete rocksdb_pointer;
            }
        }

        void loadFileMetadata(std::fstream &diskFile) {
                std::string metadata;

                // read last std::size_t to know the length of metadata
                diskFile.seekg(0, std::ios::end);
                std::size_t fileSize = diskFile.tellg();
                diskFile.seekg(fileSize - sizeof(std::size_t));
                std::size_t metadataLength;
                diskFile.read(reinterpret_cast<char *>(&metadataLength), sizeof(metadataLength));
                std::cout << "metadata length: " << metadataLength << std::endl; 

                // read the metadata
                diskFile.seekg(fileSize - sizeof(std::size_t) - metadataLength);
                metadata.resize(metadataLength);
                diskFile.read(&metadata[0], metadataLength);

                MetaDataListForFile metaDataListForFile;
                metaDataListForFile.ParseFromString(metadata);

                for (const auto &metaDataNodeForFile : metaDataListForFile.metadatanodeforfile()) {
                    metadataMap[metaDataNodeForFile.keyname()] = metaDataNodeForFile;
                }
        }

        void printMetadata() {
            for (const auto &metaData : metadataMap) {
                std::cout << "key: " << metaData.first << std::endl;
                std::cout << "startbytes: " << metaData.second.startbytes() << std::endl;
                std::cout << "length: " << metaData.second.length() << std::endl;
            }
        }

        template<typename T>
        void readNodeList(const std::string &key, std::fstream &diskFile, T& message){
            if (storageBackend == "file") {
                auto metaData = metadataMap[key];
                // if metadata not found, return
                if (metaData.startbytes() == 0 && metaData.length() == 0) {
                    std::cerr << "metadata not found!" << std::endl;
                    return;
                }
                diskFile.seekg(metaData.startbytes(), std::ios::beg);
                char *buffer = new char[metaData.length()];
                diskFile.read(buffer, metaData.length());
                message.ParseFromArray(buffer, metaData.length());
            } else if (storageBackend == "rocksdb") {
                std::string value;
                rocksdb::Status s = rocksdb_pointer->Get(rocksdb::ReadOptions(), key, &value);
                if (s.ok()) {
                    message.ParseFromString(value);
                } else if (s.IsNotFound()) {
                    std::cerr << "rocksdb key not found: " << key << std::endl;
                } else {
                    std::cerr << "rocksdb get error: " << s.ToString() << std::endl;
                }
            }
        }
    };}

#endif //GEOSINDEX_QUERYBASE_H

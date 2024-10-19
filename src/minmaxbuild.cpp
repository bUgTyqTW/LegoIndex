#include <geosindex/minmaxbuild.h>

namespace geosindex {
    void MinMaxBuild::processMinMaxNode(const std::vector<double> &data, const std::string &key, const size_t &start, const size_t &end) {
        MinMaxNode minMaxNode;
        if (secondaryIndexType == "none") {
            minMaxNode.set_min(*std::min_element(data.begin(), data.end()));
            minMaxNode.set_max(*std::max_element(data.begin(), data.end()));

        } else if (secondaryIndexType == "minmax") {
            minMaxNode.set_min(DoubleInfinity);
            minMaxNode.set_max(-DoubleInfinity);
            for (size_t i = 0; i < data.size(); i += inblockSliceSize) {
                // end index of the slice, not included
                auto begin_p = data.begin() + i;
                auto end_p = std::min(data.begin() + i + inblockSliceSize, data.end());
                
                MinMaxNode minMaxInblockNode;
                minMaxInblockNode.set_min(*std::min_element(begin_p, end_p));
                minMaxInblockNode.set_max(*std::max_element(begin_p, end_p));
                minMaxInblockNode.set_start(start + i);
                minMaxInblockNode.set_end(std::min(start + i + inblockSliceSize, end));

                {
                    std::lock_guard<std::mutex> lock(minMaxListMapMutex_inblock);
                    // key + start as the inblock key
                    minMaxListMap_inblock[key + std::to_string(start)].add_minmaxnodes()->CopyFrom(minMaxInblockNode);

                    minMaxNode.set_min(std::min(minMaxNode.min(), minMaxInblockNode.min()));
                    minMaxNode.set_max(std::max(minMaxNode.max(), minMaxInblockNode.max()));
                }
            }
        }

        minMaxNode.set_start(start);
        minMaxNode.set_end(end);

        {
            std::lock_guard<std::mutex> lock(minMaxListMapMutex);
            minMaxListMap[key].add_minmaxnodes()->CopyFrom(minMaxNode);
        }
    }

    void MinMaxBuild::processBlockData(BlockData blockData) {
        
        processMinMaxNode(blockData.blockXData, blockData.key + "x", blockData.blockStart, blockData.blockStart + blockData.blockCount);
        processMinMaxNode(blockData.blockYData, blockData.key + "y", blockData.blockStart, blockData.blockStart + blockData.blockCount);
        processMinMaxNode(blockData.blockZData, blockData.key + "z", blockData.blockStart, blockData.blockStart + blockData.blockCount);

    }

    void MinMaxBuild::writeIndexToFile_minmax(const std::map<std::string, MinMaxList> &minMaxListMap, std::fstream &diskFile, int &diskFileCursor){
        MetaDataListForFile metaDataListForFile;
        // for key in minMaxListMap
        for (const auto &minMaxListPair : minMaxListMap) {
            std::string serializeListStr;
            minMaxListPair.second.SerializeToString(&serializeListStr);
            diskFile.write(serializeListStr.c_str(), serializeListStr.length());

            MetaDataNodeForFile metaDataNodeForFile;
            metaDataNodeForFile.set_startbytes(diskFileCursor);
            metaDataNodeForFile.set_length(serializeListStr.length());
            metaDataNodeForFile.set_keyname(minMaxListPair.first);
            metaDataListForFile.add_metadatanodeforfile()->CopyFrom(metaDataNodeForFile);
//            std::cout << "Node Info: " << metaDataNodeForFile.startbytes() << " " << metaDataNodeForFile.length() << " " << metaDataNodeForFile.keyname() << std::endl;
            diskFileCursor += serializeListStr.length();
        }

        // serialize the minmax metadata
        std::string serializeMetaDataListStr;
        metaDataListForFile.SerializeToString(&serializeMetaDataListStr);

        std::size_t metadataLength = serializeMetaDataListStr.length();
        diskFile.write(serializeMetaDataListStr.c_str(), metadataLength);
        diskFileCursor += metadataLength;

        // write the length of metadata to the end of the file
        diskFile.write(reinterpret_cast<const char *>(&metadataLength), sizeof(metadataLength));

        std::cout << "metadata length: " << serializeMetaDataListStr.length() << std::endl;
    }

    void MinMaxBuild::writeIndexToFile() {
        std::cout << "length of minMaxListMap: " << minMaxListMap.size() << std::endl;
        for (const auto &minMaxListPair : minMaxListMap) {
            std::cout << "key: " << minMaxListPair.first << " length: " << minMaxListPair.second.minmaxnodes_size() << std::endl;
        }
        writeIndexToFile_minmax(minMaxListMap, diskFile, diskFileCursor);
        if (secondaryIndexType == "minmax") {
            std::cout << "length of minMaxListMap_inblock: " << minMaxListMap_inblock.size() << std::endl;
            writeIndexToFile_minmax(minMaxListMap_inblock, diskFileSecondary, diskFileCursorSecondary);
        }
    }

    void MinMaxBuild::writeIndexToRocksDB() {
        for (const auto &minMaxListPair : minMaxListMap) {
            std::string serializeListStr;
            minMaxListPair.second.SerializeToString(&serializeListStr);
            if (rocksdb_write_batch_flag){
                writeKVToBatch(minMaxListPair.first, serializeListStr);
            } else {
                writeKVToRocksDB(minMaxListPair.first, serializeListStr);
            }
        }
        if (rocksdb_write_batch_flag){
            writeBatchToRocksDB();
        }
        if (secondaryIndexType == "minmax") {
            int count = 0;
            for (const auto &minMaxListPair : minMaxListMap_inblock) {
                count++;
                std::string serializeListStr;
                minMaxListPair.second.SerializeToString(&serializeListStr);
                if (rocksdb_write_batch_flag){
                    writeKVToBatch(minMaxListPair.first, serializeListStr);
                    if (count % 1000 == 0){
                        writeBatchToRocksDB();
                    }
                } else {
                    writeKVToRocksDB(minMaxListPair.first, serializeListStr);
                }
            }
        }
        if (rocksdb_write_batch_flag){
            writeBatchToRocksDB();
        }
    }

};  
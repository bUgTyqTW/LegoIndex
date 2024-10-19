#ifndef GEOSINDEX_BUILDBASE_H
#define GEOSINDEX_BUILDBASE_H

#include <rocksdb/db.h>
#include <adios2.h>

#include <fstream>
#include <thread>
#include <mutex>
#include <queue>
#include <chrono>
#include <filesystem>

#include "Constant.h"
#include "MetaDataForFile.pb.h"

using ROCKSDB_NAMESPACE::WriteBatch;

namespace geosindex {

    // struct for the batch read job
    struct BatchReadJob {
        adios2::Dims start;
        adios2::Dims count;

        std::string key;
        // particle character
        std::string particleCharacter;
    };

    // struct for block data
    struct BlockData {
        std::vector<double>  blockXData;
        std::vector<double>  blockYData;
        std::vector<double>  blockZData;

        size_t blockStart;
        size_t blockCount;
        std::string key;
    };


    class BuildBase {
    protected:
        std::string bpFileName;
        std::vector<std::string> particleCharacters;
        std::string species;

        adios2::ADIOS adios;
        adios2::IO bpIO;
        adios2::Engine bpReader;

        /* --- Read and Build Index --- */
        // Define the number of threads of index building jobs
        int maxThreads = 16;

        // Define the number of threads of adios read
        int nThreads = 16;

        // Define the iteration number
        int iteration = 500;

        // read block batch size
        int blockBatchSize = 10000;

        // read job queue
        std::queue<BatchReadJob> readJobQueue;

        // Define a mutex to protect the shared data (readJobQueue)
        std::mutex readJobMutex;

        // Define a mutex to protect job distribution
        std::mutex distributeJobMutex;

        // A queue to hold the block info with data
        std::queue<BlockData> blockDataQueue;

        // an bool end flag to indicate the end of the read job
        bool end_read_job = false;

        // an bool end flag to indicate the end of the build job
        bool end_build_job = false;

        /* --- Write Index To Disk --- */
        // define storage backend, for now, only support "file" and "rocksdb"
        std::string storageBackend = "";
        std::string indexSavePath;

        std::fstream diskFile;
        int diskFileCursor = 0;
        // 100MB buffer size
        // int diskIndexBufferSizeLimit = 100 * 1024 * 1024;
        // std::string diskIndexBuffer;

        rocksdb::DB *rocksdb_pointer;
        WriteBatch batch;

        // Define a mutex to protect the shared data (rocksdb_pointer)
        std::mutex rocksdbMutex;

        /* --- Secondary Index --- */
        // secondary index type, for now, only support "none", "minmax", rtree
        // secondary index should be saved in a separate file and initialized separately
        std::string secondaryIndexType = "none";
        
        std::fstream diskFileSecondary;
        int diskFileCursorSecondary = 0;

        // inside each block, the number of particles to be processed in each slice
        int inblockSliceSize = 100;

        bool rocksdb_write_batch_flag = true;

        /* --- Time Monitor --- */
        // due to some memory leak issue inside the ADIODS, specifically in the adios2::TransportMan::OpenFileTransport and adios2::transportman::TransportMan::OpenFiles functions. 
        std::chrono::high_resolution_clock::time_point build_start = std::chrono::high_resolution_clock::now();

    public:
        BuildBase(std::string bpFileName, 
        std::vector<std::string> particleCharacters = {"position", "momentum"}, 
        std::string species = "electrons",
        int maxThreads = 16,
        int nThreads = 16, 
        int iteration = 500,
        int blockBatchSize = 10000,
        std::string storageBackend = "file",
        std::string indexSavePath = "index",
        std::string secondaryIndexType = "none",
        int inblockSliceSize = 100) :
        bpFileName(bpFileName),
        particleCharacters(particleCharacters),
        species(species),
        maxThreads(maxThreads),
        nThreads(nThreads),
        iteration(iteration),
        blockBatchSize(blockBatchSize),
        storageBackend(storageBackend),
        indexSavePath(indexSavePath),
        secondaryIndexType(secondaryIndexType),
        inblockSliceSize(inblockSliceSize
        ) {

            bpIO = adios.DeclareIO("ReadBP");
            bpIO.SetParameter("Threads", std::to_string(nThreads));
            bpReader = bpIO.Open(bpFileName, adios2::Mode::Read);

            if (storageBackend == "file") {
                // open index file
                diskFile.open(indexSavePath + ".index", std::ios::out | std::ios::binary);
                if (!diskFile.is_open()) 
                    std::cerr << "Error opening the file!" << std::endl;
                
                // open secondary index file
                if (secondaryIndexType != "none") {
                    diskFileSecondary.open(indexSavePath + "_secondary_" + secondaryIndexType + ".index", std::ios::out | std::ios::binary);
                    if (!diskFileSecondary.is_open()) 
                        std::cerr << "Error opening the file!" << std::endl;
                }

            } else if (storageBackend == "rocksdb") {
                indexSavePath = indexSavePath + "_rocksdb";
                // if exists, delete the existing rocksdb
                if (std::filesystem::exists(indexSavePath))
                    std::filesystem::remove_all(indexSavePath);

                // open a new rocksdb
                rocksdb::Options options;
                options.create_if_missing = true;
                rocksdb::Status status = rocksdb::DB::Open(options, indexSavePath, &rocksdb_pointer);

                if (!status.ok()) 
                    std::cerr << "Error opening the rocksdb!" << std::endl;
            
            }
        }

        ~BuildBase() {
            if (storageBackend == "file") {
                diskFile.close();

                if (secondaryIndexType != "none") {
                    diskFileSecondary.close();
                }
                
            } else if (storageBackend == "rocksdb") {
                delete rocksdb_pointer;
            }
            auto build_end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> build_time = build_end - build_start;
            std::cout << "Build time: " << build_time.count() << "s" << std::endl;
            
            // some issue with the adios2 when closing on Perlmutter
            bpReader.Close();
        }

        // read the metadata, based on blockBatchSize, generate the read job queue
        void initJobsQueue();

        void batchRead();

        void buildParallel();

        void readIDData(const std::string &key, const adios2::Dims &start, const adios2::Dims &count, std::vector<uint64_t> &data);

        void buildIndexByBlock();

        void buildIndexByBatch();

        void writeIndex();

        void writeKVToBatch(const std::string &key, const std::string &value);

        void writeKVToRocksDB(const std::string &key, const std::string &value);

        void writeBatchToRocksDB();

        virtual void processBlockData(BlockData blockData) = 0;

        virtual void writeIndexToFile() = 0;

        virtual void writeIndexToRocksDB() = 0;
    };

};



#endif //GEOSINDEX_BUILDBASE_H
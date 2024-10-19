//
// Created by cguo51 on 2/7/24.
//
#include <geosindex/minmaxbuild.h>
#include <geosindex/rtreebuild.h>

int main(int argc, char **argv) {
    std::string bpFileName = "/data/gc/rocksdb-index/WarpX/build/bin/diags/diag2/openpmd.bp";
    int maxThreads = 16;
    int nThreads = 16;
    int iteration = 500;
    std::string indexSaveName = "diag2";
    int i = 1;
    int blockBatchSize = 0;
    std::string indexType = "minmax";
    std::vector<std::string> particleCharacters = {"position", "momentum"};
    std::string species = "electrons";
    std::string storageBackend = "file";
    std::string secondaryIndexType = "none";
    int inblockSliceSize = 100;
    bool build_bloom_filter = false;
    while (i < argc) {
        if (std::string(argv[i]) == "-f") {
            bpFileName = std::string(argv[i + 1]);
            i++;
        }
        else if (std::string(argv[i]) == "-m") {
            maxThreads = std::stoi(argv[i + 1]);
            i++;
        }
        else if (std::string(argv[i]) == "-n") {
            nThreads = std::stoi(argv[i + 1]);
            i++;
        }
        else if (std::string(argv[i]) == "--iteration") {
            iteration = std::stoi(argv[i + 1]);
            i++;
        }
        else if (std::string(argv[i]) == "-i") {
            indexSaveName = std::string(argv[i + 1]);
            i++;
        }
        else if (std::string(argv[i]) == "-b" or std::string(argv[i]) == "--batch") {
            blockBatchSize = std::stoi(argv[i + 1]);
            i++;
        }
        else if (std::string(argv[i]) == "-t" or std::string(argv[i]) == "--type") {
            indexType = std::string(argv[i + 1]);
            i++;
        }
        else if (std::string(argv[i]) == "-p" or std::string(argv[i]) == "--particleCharacters") {
            particleCharacters = {std::string(argv[i + 1])};
            i++;
        }
        else if (std::string(argv[i]) == "-s" or std::string(argv[i]) == "--species") {
            species = std::string(argv[i + 1]);
            i++;
        }
        else if (std::string(argv[i]) == "-d" or std::string(argv[i]) == "--storageBackend") {
            storageBackend = std::string(argv[i + 1]);
            i++;
        }
        else if (std::string(argv[i]) == "-x" or std::string(argv[i]) == "--secondaryIndexType") {
            secondaryIndexType = std::string(argv[i + 1]);
            i++;
        }
        else if (std::string(argv[i]) == "-l" or std::string(argv[i]) == "--inblockSliceSize") {
            inblockSliceSize = std::stoi(argv[i + 1]);
            i++;
        }
        else if (std::string(argv[i]) == "-bloom") {
            build_bloom_filter = true;
        }
        else if (std::string(argv[i]) == "-h" or std::string(argv[i]) == "--help") {
            std::cout << "Usage: " << argv[0] << " [-f bpFileName] [-n nThreads] [-i indexSaveName] [-b blockBatchSize] [-t indexType] [-p particleCharacters]" << std::endl;
            return 0;
        }
        i++;
    }

    // calculate the running time
    auto start = std::chrono::high_resolution_clock::now();
    if (indexType == "minmax") {
        geosindex::MinMaxBuild minMaxBuild(bpFileName, particleCharacters, species, maxThreads, nThreads, iteration, blockBatchSize, storageBackend, indexSaveName, secondaryIndexType, inblockSliceSize);
        if (blockBatchSize) {
            minMaxBuild.buildIndexByBatch();
        } else {
            minMaxBuild.buildIndexByBlock();
        }
    } else if (indexType == "rtree") {
        geosindex::RTreeBuild rTreeBuild(bpFileName, particleCharacters, species, maxThreads, nThreads, iteration, blockBatchSize, storageBackend, indexSaveName, secondaryIndexType, build_bloom_filter, inblockSliceSize);
        if (blockBatchSize) {
            rTreeBuild.buildIndexByBatch();
        } else {
            rTreeBuild.buildIndexByBlock();
        }
    } else {
        std::cerr << "Index type not supported!" << std::endl;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Elapsed time: " << elapsed.count() << " s\n";
}
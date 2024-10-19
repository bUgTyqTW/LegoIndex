//
// Created by cguo51 on 2/7/24.
//
#include <geosindex/minmaxquery.h>
#include <geosindex/rtreequery.h>

int main(int argc, char **argv) {
    std::string bpFileName = "/data/gc/rocksdb-index/WarpX/build/bin/diags/diag1/openpmd.bp";
    int nThreads = 16;
    std::string indexSaveName = "diag";
    int i = 1;
    std::string indexType = "minmax";
    std::string queryKey = "/data/300/particles/electrons/position/x";
    std::string storageBackend = "file";
    std::string secondaryIndexType = "none";
    int inblockSliceSize = 100;

    while (i < argc) {
        if (std::string(argv[i]) == "-f") {
            bpFileName = std::string(argv[i + 1]);
            i++;
        }
        else if (std::string(argv[i]) == "-n") {
            nThreads = std::stoi(argv[i + 1]);
            i++;
        }
        else if (std::string(argv[i]) == "-i") {
            indexSaveName = std::string(argv[i + 1]);
            i++;
        }
        else if (std::string(argv[i]) == "-t" or std::string(argv[i]) == "--type") {
            indexType = std::string(argv[i + 1]);
            i++;
        }
        else if (std::string(argv[i]) == "-k" or std::string(argv[i]) == "--key") {
            queryKey = std::string(argv[i + 1]);
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
        else if (std::string(argv[i]) == "-h" or std::string(argv[i]) == "--help") {
            std::cout << "Usage: " << argv[0] << " [-f bpFileName] [-n nThreads] [-i indexSaveName]" << std::endl;
            return 0;
        }
        i++;
    }
    if (indexType == "minmax") {
        geosindex::MinMaxQuery minMaxQuery(indexSaveName, storageBackend, secondaryIndexType);
        minMaxQuery.printMetadata();

//        MinMaxList minMaxList;
//        minMaxQuery.queryMinMaxMetaData(queryKey, minMaxList);
//
//        for (auto &minMaxData : minMaxList.minmaxnodes()) {
//            double min = minMaxData.min();
//            double max = minMaxData.max();
//            std::cout << "min: " << min << " max: " << max << std::endl;
//        }

        std::map<std::string, geosindex::QueryBlockResult> queryResults = minMaxQuery.queryMinMaxData(queryKey, 0, 2);
        for (auto &queryResult : queryResults) {
            std::cout << "start: " << queryResult.second.start << " end: " << queryResult.second.end << std::endl;
        }
        // print the length of the query result
        std::cout << "length: " << queryResults.size() << std::endl;
    } else if (indexType == "rtree") {
        geosindex::RTreeQuery rTreeQuery(indexSaveName, storageBackend, secondaryIndexType);
        rTreeQuery.printMetadata();

        // test queryTreeMetaData
        // Note: the key is different from the key in the minmax query!!!
        // rTreeQuery.queryRTreeMetaData(queryKey);

        // test id tracing
        std::vector<uint64_t> tracingID = {77309411458, 77309411459, 77309411460};


    {
        auto start = std::chrono::high_resolution_clock::now();
        std::map<std::string, geosindex::TracingResult> tracingResultMap = rTreeQuery.queryRTreeTracing(queryKey, tracingID);
        std::cout << "tracing result size: " << tracingResultMap.size() << std::endl;
        for (auto &tracingResult : tracingResultMap) {
            std::cout << "key: " << tracingResult.first << " start: " << tracingResult.second.start << " end: " << tracingResult.second.end << " id_list: ";
            for (auto &id : tracingResult.second.id_data) {
                std::cout << id << " ";
            }
            std::cout << std::endl;
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        std::cout << "Position Elapsed time: " << elapsed.count() << " s\n\n\n";
    }

        {
        auto start = std::chrono::high_resolution_clock::now();
        std::map<std::string, geosindex::TracingResult> tracingResultMap = rTreeQuery.queryRTreeTracingInteracted(queryKey, tracingID);
        std::cout << "tracing result size: " << tracingResultMap.size() << std::endl;
        for (auto &tracingResult : tracingResultMap) {
            std::cout << "key: " << tracingResult.first << " start: " << tracingResult.second.start << " end: " << tracingResult.second.end << " id_list: ";
            for (auto &id : tracingResult.second.id_data) {
                std::cout << id << " ";
            }
            std::cout << std::endl;
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        std::cout << "Interacted Elapsed time: " << elapsed.count() << " s\n\n\n";
    }
    
    {
        auto start = std::chrono::high_resolution_clock::now();
        std::string momentum_key = queryKey;
        momentum_key.replace(momentum_key.find("position"), 8, "momentum");
        std::cout << "momentum_key: " << momentum_key << std::endl;
        std::map<std::string, geosindex::TracingResult> tracingResultMap = rTreeQuery.queryRTreeTracing(momentum_key, tracingID);
        std::cout << "tracing result size: " << tracingResultMap.size() << std::endl;
        for (auto &tracingResult : tracingResultMap) {
            std::cout << "key: " << tracingResult.first << " start: " << tracingResult.second.start << " end: " << tracingResult.second.end << " id_list: ";
            for (auto &id : tracingResult.second.id_data) {
                std::cout << id << " ";
            }
            std::cout << std::endl;
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        std::cout << "Momentum Elapsed time: " << elapsed.count() << " s\n\n\n";
    }


    } else {
        std::cerr << "Index type not supported!" << std::endl;
    }
    return 0;
}
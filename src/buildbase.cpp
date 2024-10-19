#include <geosindex/buildbase.h>
#include <chrono>
#include <iostream>

namespace geosindex {

    void BuildBase::buildIndexByBlock() {
        const std::map<std::string, adios2::Params> meta_variables = bpIO.AvailableVariables(true);

        std::vector<double> myDoubleXData;
        std::vector<double> myDoubleYData;
        std::vector<double> myDoubleZData;

        for (const auto &particleCharacter : particleCharacters) {
            for (const auto &variablePair: meta_variables) {
                if (variablePair.first.find(particleCharacter) != std::string::npos &&
                    variablePair.first.find(species) != std::string::npos &&
                    variablePair.first.find(std::to_string(iteration)) != std::string::npos &&
                    variablePair.first.back() == 'x') {
                    std::string key = variablePair.first.substr(0, variablePair.first.length() - 1);

                    adios2::Variable<double> x_meta_info = bpIO.InquireVariable<double>(key + "x");
                    adios2::Variable<double> y_meta_info = bpIO.InquireVariable<double>(key + "y");
                    adios2::Variable<double> z_meta_info = bpIO.InquireVariable<double>(key + "z");
                        
                    auto BlocksInfo = bpReader.AllStepsBlocksInfo(x_meta_info);

                    for (auto it = BlocksInfo.begin(); it != BlocksInfo.end(); ++it) {
                        const auto &var_vec = it->second;
                        for (size_t i = 0; i < var_vec.size(); ++i) {

                            x_meta_info.SetSelection({var_vec[i].Start, var_vec[i].Count});
                            y_meta_info.SetSelection({var_vec[i].Start, var_vec[i].Count});
                            z_meta_info.SetSelection({var_vec[i].Start, var_vec[i].Count});

                             bpReader.Get<double>(x_meta_info, myDoubleXData, adios2::Mode::Deferred);
                             bpReader.Get<double>(y_meta_info, myDoubleYData, adios2::Mode::Deferred);
                             bpReader.Get<double>(z_meta_info, myDoubleZData, adios2::Mode::Deferred);
                             // perform get
                             bpReader.PerformGets();

//                            bpReader.Get<double>(x_meta_info, myDoubleXData, adios2::Mode::Sync);
//                            bpReader.Get<double>(y_meta_info, myDoubleYData, adios2::Mode::Sync);
//                            bpReader.Get<double>(z_meta_info, myDoubleZData, adios2::Mode::Sync);

                            struct BlockData blockData;
                            blockData.blockXData = myDoubleXData;
                            blockData.blockYData = myDoubleYData;
                            blockData.blockZData = myDoubleZData;

                            blockData.blockStart = var_vec[i].Start[0];
                            blockData.blockCount = var_vec[i].Count[0];

                            blockData.key = key;

                            processBlockData(blockData);
                        }
                    }
                }
            }
        }

        writeIndex();
    }

    void BuildBase::buildIndexByBatch() {
        initJobsQueue();
        auto start = std::chrono::high_resolution_clock::now();
        std::thread batchReadThread(&BuildBase::batchRead, this);
        buildParallel();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        std::cout << "IO and CPU Finished in " << elapsed.count() << "s" << std::endl;
        writeIndex();
        batchReadThread.join();
    }

    void BuildBase::initJobsQueue() {
        const std::map<std::string, adios2::Params> meta_variables = bpIO.AvailableVariables(true);
        std::vector<std::vector<size_t>> blockMetaInfo;

        for (const auto &particleCharacter: particleCharacters) {
            for (const auto &variablePair: meta_variables) {
                if (variablePair.first.find(particleCharacter) != std::string::npos && 
                    variablePair.first.find(species) != std::string::npos &&
                    variablePair.first.find(std::to_string(iteration)) != std::string::npos &&
                    variablePair.first.back() == 'x') {
                    std::string key = variablePair.first.substr(0, variablePair.first.length() - 1);

                    // key: /data/9000/particles/hydrogen/momentum, iteration: 9000
                    // key: "/data/0/particles/electrons/position/", iteration: 0
                    size_t iteration = std::stoul(key.substr(6, key.find("/", 6) - 6));

                    adios2::Variable<double> x_meta_info = bpIO.InquireVariable<double>(key + "x");
                    auto xBlocksInfo = bpReader.AllStepsBlocksInfo(x_meta_info);
                    auto x_it = xBlocksInfo.begin();
                    for (; x_it != xBlocksInfo.end(); ++x_it) {
                        const auto &var_vec1 = x_it->second;
                        adios2::Dims blockBatchStart = var_vec1[0].Start;
                        adios2::Dims blockBatchCount = var_vec1[0].Count;

                        for (size_t i = 0; i < var_vec1.size(); ++i) {
                            const auto &var_info1 = var_vec1[i];
                            blockMetaInfo.push_back({iteration, var_info1.Start[0], var_info1.Count[0]});

                            // if i % blockBatchSize == 0 or reach the end, push the job to the job queue
                            if ((i + 1) % blockBatchSize == 0 || i == var_vec1.size() - 1) {
                                // assign count based on the first and last block, last block start + count - first block start
                                for (size_t j = 0; j < var_info1.Count.size(); ++j) {
                                    blockBatchCount[j] = var_info1.Start[j] + var_info1.Count[j] - blockBatchStart[j];
                                }

                                // push the job to the job queue
                                BatchReadJob batchReadJob;
                                batchReadJob.start = blockBatchStart;
                                batchReadJob.count = blockBatchCount;
                                batchReadJob.key = key;
                                batchReadJob.particleCharacter = particleCharacter;
                                readJobQueue.push(batchReadJob);

                                // reset the blockBatchStart and blockBatchCount
                                for (size_t j = 0; j < var_info1.Count.size(); ++j) {
                                    blockBatchStart[j] = var_info1.Start[j] + var_info1.Count[j];
                                }
                            }
                        }
                    }
                }
            }
        }
        // indexSavePath: diag2_minmax or diag2_rtree, remove the last part of the path
        std::string indexSavePathRaw = indexSavePath.substr(0, indexSavePath.find_last_of("_"));
        std::ofstream file(indexSavePathRaw + ".blockmeta");
        if (!file.is_open()) {
            std::cerr << "Error opening file: " << indexSavePath + ".blockmeta" << std::endl;
            return;
        }
        
        // write blockMetaInfo
        for (size_t i = 0; i < blockMetaInfo.size(); i++) {
            file << blockMetaInfo[i][0] << "," << blockMetaInfo[i][1] << "," << blockMetaInfo[i][2] << std::endl;
        }

        file.close();
    
    }

    void BuildBase::batchRead(){
        // while jobsQueue is not empty
        while(!readJobQueue.empty()){
            // if the size of secondaryJobsQueue is larger than half of blockBatchSize, wait for 1 second
            while(blockDataQueue.size() > blockBatchSize / 2){
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            BatchReadJob batchReadJob;
            {
                // Use std::unique_lock to explicitly lock and unlock the mutex
                std::lock_guard<std::mutex> lock(readJobMutex);
                if (readJobQueue.empty()) {
                    break;
                }
                batchReadJob = readJobQueue.front();
                readJobQueue.pop();

                std::cout << "Start batch Read: Thread ID: " << std::this_thread::get_id() << "  Key: " << batchReadJob.key << "  Start: " << batchReadJob.start[0] << "  Count: "  << batchReadJob.count[0] << std::endl;
            }
                                // push
            {
                std::lock_guard<std::mutex> lock(distributeJobMutex);

                auto start = std::chrono::high_resolution_clock::now();

                std::vector<double> myDoubleX;
                std::vector<double> myDoubleY;
                std::vector<double> myDoubleZ;

                // read data from bp file
                adios2::Variable<double> x_meta_info = bpIO.InquireVariable<double>(batchReadJob.key + "x");
                adios2::Variable<double> y_meta_info = bpIO.InquireVariable<double>(batchReadJob.key + "y");
                adios2::Variable<double> z_meta_info = bpIO.InquireVariable<double>(batchReadJob.key + "z");

                x_meta_info.SetSelection({batchReadJob.start, batchReadJob.count});
                y_meta_info.SetSelection({batchReadJob.start, batchReadJob.count});
                z_meta_info.SetSelection({batchReadJob.start, batchReadJob.count});

                bpReader.Get<double>(x_meta_info, myDoubleX, adios2::Mode::Deferred);
                bpReader.Get<double>(y_meta_info, myDoubleY, adios2::Mode::Deferred);
                bpReader.Get<double>(z_meta_info, myDoubleZ, adios2::Mode::Deferred);
                // perform get
                bpReader.PerformGets();

                // push data to secondaryJobsQueue
                // Note: this copy only works for 1D particle data, ND not work
                auto xBlocksInfo = bpReader.AllStepsBlocksInfo(x_meta_info);
                auto x_it = xBlocksInfo.begin();
                for (; x_it != xBlocksInfo.end(); ++x_it) {
                    const auto &var_vec1 = x_it->second;
                    for (size_t i = 0; i < var_vec1.size(); ++i) {
                        // if current block start != blockBatchStart, skip
                        if (var_vec1[i].Start[0] < batchReadJob.start[0]) {
                            continue;
                        }

                        // if reach the end of blockBatchSize, break
                        if (var_vec1[i].Start[0] >= batchReadJob.start[0] + batchReadJob.count[0]) {
                            break;
                        }

                        // pick the corresponding part of data push to secondaryJobsQueue
                        std::vector<double> myDoubleXData(myDoubleX.begin() + var_vec1[i].Start[0] - batchReadJob.start[0],
                                                            myDoubleX.begin() + var_vec1[i].Start[0] - batchReadJob.start[0] + var_vec1[i].Count[0]);
                        std::vector<double> myDoubleYData(myDoubleY.begin() + var_vec1[i].Start[0] - batchReadJob.start[0],
                                                            myDoubleY.begin() + var_vec1[i].Start[0] - batchReadJob.start[0] + var_vec1[i].Count[0]);
                        std::vector<double> myDoubleZData(myDoubleZ.begin() + var_vec1[i].Start[0] - batchReadJob.start[0],
                                                            myDoubleZ.begin() + var_vec1[i].Start[0] - batchReadJob.start[0] + var_vec1[i].Count[0]);
                        
                        // push info to secondary job list
                        struct BlockData blockData;
                        blockData.blockXData = myDoubleXData;
                        blockData.blockYData = myDoubleYData;
                        blockData.blockZData = myDoubleZData;

                        blockData.blockStart = var_vec1[i].Start[0];
                        blockData.blockCount = var_vec1[i].Count[0];
                        // std::cout << "block start:" << var_vec1[i].Start[0] << std::endl;
                        blockData.key = batchReadJob.key;

                        blockDataQueue.push(blockData);

                    }
                }

                // free the memory of myDouble
                myDoubleX.clear();
                myDoubleY.clear();
                myDoubleZ.clear();

                auto end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> elapsed = end - start;
                std::cout << "Read Batch finished in " << elapsed.count() << "s" << std::endl;
            }
            // std::cout << "Read Batch finished" << std::endl;
        }

        // set end flag to true
        end_read_job = true;
    }

    void BuildBase::buildParallel(){
        // Create a vector to store the thread objects
        std::vector<std::thread> threads;

        // Create and start the worker threads
        for (int i = 0; i < maxThreads; ++i) {
            threads.emplace_back([this]() {
                while (true) {

                    BlockData blockData;

                    {
                        std::lock_guard<std::mutex> lock(distributeJobMutex);

                        if (end_read_job && blockDataQueue.empty()) {
                            break;
                        }

                        if (blockDataQueue.empty()) {
                            // Release mutex before sleeping
                            distributeJobMutex.unlock();
                            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                            distributeJobMutex.lock();
                            continue;
                        }

                        blockData = blockDataQueue.front();
                        blockDataQueue.pop();
                    }
                    
                    // Process the job
                    processBlockData(blockData);
                }
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        for (auto &thread: threads) {
            thread.join();
        }
    }

    void BuildBase::readIDData(const std::string &key, const adios2::Dims &start, const adios2::Dims &count, std::vector<uint64_t> &data) {
        adios2::Variable<uint64_t> id_meta_info = bpIO.InquireVariable<uint64_t>(key);
        // id_meta_info.SetSelection({start, count});
        bpReader.Get<uint64_t>(id_meta_info, data, adios2::Mode::Sync);
    }

    void BuildBase::writeIndex(){
        if (storageBackend == "file")
            writeIndexToFile();
        else if (storageBackend == "rocksdb")
            writeIndexToRocksDB();
    }

    void BuildBase::writeKVToBatch(const std::string &key, const std::string &value) {
        std::lock_guard<std::mutex> lock(rocksdbMutex);
        batch.Put(key, value);
    }

    void BuildBase::writeKVToRocksDB(const std::string &key, const std::string &value) {
        // Use std::unique_lock to explicitly lock and unlock the mutex
        std::lock_guard<std::mutex> lock(rocksdbMutex);
        rocksdb::Status status = rocksdb_pointer->Put(rocksdb::WriteOptions(), key, value);
        if (status.ok()) {
            std::cout << key << "\tWriting key to RocksDB successfully" << std::endl;
        }
    }
    
    // write batch to db
    void BuildBase::writeBatchToRocksDB(){
        // Use std::unique_lock to explicitly lock and unlock the mutex
        std::lock_guard<std::mutex> lock(rocksdbMutex);
        rocksdb::Status status = rocksdb_pointer->Write(rocksdb::WriteOptions(), &batch);
        if (status.ok()) {
            std::cout << "Writing batch to RocksDB successfully" << std::endl;
        }
        batch.Clear();
    }


} // namespace geosindex
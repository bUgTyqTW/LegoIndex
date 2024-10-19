#include <adios2.h>
#include <queue>
#include <chrono>

// struct for the batch read job
struct BatchReadJob {
    adios2::Dims Start;
    adios2::Dims Count;

    size_t blockNum;
};


int main(int argc, char const *argv[])
{
    
    adios2::ADIOS adios;
    adios2::IO bpIO;
    adios2::Engine bpReader;

    std::string bpFileName = "/data/gc/rocksdb-index/WarpX/build/bin/diags/diag2/openpmd.bp";
    std::string key = "/data/500/particles/electrons/";
    // std::string key = "/data/500/particles/electrons/momentum/";
    // Define the number of threads of adios read
    int nThreads = 16;

    // bpFileName can be passed as a command line argument by `-f "bpFileName"`
    // key can be passed as a command line argument by `-k "key"`
    // nThreads can be passed as a command line argument by `-n "nThreads"`
    if (argc > 1)
    {
        for (int i = 1; i < argc; ++i)
        {
            if (std::string(argv[i]) == "-f")
            {
                bpFileName = argv[i + 1];
                i++;
            }
            else if (std::string(argv[i]) == "-k")
            {
                key = argv[i + 1];
                i++;
            }
            else if (std::string(argv[i]) == "-n")
            {
                nThreads = std::stoi(argv[i + 1]);
                i++;
            }
        }
    }

    std::cout << "The bpFileName is " << bpFileName << std::endl;
    std::cout << "The key is " << key << std::endl;
    std::cout << "The number of threads is " << nThreads << std::endl;

    // read job queue
    std::queue<BatchReadJob> jobsQueue;

    bpIO = adios.DeclareIO("ReadBP");
    bpIO.SetParameter("Threads", std::to_string(nThreads));
    bpReader = bpIO.Open(bpFileName, adios2::Mode::Read);

    adios2::Variable<uint64_t> id_meta_info = bpIO.InquireVariable<uint64_t>(key + "id");
    std::vector<uint64_t> myUint64ID;
    bpReader.Get<uint64_t>(id_meta_info, myUint64ID, adios2::Mode::Sync);

    std::cout << "The size of the myUint64ID is " << myUint64ID.size() << std::endl;
    for (size_t i = 0; i < myUint64ID.size(); ++i)
    {
        std::cout << myUint64ID[i] << std::endl;
        if (i == 10)
        {
            break;
        }
    }

    /*
    adios2::Variable<double> x_meta_info = bpIO.InquireVariable<double>(key + "x");

//    x_meta_info.SetSelection({[5705711], [5710011]});
//    bpReader.Get<double>(x_meta_info, myDoubleX, adios2::Mode::Sync);

    auto xBlocksInfo = bpReader.AllStepsBlocksInfo(x_meta_info);
    auto x_it = xBlocksInfo.begin();

    int blockBatchSize = 100;
    for (; x_it != xBlocksInfo.end(); ++x_it) {
        const auto &var_vec1 = x_it->second;
        adios2::Dims blockBatchStart = var_vec1[0].Start;
        adios2::Dims blockBatchCount = var_vec1[0].Count;
        for (size_t i = 0; i < var_vec1.size(); ++i) {
            const auto &var_info1 = var_vec1[i];
            if ((i + 1) % blockBatchSize == 0 || i == var_vec1.size() - 1) {
                // assign count based on the first and last block, last block start + count - first block start
                for (size_t j = 0; j < var_info1.Count.size(); ++j) {
                    blockBatchCount[j] = var_info1.Start[j] + var_info1.Count[j] - blockBatchStart[j];
                }

                // push the job to the job queue
                BatchReadJob batchReadJob;
                batchReadJob.Start = blockBatchStart;
                batchReadJob.Count = blockBatchCount;
                batchReadJob.blockNum = i + 1;
                jobsQueue.push(batchReadJob);
            }
        }
    }
    

    std::cout << "The size of the jobsQueue is " << jobsQueue.size() << std::endl;
    // for each job in the job queue, read the data, and monitor read time
    while (!jobsQueue.empty()) {
        std::vector<double> myDoubleX;
        BatchReadJob batchReadJob = jobsQueue.front();
        jobsQueue.pop();
        std::cout << "The blockNum is " << batchReadJob.blockNum << ", The Count is " << batchReadJob.Count[0] << std::endl;
        auto start = std::chrono::high_resolution_clock::now();

        // read the data
        x_meta_info.SetSelection({batchReadJob.Start, batchReadJob.Count});
        bpReader.Get<double>(x_meta_info, myDoubleX, adios2::Mode::Sync);

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        double seconds = duration.count();
        std::cout << "The code execution took " << seconds << " seconds." << std::endl;
        myDoubleX.clear();
    }
    */
    return 0;
}



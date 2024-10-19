#include <adios2.h>
#include <vector>
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

    std::string bpFileName = "/data/gc/rocksdb-index/WarpX/build/bin/diags/diag2/openpmd.bp";
    std::string key = "/data/500/particles/electrons/momentum/";
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

    adios2::IO bpIO;
    adios2::Engine bpReader;

    bpIO = adios.DeclareIO("ReadBP" + std::to_string(nThreads));
    bpIO.SetParameter("Threads", std::to_string(nThreads));
    bpReader = bpIO.Open(bpFileName, adios2::Mode::Read);
    adios2::Variable<double> x_meta_info = bpIO.InquireVariable<double>(key + "x");
    auto xBlocksInfo = bpReader.AllStepsBlocksInfo(x_meta_info);
    auto x_it = xBlocksInfo.begin();

    std::vector<double> myDoubleX;
    for (; x_it != xBlocksInfo.end(); ++x_it) {
        std::cout << "Use threads " << nThreads << " to read block." << std::endl;
        const auto &var_vec1 = x_it->second;
        adios2::Dims blockBatchStart = var_vec1[0].Start;
        adios2::Dims blockBatchCount = var_vec1[0].Count;

        for (size_t dim = 0; dim < blockBatchCount.size(); ++dim) {
            blockBatchCount[dim] = var_vec1[var_vec1.size() - 1].Start[dim] + var_vec1[var_vec1.size() - 1].Count[dim] - var_vec1[0].Start[dim];
        }
        // print blockBatchStart and blockBatchCount
        std::cout << "blockBatchStart: " << blockBatchStart[0] << " " << blockBatchStart[1] << " " << blockBatchStart[2] << std::endl;
        std::cout << "blockBatchCount: " << blockBatchCount[0] << " " << blockBatchCount[1] << " " << blockBatchCount[2] << std::endl;

        auto start = std::chrono::high_resolution_clock::now();
        // read the data
        x_meta_info.SetSelection({blockBatchStart, blockBatchCount});
        bpReader.Get<double>(x_meta_info, myDoubleX, adios2::Mode::Sync);

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        double seconds = duration.count();
        std::cout << "The code execution took " << seconds << " seconds." << std::endl;
    }
    std::cout << "The size of myDoubleX is " << myDoubleX.size() << std::endl;

    myDoubleX.clear();
    bpReader.Close();

    return 0;
}



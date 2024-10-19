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

    bpIO = adios.DeclareIO("ReadBP");
    bpIO.SetParameter("Threads", std::to_string(64));
    bpReader = bpIO.Open(bpFileName, adios2::Mode::Read);

    adios2::Variable<double> x_meta_info = bpIO.InquireVariable<double>(key + "x");
    auto xBlocksInfo = bpReader.AllStepsBlocksInfo(x_meta_info);
    auto x_it = xBlocksInfo.begin();

    auto total_start = std::chrono::high_resolution_clock::now();
    for (; x_it != xBlocksInfo.end(); ++x_it) {
        const auto &var_vec1 = x_it->second;
        adios2::Dims blockBatchStart = var_vec1[0].Start;
        adios2::Dims blockBatchCount = var_vec1[0].Count;
        std::vector<double> myDoubleX;
        for (size_t i = 0; i < var_vec1.size(); ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            std::cout << "The blockSeq is " << i << ", The Start is " << var_vec1[i].Start[0] << ", The Count is " << var_vec1[i].Count[0] << std::endl;
            // read the data
            x_meta_info.SetSelection({var_vec1[i].Start, var_vec1[i].Count});
            bpReader.Get<double>(x_meta_info, myDoubleX, adios2::Mode::Sync);
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> duration = end - start;
            double seconds = duration.count();
            std::cout << "The code execution took " << seconds << " seconds." << std::endl;
            myDoubleX.clear();
        }
    }
    auto total_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = total_end - total_start;
    double seconds = duration.count();
    std::cout << "The total code execution took " << seconds << " seconds." << std::endl;

    return 0;
}



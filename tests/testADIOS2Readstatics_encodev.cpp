#include <adios2.h>
#include <queue>
#include <chrono>


int main(int argc, char const *argv[])
{
    
    adios2::ADIOS adios;
    adios2::IO bpIO;
    adios2::Engine bpReader;

    std::string bpFileName = "/data/gc/rocksdb-index/WarpX/build/bin/diags/diag1/openpmd.bp/";
//    std::string bpFileName = "/data/gc/rocksdb-index/WarpX/build/bin/diags/diag2/openpmd.bp";
    int iteration = 500;
    // Define the number of threads of adios read
    int nThreads = 16;

    // bpFileName can be passed as a command line argument by `-f "bpFileName"`
    // iteration can be passed as a command line argument by `-i "iteration"`
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
            else if (std::string(argv[i]) == "-i")
            {
                iteration = std::stoi(argv[i + 1]);
                i++;
            }
            else if (std::string(argv[i]) == "-n")
            {
                nThreads = std::stoi(argv[i + 1]);
                i++;
            }
            else if (std::string(argv[i]) == "-h" or std::string(argv[i]) == "--help")
            {
                std::cout << "Usage: " << argv[0] << " [-f bpFileName] [-i iteration] [-n nThreads]" << std::endl;
                return 0;
            }
        }
    }

    // use iteration to compose the key
    std::string particleKey = "/data/" + std::to_string(iteration) + "/particles/electrons/momentum/x";
    std::string fieldKey = "/data/" + std::to_string(iteration) + "/fields/E/x";

    std::cout << "The bpFileName is " << bpFileName << std::endl;
    std::cout << "The particleKey is " << particleKey << std::endl;
    std::cout << "The fieldKey is " << fieldKey << std::endl;
    std::cout << "The number of threads is " << nThreads << std::endl;

    bpIO = adios.DeclareIO("ReadBP");
    bpIO.SetParameter("Threads", std::to_string(nThreads));
    bpIO.SetEngine("BP4");
    bpReader = bpIO.Open(bpFileName, adios2::Mode::Read);

//    adios2::Variable<double> x_meta_info = bpIO.InquireVariable<double>(particleKey);
    adios2::Variable<double> x_meta_info = bpIO.InquireVariable<double>("/data/particles/electrons/position/y/__data__");
    auto xBlocksInfo = bpReader.AllStepsBlocksInfo(x_meta_info);
    auto x_it = xBlocksInfo.begin();

    int particle_number = 0;
    for (; x_it != xBlocksInfo.end(); ++x_it) {
        const auto &var_vec1 = x_it->second;
        adios2::Dims blockBatchStart = var_vec1[0].Start;
        adios2::Dims blockBatchCount = var_vec1[0].Count;
        for (size_t i = 0; i < var_vec1.size(); ++i) {
            particle_number += var_vec1[i].Count[0];
        }
        std::cout << "The particle blockNum is " << var_vec1.size() << ", The particle Count is " << particle_number << std::endl;
    }
    
//    x_meta_info = bpIO.InquireVariable<double>(fieldKey);
    x_meta_info = bpIO.InquireVariable<double>("/data/fields/B/y/__data__");
    xBlocksInfo = bpReader.AllStepsBlocksInfo(x_meta_info);
    x_it = xBlocksInfo.begin();
    for (; x_it != xBlocksInfo.end(); ++x_it) {
        const auto &var_vec1 = x_it->second;
        std::cout << "The field blockNum is " << var_vec1.size() << std::endl;
    }
}


//
// Created by cguo51 on 1/20/24.
//

#include <iostream>
#include <vector>

#include <adios2.h>


struct blockEnvelope
{
    double xMin;
    double xMax;
    double yMin;
    double yMax;
    double zMin;
    double zMax;
};


void cutBlock(const struct blockEnvelope &blockEnv, const size_t &nSubBlocksEach, std::vector<blockEnvelope> &subBlockEnvs)
{
    double xStep = (blockEnv.xMax - blockEnv.xMin) * 1.1 / nSubBlocksEach;
    double yStep = (blockEnv.yMax - blockEnv.yMin) * 1.1/ nSubBlocksEach;
    double zStep = (blockEnv.zMax - blockEnv.zMin) * 1.1/ nSubBlocksEach;

    for (size_t i = 0; i < nSubBlocksEach; i++)
    {
        for (size_t j = 0; j < nSubBlocksEach; j++)
        {
            for (size_t k = 0; k < nSubBlocksEach; k++)
            {
                blockEnvelope subBlockEnv;
                subBlockEnv.xMin = blockEnv.xMin + i * xStep;
                subBlockEnv.xMax = blockEnv.xMin + (i + 1) * xStep;
                subBlockEnv.yMin = blockEnv.yMin + j * yStep;
                subBlockEnv.yMax = blockEnv.yMin + (j + 1) * yStep;
                subBlockEnv.zMin = blockEnv.zMin + k * zStep;
                subBlockEnv.zMax = blockEnv.zMin + (k + 1) * zStep;
                subBlockEnvs.push_back(subBlockEnv);
            }
        }
    }
}

int main(int argc, char *argv[])
{
    std::string inputFileName;
    std::string variableName;
    std::string variableType;
    size_t nSubBlocksEach = 0;
    std::string outputFileName;
    for (int i = 0; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--input_file")
        {
            if (i+1 < argc)
            {
                inputFileName = argv[i+1];
            }
            else
            {
                std::cerr << "--input_file option requires one argument." << std::endl;
                return 1;
            }            
        }
        else if (arg == "--variable_name")
        {
            if (i+1 < argc)
            {
                variableName = argv[i+1];
            }
            else
            {
                std::cerr << "--variable_name option requires one argument." << std::endl;
                return 1;
            }             
        }
        else if (arg == "--n_sub_blocks_each")
        {
            if (i+1 < argc)
            {
                nSubBlocksEach = std::stoul(argv[i+1]);
            }
            else
            {
                std::cerr << "--n_sub_blocks_each option requires one argument." << std::endl;
                return 1;
            }             
        }
        else if (arg == "--output_file")
        {
            if (i+1 < argc)
            {
                outputFileName = argv[i+1];
            }
            else
            {
                std::cerr << "--output_file option requires one argument." << std::endl;
                return 1;
            }            
        }
    }

    adios2::ADIOS adios;
    adios2::IO reader_io = adios.DeclareIO("ReaderIO");
    adios2::Engine reader_engine = reader_io.Open(inputFileName, adios2::Mode::Read);

    adios2::IO writer_io = adios.DeclareIO("WriterIO");
    adios2::Engine writer_engine = writer_io.Open(outputFileName, adios2::Mode::Write); 

    const auto attributesMap = reader_io.AvailableAttributes();
    for (const auto &attributePair : attributesMap) {
        const std::string name = attributePair.first;
        const adios2::Params parameters = attributePair.second;
        const std::size_t elements = std::stoul(parameters.at("Elements"));
        const std::string type = parameters.at("Type");
        const std::string value = parameters.at("Value");

        if (type == "uint32_t")
        {
            writer_io.DefineAttribute<uint32_t>(name, std::stoul(value));
        }
        else if (type == "float")
        {
            writer_io.DefineAttribute<float>(name, std::stof(value));
        }
        else if (type == "double")
        {
            if (elements == 1)
            {
                writer_io.DefineAttribute<double>(name, std::stod(value));
            }
            else
            {
                std::vector<double> myDoubles;
                myDoubles.reserve(elements);
                for (size_t i = 0; i < value.size(); )
                {
                    // value =  "{ 0, 0, 1, 1, 0, 0, 0 }"
                    const size_t start = value.find_first_of("0123456789", i);
                    const size_t end = value.find_first_of(",} ", start);
                    const std::string number = value.substr(start, end - start);
                    myDoubles.push_back(std::stod(number));
                    i = end;
                    if (elements == myDoubles.size())
                    {
                        break;
                    }
                }
                writer_io.DefineAttribute<double>(name, myDoubles.data(), myDoubles.size());
            }
        }
        else if (type == "string")
        {
            // remove \" in value 
            const std::string valueStr = value.substr(1, value.size() - 2);
            writer_io.DefineAttribute<std::string>(name, valueStr);
        }
        else if (type == "uint64_t")
        {   
            auto original_value = std::stoull(value);
            writer_io.DefineAttribute<uint64_t>(name, original_value);
        }



    }

    size_t step = 0;
    size_t particleOffset = 0;
    while (reader_engine.BeginStep() == adios2::StepStatus::OK)
    {
        adios2::Variable<double> x_meta_info = reader_io.InquireVariable<double>(variableName + "/position/x");
        adios2::Variable<double> y_meta_info = reader_io.InquireVariable<double>(variableName + "/position/y");
        adios2::Variable<double> z_meta_info = reader_io.InquireVariable<double>(variableName + "/position/z");
        adios2::Variable<double> ux_meta_info = reader_io.InquireVariable<double>(variableName + "/momentum/x");
        adios2::Variable<double> uy_meta_info = reader_io.InquireVariable<double>(variableName + "/momentum/y");
        adios2::Variable<double> uz_meta_info = reader_io.InquireVariable<double>(variableName + "/momentum/z");

        auto xBlocksInfo = reader_engine.AllStepsBlocksInfo(x_meta_info);
        auto yBlocksInfo = reader_engine.AllStepsBlocksInfo(y_meta_info);
        auto zBlocksInfo = reader_engine.AllStepsBlocksInfo(z_meta_info);
        auto uxBlocksInfo = reader_engine.AllStepsBlocksInfo(ux_meta_info);
        auto uyBlocksInfo = reader_engine.AllStepsBlocksInfo(uy_meta_info);
        auto uzBlocksInfo = reader_engine.AllStepsBlocksInfo(uz_meta_info);

        auto x_it = xBlocksInfo.begin();
        auto y_it = yBlocksInfo.begin();
        auto z_it = zBlocksInfo.begin();
        auto ux_it = uxBlocksInfo.begin();
        auto uy_it = uyBlocksInfo.begin();
        auto uz_it = uzBlocksInfo.begin();

        size_t total_particles = 0;
        for (; x_it != xBlocksInfo.end(); ++x_it){
            const auto &var_vec1 = x_it->second;
            for (size_t i = 0; i < var_vec1.size(); ++i) {
                total_particles += var_vec1[i].Count[0];
            }
        }
        std::cout << "Total Particles is " << total_particles << std::endl;

        
        for (x_it = xBlocksInfo.begin(); x_it != xBlocksInfo.end(); ++x_it, ++y_it, ++z_it, ++ux_it, ++uy_it, ++uz_it) {
            const auto &var_vec1 = x_it->second;

            size_t total_blocks = var_vec1.size();
            for (size_t i = 0; i < 3; i++)
            {
                total_blocks *= nSubBlocksEach;
            }
            std::cout <<"Total Blocks is " << total_blocks << std::endl;

            std::vector<std::vector<double>> blocksData_x(total_blocks);
            std::vector<std::vector<double>> blocksData_y(total_blocks);
            std::vector<std::vector<double>> blocksData_z(total_blocks);
            std::vector<std::vector<double>> blocksData_ux(total_blocks);
            std::vector<std::vector<double>> blocksData_uy(total_blocks);
            std::vector<std::vector<double>> blocksData_uz(total_blocks);

            size_t blockOffset = 0;
            size_t blockStep = nSubBlocksEach * nSubBlocksEach * nSubBlocksEach;
            // cut for each block
            for (size_t i = 0; i < var_vec1.size(); ++i) {

                const auto &var_x = var_vec1[i];
                const auto &var_y = y_it->second[i];
                const auto &var_z = z_it->second[i];
                const auto &var_ux = ux_it->second[i];
                const auto &var_uy = uy_it->second[i];
                const auto &var_uz = uz_it->second[i];

                std::cout << "The blockSeq is " << i << ", The particle Count is " << var_x.Count[0] << std::endl;

                std::vector<double> data_x;
                std::vector<double> data_y;
                std::vector<double> data_z;
                std::vector<double> data_ux;
                std::vector<double> data_uy;
                std::vector<double> data_uz;

                data_x.resize(var_x.Count[0]);
                data_y.resize(var_y.Count[0]);
                data_z.resize(var_z.Count[0]);
                data_ux.resize(var_ux.Count[0]);
                data_uy.resize(var_uy.Count[0]);
                data_uz.resize(var_uz.Count[0]);

                x_meta_info.SetSelection({var_x.Start, var_x.Count});
                y_meta_info.SetSelection({var_y.Start, var_y.Count});
                z_meta_info.SetSelection({var_z.Start, var_z.Count});
                ux_meta_info.SetSelection({var_ux.Start, var_ux.Count});
                uy_meta_info.SetSelection({var_uy.Start, var_uy.Count});
                uz_meta_info.SetSelection({var_uz.Start, var_uz.Count});

                reader_engine.Get<double>(x_meta_info, data_x.data(), adios2::Mode::Sync);
                reader_engine.Get<double>(y_meta_info, data_y.data(), adios2::Mode::Sync);
                reader_engine.Get<double>(z_meta_info, data_z.data(), adios2::Mode::Sync);
                reader_engine.Get<double>(ux_meta_info, data_ux.data(), adios2::Mode::Sync);
                reader_engine.Get<double>(uy_meta_info, data_uy.data(), adios2::Mode::Sync);
                reader_engine.Get<double>(uz_meta_info, data_uz.data(), adios2::Mode::Sync);

                blockEnvelope blockEnv;
                // min data_x
                blockEnv.xMin = *std::min_element(data_x.begin(), data_x.end());
                blockEnv.xMax = *std::max_element(data_x.begin(), data_x.end());
                blockEnv.yMin = *std::min_element(data_y.begin(), data_y.end());
                blockEnv.yMax = *std::max_element(data_y.begin(), data_y.end());
                blockEnv.zMin = *std::min_element(data_z.begin(), data_z.end());
                blockEnv.zMax = *std::max_element(data_z.begin(), data_z.end());
                
                std::vector<blockEnvelope> subBlockEnvs;
                std::vector<std::vector<double>> blocks;
                // subBlockEnvs.resize(blockStep);
                cutBlock(blockEnv, nSubBlocksEach, subBlockEnvs);

                for (size_t j = 0; j < var_x.Count[0]; j++)
                {
                    // std::cout << "The particle position is (" << data_x[j] << ", " << data_y[j] << ", " << data_z[j] << "), The particle momentum is (" << data_ux[j] << ", " << data_uy[j] << ", " << data_uz[j] << ")" << std::endl;
                    for (size_t k = 0; k < blockStep; k++)
                    {
                        if (data_x[j] >= subBlockEnvs[k].xMin && data_x[j] < subBlockEnvs[k].xMax && data_y[j] >= subBlockEnvs[k].yMin && data_y[j] < subBlockEnvs[k].yMax && data_z[j] >= subBlockEnvs[k].zMin && data_z[j] < subBlockEnvs[k].zMax)
                        {
                            blocksData_x[k + blockOffset].push_back(data_x[j]);
                            blocksData_y[k + blockOffset].push_back(data_y[j]);
                            blocksData_z[k + blockOffset].push_back(data_z[j]);
                            blocksData_ux[k + blockOffset].push_back(data_ux[j]);
                            blocksData_uy[k + blockOffset].push_back(data_uy[j]);
                            blocksData_uz[k + blockOffset].push_back(data_uz[j]);
                            break;
                        }
                    }
                }

                adios2::Variable<double> outputVariable_x;
                adios2::Variable<double> outputVariable_y;
                adios2::Variable<double> outputVariable_z;
                adios2::Variable<double> outputVariable_ux;
                adios2::Variable<double> outputVariable_uy;
                adios2::Variable<double> outputVariable_uz;

                if (writer_io.InquireVariable<double>(variableName + "/position/x"))
                {
                    outputVariable_x = writer_io.InquireVariable<double>(variableName + "/position/x");
                    outputVariable_y = writer_io.InquireVariable<double>(variableName + "/position/y");
                    outputVariable_z = writer_io.InquireVariable<double>(variableName + "/position/z");
                    outputVariable_ux = writer_io.InquireVariable<double>(variableName + "/momentum/x");
                    outputVariable_uy = writer_io.InquireVariable<double>(variableName + "/momentum/y");
                    outputVariable_uz = writer_io.InquireVariable<double>(variableName + "/momentum/z");
                }
                else
                {
                    outputVariable_x = writer_io.DefineVariable<double>(variableName + "/position/x", {total_particles});
                    outputVariable_y = writer_io.DefineVariable<double>(variableName + "/position/y", {total_particles});
                    outputVariable_z = writer_io.DefineVariable<double>(variableName + "/position/z", {total_particles});
                    outputVariable_ux = writer_io.DefineVariable<double>(variableName + "/momentum/x", {total_particles});
                    outputVariable_uy = writer_io.DefineVariable<double>(variableName + "/momentum/y", {total_particles});
                    outputVariable_uz = writer_io.DefineVariable<double>(variableName + "/momentum/z", {total_particles});
                }

                for (size_t k = blockOffset; k < blockOffset + blockStep; k++)
                {
                    if (blocksData_x[k].size() == 0)
                    {
                        continue;
                    }

                    outputVariable_x.SetSelection({{particleOffset}, {blocksData_x[k].size()}});
                    outputVariable_y.SetSelection({{particleOffset}, {blocksData_y[k].size()}});
                    outputVariable_z.SetSelection({{particleOffset}, {blocksData_z[k].size()}});
                    outputVariable_ux.SetSelection({{particleOffset}, {blocksData_ux[k].size()}});
                    outputVariable_uy.SetSelection({{particleOffset}, {blocksData_uy[k].size()}});
                    outputVariable_uz.SetSelection({{particleOffset}, {blocksData_uz[k].size()}});

                    writer_engine.Put<double>(outputVariable_x, blocksData_x[k].data(), adios2::Mode::Sync);
                    writer_engine.Put<double>(outputVariable_y, blocksData_y[k].data(), adios2::Mode::Sync);
                    writer_engine.Put<double>(outputVariable_z, blocksData_z[k].data(), adios2::Mode::Sync);
                    writer_engine.Put<double>(outputVariable_ux, blocksData_ux[k].data(), adios2::Mode::Sync);
                    writer_engine.Put<double>(outputVariable_uy, blocksData_uy[k].data(), adios2::Mode::Sync);
                    writer_engine.Put<double>(outputVariable_uz, blocksData_uz[k].data(), adios2::Mode::Sync);

                    std::cout << "The block " << k << " has " << blocksData_x[k].size() << " particles." << std::endl;

                    particleOffset += blocksData_x[k].size();
                    // std::cout << "The particle in block k is " << blocksData_x[k].data() << std::endl;
                }

                blockOffset += blockStep;
                // break;
            }
        }

        writer_engine.EndStep();
        reader_engine.EndStep();
        step++;
    }
    writer_engine.Close();
    reader_engine.Close();

    return 0;
}

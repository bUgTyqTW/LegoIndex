#include <iostream>
#include <vector>

#include <adios2.h>

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

    const auto attributesMap = reader_io.AvailableAttributes();

    adios2::IO writer_io = adios.DeclareIO("WriterIO");
    adios2::Engine writer_engine = writer_io.Open(outputFileName, adios2::Mode::Write);

    std::vector<float> myFloats = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    const std::size_t Nx = myFloats.size();

    /** global array : name, { shape (total) }, { start (local) }, { count
     * (local) }, all are constant dimensions */
    adios2::Variable<float> bpFloats = writer_io.DefineVariable<float>(
        "bpFloats", {1 * Nx}, {0 * Nx}, {Nx}, adios2::ConstantDims);

    // writer
    for (const auto &attributePair : attributesMap)
    {
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
            writer_io.DefineAttribute<uint64_t>(name, std::stoull(value));
        }


    }

    writer_engine.BeginStep();

    /** Write variable for buffering */
    writer_engine.Put<float>(bpFloats, myFloats.data());

    writer_engine.EndStep();


    // Close the ADIOS2 engine
    reader_engine.Close();
    writer_engine.Close();
}
#include <iostream>
#include <vector>
#include <unistd.h>
#include <cstdlib>

#include <adios2.h>

std::vector<size_t> positionToIndices(size_t position, const std::vector<size_t>& shape) 
{
    std::vector<size_t> indices;
    size_t remainingPosition = position;

    for (auto dimensionSize = shape.rbegin(); dimensionSize != shape.rend(); ++dimensionSize) 
    {
        indices.insert(indices.begin(), remainingPosition % *dimensionSize);
        remainingPosition /= *dimensionSize;
    }

    return indices;
}

size_t indicesToPosition(const std::vector<size_t>& shape, const std::vector<size_t>& indices) {
    size_t position = indices[0];
    size_t multiplier = 1;

    for (size_t i = 1; i < shape.size(); ++i) {
        multiplier *= shape[i - 1];
        position += indices[i] * multiplier;
    }

    return position;
}

int main(int argc, char *argv[])
{
    std::string inputFileName;
    std::string variableName;
    std::string variableType;
    size_t nDim = 0;
    std::vector<size_t> blockShape;
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
        else if (arg == "--dimensions")
        {
            if (i+1 < argc)
            {
                nDim = atoi(argv[i+1]);
            }
            else
            {
                std::cerr << "--dimensions option requires one argument." << std::endl;
                return 1;
            } 
        }
        else if (arg == "--block_shape")
        {
            if (nDim)
            {
                if ((int)(i+nDim) < argc)
                {
                    for (size_t j = i+1; j < i+1+nDim; j++)
                    {
                        blockShape.push_back(atoi(argv[j]));
                    }
                    
                }
                else
                {
                    std::cerr << "--blockShape option requires [# of dimensions] argument." << std::endl;
                    return 1;
                }  
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

    size_t step = 0;
    while (reader_engine.BeginStep() == adios2::StepStatus::OK)
    {
        auto var = reader_io.InquireVariable<double>(variableName);
        variableType= reader_io.VariableType(variableName);
        size_t varElements = 1;
        std::cout << "step: " << step << ", variable name: " << variableName << ", type: " << variableType << ", shape: ";
        for (size_t i = 0; i < nDim; i++)
        {
            std::cout << var.Shape()[i] << " ";
            varElements *= var.Shape()[i];
        }
        std::cout << std::endl;
        std::vector<size_t> blockCountOnEachDim;
        size_t total_blocks = 1;
        for (size_t i = 0; i < nDim; i++)
        {
            blockCountOnEachDim.push_back(var.Shape()[i]/blockShape[i]);
            total_blocks *= var.Shape()[i]/blockShape[i];
        }
        writer_engine.BeginStep();
        if (variableType == "double")
        {
            std::vector<double> varData(varElements);
            reader_engine.Get(var, varData.data(), adios2::Mode::Sync);
            std::vector<std::vector<double>> blocks(total_blocks);
            for (size_t p = 0; p < varElements; p++)
            {
                std::vector<size_t> elem_global_id = positionToIndices(p, var.Shape());
                std::vector<size_t> block_global_id(nDim);
                for (size_t i = 0; i < nDim; i++)
                {
                    block_global_id[i] = elem_global_id[i]/blockShape[i];
                }                
                size_t block_position = indicesToPosition(blockCountOnEachDim, block_global_id);
                blocks[block_position].push_back(varData[p]);
            }
            adios2::Variable<double> outputVariable;
            if (writer_io.InquireVariable<double>(variableName))
            {
                outputVariable = writer_io.InquireVariable<double>(variableName);
            }
            else
            {
                outputVariable = writer_io.DefineVariable<double>(variableName, var.Shape());
            }

            for (size_t b = 0; b < total_blocks; b++)
            {
                std::vector<size_t> block_idx_on_each_dim = positionToIndices(b, blockCountOnEachDim);
                std::vector<size_t> blockStart(nDim);
                std::vector<size_t> blockCount(nDim);
                std::cout << "block " << b << " start: ";
                for (size_t i = 0; i < nDim; i++)
                {
                    blockStart[i] = block_idx_on_each_dim[i]*blockShape[i];
                    std::cout << blockStart[i] << " ";
                }
                std::cout << std::endl;
                std::cout << "block " << b << " count: ";
                for (size_t i = 0; i < nDim; i++)
                {
                    if (block_idx_on_each_dim[i] == blockCountOnEachDim[i]-1)
                    {
                        blockCount[i] = var.Shape()[i]-block_idx_on_each_dim[i]*blockShape[i];
                    }
                    else
                    {
                        blockCount[i] = blockShape[i];
                    }
                    std::cout << blockCount[i] << " ";
                }
                std::cout << std::endl;
                outputVariable.SetSelection({blockStart, blockCount});
                writer_engine.Put(outputVariable, blocks[b].data(), adios2::Mode::Sync);
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

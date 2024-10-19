#ifndef GEOSINDEX_MINMAXBUILD_H
#define GEOSINDEX_MINMAXBUILD_H

#include <map>
#include <utility>

#include "minMaxNode.pb.h"

#include <geosindex/buildbase.h>

namespace geosindex {

  class MinMaxBuild: public BuildBase{
    private:
        std::map<std::string, MinMaxList> minMaxListMap;

        std::mutex minMaxListMapMutex;

        /* --- Secondary Index --- */
        std::map<std::string, MinMaxList> minMaxListMap_inblock;

        std::mutex minMaxListMapMutex_inblock;

    public:
        MinMaxBuild(std::string bpFileName, std::vector<std::string> particleCharacters = {"position", "momentum"}, 
                    std::string species = "electrons", int maxThreads = 16, int nThreads = 16, int iteration = 500,
                    int blockBatchSize = 10000, std::string storageBackend = "file", std::string indexSavePath = "minmaxindex", 
                    std::string secondaryIndexType = "none", int inblockSliceSize = 1000) :
                    BuildBase(bpFileName, particleCharacters, species, maxThreads, nThreads, iteration, blockBatchSize,
                    storageBackend, indexSavePath + "_minmax", secondaryIndexType, inblockSliceSize) {
        }

        ~MinMaxBuild() {
            std::cout << "free MinMaxBuild" << std::endl;
        }
        
        void processMinMaxNode(const std::vector<double> &data, const std::string &key, const size_t &start, const size_t &end);

        void processBlockData(BlockData blockData) override;

        void writeIndexToFile_minmax(const std::map<std::string, MinMaxList> &minMaxListMap, std::fstream &diskFile, int &diskFileCursor);

        void writeIndexToFile() override;

        void writeIndexToRocksDB() override;
  };  

};

#endif //GEOSINDEX_MINMAXBUILD_H


//
// Created by chang on 5/31/23.
//

#ifndef GEOSINDEX_RTREEBUILD_H
#define GEOSINDEX_RTREEBUILD_H

#include <geosindex/buildbase.h>
#include <geosindex/utils.h>
#include <geosindex/bloomfilter.h>

#include <map>
#include <vector>
#include <utility>

// #include <geosindex/bloomfilter.h>



namespace geosindex {

    class RTreeBuild: public BuildBase{
    private:
        int STRtreeLeafSize = 10;

        std::map<std::string, std::vector<geos::geom::Envelope3d>> blockTreeMap;

        std::mutex blockTreeMapMutex;

        /* --- bloom filter for tracing --- */
        bool build_bloom_filter = false;

        std::string id_key = "/data/";

        std::vector<uint64_t> id_data;

        size_t max_level = 0;

    public:
        RTreeBuild(std::string bpFileName, std::vector<std::string> particleCharacters = {"position", "momentum"}, 
                    std::string species = "electrons", int maxThreads = 16, int nThreads = 16, int iteration = 500,
                    int blockBatchSize = 10000, std::string storageBackend = "file", std::string indexSavePath = "rtreeindex", 
                    std::string secondaryIndexType = "none", bool build_bloom_filter = false,
                    int inblockSliceSize = 1000, int STRtreeLeafSize = 10) :
                    BuildBase(bpFileName, particleCharacters, species, maxThreads, nThreads, iteration, blockBatchSize,
                    storageBackend, indexSavePath + "_rtree", secondaryIndexType, inblockSliceSize) {
                        
            this->STRtreeLeafSize = STRtreeLeafSize;
            this->build_bloom_filter = build_bloom_filter;
            this->id_key = "/data/" + std::to_string(iteration) + "/particles/" + species + "/" + "id";
        }

        void processBlockData(BlockData blockData) override;

        void writeIndexToFile() override;

        void writeIndexToRocksDB() override;

        void initBloomFilter(geos::index::strtree::SimpleSTRnode3d *node_ptr, std::vector<uint64_t> &id_data_sub);

    };
}
#endif //GEOSINDEX_RTREEBUILD_H
#ifndef GEOSINDEX_MINMAXQUERY_H
#define GEOSINDEX_MINMAXQUERY_H

#include <geosindex/querybase.h>

#include "minMaxNode.pb.h"

#include <fstream>

namespace geosindex {

    class MinMaxQuery : public QueryBase{
    private:

    public:
        MinMaxQuery(std::string indexSavePath, std::string storageBackend, std::string secondaryIndexType = "none") : 
            QueryBase(indexSavePath + "_minmax", storageBackend, secondaryIndexType) {
        }

        ~MinMaxQuery() {
            diskFile.close();
        }

        std::map<std::string, QueryBlockResult> queryMinMaxData(std::string key, double min=-DoubleInfinity, double max=DoubleInfinity);
    };

};


#endif //GEOSINDEX_MINMAXQUERY_H
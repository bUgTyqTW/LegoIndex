//
// Created by chang on 5/31/23.
//

#ifndef GEOSINDEX_QUERY_H
#define GEOSINDEX_QUERY_H

#include <geosindex/querybase.h>
#include <geosindex/utils.h>
#include <geosindex/bloomfilter.h>

#include <vector>

namespace geosindex {
    
    class RTreeQuery: public QueryBase{
    private:

    public:
        RTreeQuery(std::string indexSavePath, std::string storageBackend, std::string secondaryIndexType = "none") : QueryBase(indexSavePath + "_rtree", storageBackend, secondaryIndexType) {

        }

        void matchEnv(geos::index::strtree::SimpleSTRnode3d *node_ptr, geos::geom::Envelope3d *e, std::vector<geos::geom::Envelope3d *> &envolope_list);

        void matchId(geos::index::strtree::SimpleSTRnode3d *node_ptr, const uint64_t targetId, std::map<std::string, TracingResult> &tracingResultMap);

        void queryByEnvelope(std::string key, geos::geom::Envelope3d *e, std::vector<geos::geom::Envelope3d *> &envolope_list);

        std::map<std::string, QueryBlockResult> queryRTreeXYZ(std::string key, double minx=-DoubleInfinity, double maxx=DoubleInfinity, double miny=-DoubleInfinity, double maxy=DoubleInfinity, double minz=-DoubleInfinity, double maxz=DoubleInfinity);

        std::vector<geos::geom::Envelope3d> queryRTreeMetaData(std::string key);

        geos::geom::Envelope3d queryRTreeMetaDataRoot(std::string key);

        std::map<std::string, TracingResult> queryRTreeTracing(std::string key, std::vector<uint64_t> tracingID);

        std::map<std::string, TracingResult> queryRTreeTracingInteracted(std::string key, std::vector<uint64_t> tracingID);

    };
}

#endif //GEOSINDEX_QUERY_H
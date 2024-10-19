#include <geosindex/minmaxquery.h>

namespace geosindex{

    std::map<std::string, QueryBlockResult> MinMaxQuery::queryMinMaxData(std::string key, double min, double max) {
        MinMaxList minMaxList;
        readNodeList(key, diskFile, minMaxList);

        std::map<std::string, QueryBlockResult> queryBlockResultMap;
        for (auto &minMaxData : minMaxList.minmaxnodes()) {
            // if minMaxData an min, max are interacted
            if (minMaxData.min() <= max && minMaxData.max() >= min) {
                if (secondaryIndexType == "none") {
                    queryBlockResultMap.insert(std::pair<std::string, QueryBlockResult>(std::to_string(minMaxData.start()), 
                    QueryBlockResult(minMaxData.start(), minMaxData.end())));
                } else {
                    MinMaxList minMaxListSecondary;
                    readNodeList(key + std::to_string(minMaxData.start()), diskFileSecondary, minMaxListSecondary);

                    std::map<std::string, QueryResult> queryResultList;
                    for (auto &minMaxDataSecondary : minMaxListSecondary.minmaxnodes()) {
                        if (minMaxDataSecondary.min() <= max && minMaxDataSecondary.max() >= min) {
                            queryResultList.insert(std::pair<std::string, QueryResult>(std::to_string(minMaxDataSecondary.start()), 
                            QueryResult(minMaxDataSecondary.start(), minMaxDataSecondary.end())));
                        }
                    }
                    queryBlockResultMap.insert(std::pair<std::string, QueryBlockResult>(std::to_string(minMaxData.start()), 
                    QueryBlockResult(minMaxData.start(), minMaxData.end(), queryResultList)));
                }
            }
        }
        return queryBlockResultMap;
    }

}; // namespace geosindex
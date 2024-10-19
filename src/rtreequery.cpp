//
// Created by chang on 5/31/23.
//

#include <geosindex/rtreequery.h>

namespace geosindex {

    void RTreeQuery::matchEnv(geos::index::strtree::SimpleSTRnode3d *node_ptr, geos::geom::Envelope3d *e, std::vector<geos::geom::Envelope3d *> &envolope_list) {
        auto* envelopePtr = static_cast<geos::geom::Envelope3d*>(const_cast<void*>(node_ptr->getBounds()));
        if (envelopePtr->intersects(e)) {
            if (node_ptr->isLeaf()) {
                envolope_list.push_back(envelopePtr);
            } else {
                for (const auto &child : node_ptr->getChildNodes()) {
                    matchEnv(child, e, envolope_list);
                }
            }
        }
    }

    void RTreeQuery::matchId(geos::index::strtree::SimpleSTRnode3d *node_ptr, const uint64_t targetId, std::map<std::string, TracingResult> &tracingResultMap) {
        for (const auto &child : node_ptr->getChildNodes()) {
            auto* childPtr = static_cast<geos::geom::Envelope3d*>(const_cast<void*>(child->getBounds()));
            BloomFilter bf_ins(10);
            std::string bloom_filter = childPtr->getBloomFilter();
            if (bf_ins.KeyMayMatch(std::to_string(targetId), bloom_filter)) {
                if (child->isLeaf()) {
                    size_t id_data_start = childPtr->getStart();
                    // if id_data_start is in tracingResultMap
                    if (tracingResultMap.find(std::to_string(id_data_start)) != tracingResultMap.end()) {
                        tracingResultMap[std::to_string(id_data_start)].id_data.push_back(targetId);
                    } else {
                        TracingResult tracingResult;
                        tracingResult.start = childPtr->getStart();
                        tracingResult.end = childPtr->getEnd();
                        tracingResult.id_data.push_back(targetId);
                        tracingResultMap.insert(std::pair<std::string, TracingResult>(std::to_string(tracingResult.start), tracingResult));
                    }
                } else {
                    matchId(child, targetId, tracingResultMap);
                }
            }
        }
    }


    void RTreeQuery::queryByEnvelope(std::string key, geos::geom::Envelope3d *e, std::vector<geos::geom::Envelope3d *> &envolope_list) {
        TreeNodeList treeNodeList;
        readNodeList(key, diskFile, treeNodeList); // todo bug here for secondary file

        // deserialize the first level tree
        geos::index::strtree::SimpleSTRnode3d *root_node = nullptr;
        int seq = 0;
        deSerialize(root_node, &treeNodeList, seq);
        std::cout << "deSerialize successfully" << std::endl;

        // if the root node is null, return
        if (root_node == nullptr) {
            std::cout << "root node is null" << std::endl;
            return;
        }

        // match the envolope with the tree
        matchEnv(root_node, e, envolope_list);
    }

    std::map<std::string, QueryBlockResult> RTreeQuery::queryRTreeXYZ(std::string key, double minx, double maxx, double miny, double maxy, double minz, double maxz){
        std::map<std::string, QueryBlockResult> queryBlockResultMap;
        auto *e = new geos::geom::Envelope3d(minx, maxx, miny, maxy, minz, maxz);
        std::vector<geos::geom::Envelope3d *> envelope_list;
        // query first level tree
        queryByEnvelope(key, e, envelope_list);
        std::cout << "first level match num: " << envelope_list.size() << std::endl;
        // match the envolope with the tree
        for (auto &envelope : envelope_list) {
            QueryBlockResult blockResult;
            blockResult.start = envelope->getStart();
            blockResult.end = envelope->getEnd();

            if (secondaryIndexType != "none") {
                std::vector<geos::geom::Envelope3d *> envelope_list_second;
                std::string secondLevelKey = key + std::to_string(envelope->getStart());
                queryByEnvelope(secondLevelKey, e, envelope_list_second);

                std::map<std::string, QueryResult> queryResultList;
                for (auto &envelope_second : envelope_list_second) {
                    QueryResult queryResult;
                    queryResult.start = envelope_second->getStart();
                    queryResult.end = envelope_second->getEnd();
                    queryResultList.insert(std::pair<std::string, QueryResult>(std::to_string(queryResult.start), queryResult));
                }

                blockResult.q = queryResultList;
            }
            
            queryBlockResultMap.insert(std::pair<std::string, QueryBlockResult>(std::to_string(blockResult.start), blockResult));
        }
        return queryBlockResultMap;
    }


    std::vector<geos::geom::Envelope3d> RTreeQuery::queryRTreeMetaData(std::string key) {
        // initial with max and min value for the envelope
        geos::geom::Envelope3d e(DoubleInfinity, -DoubleInfinity, DoubleInfinity, -DoubleInfinity, DoubleInfinity, -DoubleInfinity);

        std::vector<geos::geom::Envelope3d *> envolope_list;
        queryByEnvelope(key, &e, envolope_list);

        std::vector<geos::geom::Envelope3d> result;
        for (auto &envelope : envolope_list) {
            result.push_back(*envelope);
        }
        return result;
    }

    geos::geom::Envelope3d RTreeQuery::queryRTreeMetaDataRoot(std::string key) {
        TreeNodeList treeNodeList;
        readNodeList(key, diskFile, treeNodeList); // todo bug here for secondary file

        // deserialize the first level tree
        geos::index::strtree::SimpleSTRnode3d *root_node = nullptr;
        int seq = 0;
        deSerialize(root_node, &treeNodeList, seq);
        std::cout << "deSerialize successfully" << std::endl;

        return *static_cast<geos::geom::Envelope3d*>(const_cast<void*>(root_node->getBounds()));
    }

    std::map<std::string, TracingResult> RTreeQuery::queryRTreeTracing(std::string key, std::vector<uint64_t> tracingID) {
        std::map<std::string, TracingResult> tracingResultMap;
        TreeNodeList treeNodeList;
        readNodeList(key, diskFile, treeNodeList); // todo bug here for secondary file

        // deserialize the first level tree
        geos::index::strtree::SimpleSTRnode3d *root_node = nullptr;
        int seq = 0;
        deSerialize(root_node, &treeNodeList, seq);
        std::cout << "deSerialize successfully" << std::endl;

        // if the root node is null, return
        if (root_node == nullptr) {
            std::cout << "root node is null" << std::endl;
            return tracingResultMap;
        }

        // match the envolope with the tree
        for (const auto &id : tracingID) {
            matchId(root_node, id, tracingResultMap);
        }
        return tracingResultMap;
    }

    std::map<std::string, TracingResult> RTreeQuery::queryRTreeTracingInteracted(std::string key, std::vector<uint64_t> tracingID) {
        std::map<std::string, TracingResult> tracingResultMap_position = queryRTreeTracing(key, tracingID);
        
        // use key to replace position with momentum
        std::string momentum_key = key;
        momentum_key.replace(momentum_key.find("position"), 8, "momentum");
        std::cout << "momentum_key: " << momentum_key << std::endl;
        std::map<std::string, TracingResult> tracingResultMap_final;

        if (momentum_key != key) {
            std::map<std::string, TracingResult> tracingResultMap_momentum = queryRTreeTracing(momentum_key, tracingID);

            // interact the two map
            for (const auto &tracingResult : tracingResultMap_position) {
                if (tracingResultMap_momentum.count(tracingResult.first) > 0) {
                    TracingResult tracingResult_final;
                    tracingResult_final.start = tracingResult.second.start;
                    tracingResult_final.end = tracingResult.second.end;

                    std::set_intersection(tracingResult.second.id_data.begin(), tracingResult.second.id_data.end(),
                                          tracingResultMap_momentum[tracingResult.first].id_data.begin(), tracingResultMap_momentum[tracingResult.first].id_data.end(),
                                          std::back_inserter(tracingResult_final.id_data));

                    tracingResultMap_final.insert(std::pair<std::string, TracingResult>(tracingResult.first, tracingResult_final));
                }
            }
        } else {
            tracingResultMap_final = tracingResultMap_position;
        }
        return tracingResultMap_final;
    }
}

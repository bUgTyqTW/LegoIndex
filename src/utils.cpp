//
// Created by chang on 5/30/23.
//

#include <geosindex/utils.h>

namespace geosindex {
    void serialize(geos::index::strtree::SimpleSTRnode3d *node_ptr, TreeNodeList &serializeList){
        auto *bounds = (geos::geom::Envelope3d *) node_ptr->getBounds();
        std::size_t nChildren = node_ptr->getChildNodes().size();
        TreeNode signleNode;
        signleNode.set_level((int) node_ptr->getLevel());
        signleNode.set_childsize((int) node_ptr->getChildNodes().size());
        signleNode.set_minx(bounds->getMinX());
        signleNode.set_maxx(bounds->getMaxX());
        signleNode.set_miny(bounds->getMinY());
        signleNode.set_maxy(bounds->getMaxY());
        signleNode.set_minz(bounds->getMinZ());
        signleNode.set_maxz(bounds->getMaxZ());
        signleNode.set_start(bounds->getStart());
        signleNode.set_end(bounds->getEnd());
        signleNode.set_bloom_filter(bounds->getBloomFilter());

        serializeList.add_treenodes()->CopyFrom(signleNode);
        for (std::size_t j = 0; j < nChildren; j++) {
            serialize(node_ptr->getChildNodes()[j], serializeList);
        }
    }

    void deSerialize(geos::index::strtree::SimpleSTRnode3d *&node_ptr, TreeNodeList *serializeList, int &seq){
        const TreeNode &node = (*serializeList).treenodes(seq);
        geos::geom::Envelope3d e(node.minx(), node.maxx(), node.miny(), node.maxy(), node.minz(), node.maxz(), node.start(), node.end(), node.bloom_filter());
        node_ptr = new geos::index::strtree::SimpleSTRnode3d(static_cast<size_t>(node.level()), &e,
                                                             static_cast<void *>(&e));
        if (node.childsize() != 0) {
            for (int j = 0; j < node.childsize(); j++) {
                geos::index::strtree::SimpleSTRnode3d *child = nullptr;
                seq = seq + 1;
                deSerialize(child, serializeList, seq);
                node_ptr->addChildNode3d(child);
            }
        }
    }

}
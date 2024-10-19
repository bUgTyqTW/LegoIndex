//
// Created by chang on 5/30/23.
//

#ifndef GEOSINDEX_SERIALIZE_H
#define GEOSINDEX_SERIALIZE_H

#include <geos/index/strtree/SimpleSTRtree.h>

#include "treeNode.pb.h"

namespace geosindex {

    void serialize(geos::index::strtree::SimpleSTRnode3d *node_ptr, TreeNodeList &serializeList);

    void deSerialize(geos::index::strtree::SimpleSTRnode3d *&node_ptr, TreeNodeList *serializeList, int &seq);

}

#endif //GEOSINDEX_SERIALIZE_H
//
// Created by chang on 5/31/23.
//
#include <geosindex/rtreebuild.h>
#include <set>

namespace geosindex {

    void RTreeBuild::processBlockData(BlockData blockData) {
        geos::geom::Envelope3d envelope3d;
        if (secondaryIndexType == "none") {
            double minx = *std::min_element(blockData.blockXData.begin(), blockData.blockXData.end());
            double maxx = *std::max_element(blockData.blockXData.begin(), blockData.blockXData.end());
            double miny = *std::min_element(blockData.blockYData.begin(), blockData.blockYData.end());
            double maxy = *std::max_element(blockData.blockYData.begin(), blockData.blockYData.end());
            double minz = *std::min_element(blockData.blockZData.begin(), blockData.blockZData.end());
            double maxz = *std::max_element(blockData.blockZData.begin(), blockData.blockZData.end());
            envelope3d.init(minx, maxx, miny, maxy, minz, maxz, blockData.blockStart, blockData.blockStart + blockData.blockCount, "");

            std::lock_guard<std::mutex> lock(blockTreeMapMutex);
            blockTreeMap[blockData.key].push_back(envelope3d);

        } else if (secondaryIndexType == "minmax") {

        } else if (secondaryIndexType == "rtree") {

        }

    }

    void RTreeBuild::writeIndexToFile() {
        MetaDataListForFile metaDataListForFile;
        // for key in blockTreeMap
        for (const auto &blockTreePair : blockTreeMap) {
            geos::index::strtree::SimpleSTRtree simpleSTRtree = geos::index::strtree::SimpleSTRtree(STRtreeLeafSize);
            for (const auto &envelope3d : blockTreePair.second) {
                const auto ep = &envelope3d;
                simpleSTRtree.insert(ep, nullptr);
            }
            simpleSTRtree.getRoot3d();
            if (build_bloom_filter) {
                auto *bounds = (geos::geom::Envelope3d *) (simpleSTRtree.root3d)->getBounds();
                max_level = simpleSTRtree.root3d->getLevel();
                size_t id_data_start = bounds->getStart();
                size_t id_data_end = bounds->getEnd();
                this->readIDData(id_key, {id_data_start}, {id_data_end}, id_data);
                std::vector<uint64_t> id_data_sub;
                initBloomFilter(simpleSTRtree.root3d, id_data_sub);
            }

            TreeNodeList treeNodeList;
            serialize(simpleSTRtree.root3d, treeNodeList);
            std::string serializeListStr;
            treeNodeList.SerializeToString(&serializeListStr);
            diskFile.write(serializeListStr.c_str(), serializeListStr.length());


            MetaDataNodeForFile metaDataNodeForFile;
            metaDataNodeForFile.set_startbytes(diskFileCursor);
            metaDataNodeForFile.set_length(serializeListStr.length());
            metaDataNodeForFile.set_keyname(blockTreePair.first);
            metaDataListForFile.add_metadatanodeforfile()->CopyFrom(metaDataNodeForFile);

            diskFileCursor += serializeListStr.length();
        }

        std::string serializeMetaDataListStr;
        metaDataListForFile.SerializeToString(&serializeMetaDataListStr);

        std::size_t metadataLength = serializeMetaDataListStr.length();
        diskFile.write(serializeMetaDataListStr.c_str(), metadataLength);
        diskFileCursor += metadataLength;

        diskFile.write(reinterpret_cast<const char *>(&metadataLength), sizeof(metadataLength));
        std::cout << "metadata length: " << metadataLength << std::endl;
    
    }

    void RTreeBuild::writeIndexToRocksDB() {
        for (const auto &blockTreePair : blockTreeMap) {
            geos::index::strtree::SimpleSTRtree simpleSTRtree = geos::index::strtree::SimpleSTRtree(STRtreeLeafSize);
            for (const auto &envelope3d : blockTreePair.second) {
                const auto ep = &envelope3d;
                simpleSTRtree.insert(ep, nullptr);
            }
            simpleSTRtree.getRoot3d();
            TreeNodeList treeNodeList;
            serialize(simpleSTRtree.root3d, treeNodeList);
            std::string serializeListStr;
            treeNodeList.SerializeToString(&serializeListStr);
            if (rocksdb_write_batch_flag){
                writeKVToBatch(blockTreePair.first, serializeListStr);
            } else {
                writeKVToRocksDB(blockTreePair.first, serializeListStr);
            }
        }
        if (rocksdb_write_batch_flag){
            writeBatchToRocksDB();
        }
    }

    void RTreeBuild::initBloomFilter(geos::index::strtree::SimpleSTRnode3d *node_ptr, std::vector<uint64_t> &id_data_sub) {
        std::size_t nChildren = node_ptr->getChildNodes().size();
        for (std::size_t j = 0; j < nChildren; j++) {
            std::vector<uint64_t> id_data_sub_level;
            initBloomFilter(node_ptr->getChildNodes()[j], id_data_sub_level);
            id_data_sub.insert(id_data_sub.end(), id_data_sub_level.begin(), id_data_sub_level.end());
        }

        size_t level = node_ptr->getLevel();
        auto *bounds = (geos::geom::Envelope3d *) node_ptr->getBounds();
        size_t id_data_start = bounds->getStart();
        size_t id_data_end = bounds->getEnd();
        if (level == 0) {
            // id_data_sub = id_data[id_data_start, id_data_end]
            id_data_sub.insert(id_data_sub.end(), id_data.begin() + id_data_start, id_data.begin() + id_data_end);

        } else if (level == max_level) {
            return;
        }

        BloomFilter bloomFilter(max_level - (int) node_ptr->getLevel() + 1, max_bf_size);

        std::string bf;
        bloomFilter.CreateFilter(id_data_sub, &bf);
        bounds->setBloomFilter(bf);
        
        // CreateFilter use subvector of id_data, use the id_data_start and id_data_end
        // std::cout << "id_data.size(): " << id_data.size() << std::endl;
        
        std::cout << "tree level: " << node_ptr->getLevel() << " start:" << id_data_start << " end:" << id_data_end << std::endl;
        // set of id_data_sub
        std::set<uint64_t> id_data_sub_set(id_data_sub.begin(), id_data_sub.end());
        std::cout << "id_data_sub_set.size(): " << id_data_sub_set.size() << std::endl;
    }

}
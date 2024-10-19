//
// Created by chang on 5/28/23.
//

#ifndef GEOSINDEX_BLOOMFILTER_H
#define GEOSINDEX_BLOOMFILTER_H

#include <string>
#include <vector>

namespace geosindex {

    class BloomFilter {
    public:
        BloomFilter(size_t bits_per_key, size_t max_bf_size_ = 1024) : bits_per_key_(bits_per_key),
                                                                      max_bf_size_(max_bf_size_) {
            // We intentionally round down to reduce probing cost a little bit
            // k_ = static_cast<size_t>(bits_per_key * 0.69);  // 0.69 =~ ln(2)
            // e.g. bits_per_key = 10, k_ = 6, k_ is the iteration number of hash function
            k_ = static_cast<size_t>(bits_per_key * 0.69);  // 0.69 =~ ln(2)
            if (k_ < 1) k_ = 1;
            if (k_ > 30) k_ = 30;
        }
        // ~BloomFilter();

        // keys[0,n-1] contains a list of keys (potentially with duplicates)
        // that are ordered according to the user supplied comparator.
        // Append a filter that summarizes keys[0,n-1] to *dst.
        //
        // Warning: do not change the initial contents of *dst.  Instead,
        // append the newly constructed filter to *dst.
        void CreateFilter(const std::vector<uint64_t> &keys, std::string *dst);

        // "filter" contains the data appended by a preceding call to
        // CreateFilter() on this class.  This method must return true if
        // the key was in the list of keys passed to CreateFilter().
        // This method may return true or false if the key was not on the
        // list, but it should aim to return false with a high probability.
        bool KeyMayMatch(const std::string &key, const std::string &filter);

        void CombineFilters(std::string& result, const std::string& src);

    private:
        size_t bits_per_key_;
        size_t k_;
        size_t max_bf_size_;
    };

}
#endif //GEOSINDEX_BLOOMFILTER_H

//
// Created by cguo51 on 3/14/24.
//

#ifndef GEOSINDEX_CONSTANT_H
#define GEOSINDEX_CONSTANT_H

#include <limits>

namespace geosindex {
    constexpr double DoubleInfinity = (std::numeric_limits<double>::infinity)();

    const size_t max_bf_size = 1e9;

}

#endif //GEOSINDEX_CONSTANT_H

//
// Created by chang on 5/29/23.
//
#include <iostream>
#include "geosindex/bloomfilter.h"

int main() {
    geosindex::BloomFilter bf(10);
    std::vector<double> keys = {1.9, 2.9, 3.2};
    std::string dst;

    bf.CreateFilter(keys, keys.size(), &dst);
    std::cout << "bf" << std::endl;
    std::cout << bf.KeyMayMatch("a", dst) << std::endl;
    std::cout << bf.KeyMayMatch("b", dst) << std::endl;
    std::cout << bf.KeyMayMatch("c", dst) << std::endl;
    std::cout << bf.KeyMayMatch("d", dst) << std::endl;

    geosindex::BloomFilter bf2(10);
    std::vector<double> keys2 = {1.9, 2.9, 3.2};
    std::string dst2;

    bf2.CreateFilter(keys2, keys2.size(), &dst2);
    std::cout << "bf2" << std::endl;
    std::cout << bf2.KeyMayMatch("e", dst2) << std::endl;
    std::cout << bf2.KeyMayMatch("b", dst2) << std::endl;
    std::cout << bf2.KeyMayMatch("c", dst2) << std::endl;
    std::cout << bf2.KeyMayMatch("d", dst2) << std::endl;

    std::string new_dst;
    bf2.CombineFilters(dst, dst2, new_dst);

    std::cout << "bf3" << std::endl;
    std::cout << bf2.KeyMayMatch("e", new_dst) << std::endl;
    std::cout << bf2.KeyMayMatch("f", new_dst) << std::endl;
    std::cout << bf2.KeyMayMatch("c", new_dst) << std::endl;
    std::cout << bf2.KeyMayMatch("d", new_dst) << std::endl;
    std::cout << bf2.KeyMayMatch("a", new_dst) << std::endl;
    std::cout << bf2.KeyMayMatch("b", new_dst) << std::endl;
    std::cout << bf2.KeyMayMatch("c", new_dst) << std::endl;
    std::cout << bf2.KeyMayMatch("d", new_dst) << std::endl;

    return 0;
}

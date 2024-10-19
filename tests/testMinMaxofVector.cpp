//
// Created by cguo51 on 3/14/24.
//
#include <iostream>
#include <vector>
#include <algorithm>

int main() {
    std::vector<double> data = {3.14, 2.71, 1.618, 42.0, -10.0};

    auto minElement = std::min_element(data.begin(), data.end());

    std::cout << "Minimum value: " << *minElement << std::endl;

    return 0;
}
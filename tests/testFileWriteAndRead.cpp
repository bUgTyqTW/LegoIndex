#include <iostream>
#include <fstream>
#include <vector>

int main() {
    // Example string list
    std::vector<std::string> stringList = {"Hello", "World", "C++", "Programming"};

    // Writing to file
    std::fstream outputFile("example.txt", std::ios::out | std::ios::binary);

    if (!outputFile.is_open()) {
        std::cerr << "Error opening the file for writing!" << std::endl;
        return 1;
    }

    // Store the number of strings at the beginning of the file
    std::size_t numStrings = stringList.size();
    outputFile.write(reinterpret_cast<const char*>(&numStrings), sizeof(std::size_t));

    // Write each string along with its length
    for (const auto& str : stringList) {
        std::size_t strLength = str.length();
        outputFile.write(reinterpret_cast<const char*>(&strLength), sizeof(std::size_t));
        outputFile.write(str.c_str(), strLength);
    }

    outputFile.close();

    // Reading from file
    std::fstream inputFile("example.txt", std::ios::in | std::ios::binary);

    if (!inputFile.is_open()) {
        std::cerr << "Error opening the file for reading!" << std::endl;
        return 1;
    }

    // Read the number of strings
    std::size_t readNumStrings;
    inputFile.read(reinterpret_cast<char*>(&readNumStrings), sizeof(std::size_t));

    // Read each string back
    for (std::size_t i = 0; i < readNumStrings; ++i) {
        std::size_t strLength;
        inputFile.read(reinterpret_cast<char*>(&strLength), sizeof(std::size_t));

        std::string readStr;
        readStr.resize(strLength);
        inputFile.read(&readStr[0], strLength);

        // Output the read string
        std::cout << "Read String: " << readStr << std::endl;
    }

    inputFile.close();

    return 0;
}

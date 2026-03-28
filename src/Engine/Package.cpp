#include "Package.h"
#include <fstream>
#include <iostream>
#include <cstdint>

namespace Luwow::Engine {

Package::Package(): packagePath("") {}

Package::~Package() {}

void Package::addFile(const std::string& filePath, const std::string& content) {
    fileNames.push_back(filePath);
    fileContents.push_back(content);
}

int Package::indexOfFile(const std::string& filePath) {
    for (int i = 0; i < fileNames.size(); i++) {
        if (fileNames[i] == filePath) {
            return i;
        }
    }
    return -1;
}

int Package::getFileCount() {
    return fileNames.size();
}

std::string Package::getFileName(int index) {
    return fileNames[index];
}

std::string Package::getFileContent(int index) {
    return fileContents[index];
}

std::string Package::getFile(const std::string& filePath) {
    int index = indexOfFile(filePath);
    if (index != -1) {
        return fileContents[index];
    }
    std::cerr << "File not found: " << filePath << std::endl;
    return "";
}

// Signature is "PKG\1" in ASCII
static const uint32_t packageSignature = 0x474B5001;

bool Package::save(const std::string& packagePath) {
    this->packagePath = packagePath;
    std::ofstream out(packagePath, std::ios::out | std::ios::binary);
    if (!out)
    {
        // std::cerr << "Could not open output file: " << packagePath << std::endl;
        return false;
    }

    // Simple package format:
    // [script count]
    // Each script: [name length][name][bytecode length][bytecode]
    // End of file: [total size][4-byte signature]

    uint16_t fileCount = static_cast<uint16_t>(fileNames.size());
    out.write(reinterpret_cast<const char*>(&fileCount), sizeof(fileCount));
    uint32_t totalSize = sizeof(fileCount);

    for (int i = 0; i < fileNames.size(); i++) {
        uint16_t nameLen = static_cast<uint16_t>(fileNames[i].size());
        out.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
        out.write(fileNames[i].data(), nameLen);

        uint32_t bytecodeLen = static_cast<uint32_t>(fileContents[i].size());
        out.write(reinterpret_cast<const char*>(&bytecodeLen), sizeof(bytecodeLen));
        out.write(fileContents[i].data(), bytecodeLen);
        totalSize += sizeof(nameLen) + nameLen + sizeof(bytecodeLen) + bytecodeLen;
    }
    out.write(reinterpret_cast<const char*>(&totalSize), sizeof(totalSize));
    out.write(reinterpret_cast<const char*>(&packageSignature), sizeof(packageSignature));
    out.close();
    // std::cout << "Package saved to " << packagePath << std::endl;
    // std::cout << "Total size: " << (totalSize + sizeof(totalSize) + sizeof(signature)) << " bytes" << std::endl;
    return true;
}

bool Package::load(const std::string& packagePath) {

    std::ifstream file(packagePath, std::ios::in | std::ios::binary);
    if (!file)
    {
        std::cerr << "Could not open input file: " << packagePath << std::endl;
        return false;
    }

    // Validate the signature at the end of the file first
    uint32_t signature;
    file.seekg(-sizeof(signature), std::ios::end);
    file.read(reinterpret_cast<char*>(&signature), sizeof(signature));
    if (signature != packageSignature) {
        // std::cerr << "Invalid signature in package: " << packagePath << std::endl;
        return false;
    }

    // Then get the total size from before the signature

    // Seek to the end of the file and get the total size
    uint32_t packageSize;
    file.seekg(-sizeof(packageSize) -sizeof(signature), std::ios::end);
    file.read(reinterpret_cast<char*>(&packageSize), sizeof(packageSize));

    file.seekg(0, std::ios::end);
    uint32_t totalSize = file.tellg();

    // Validate that the file is equal to or longer than the total size of the package
    if (packageSize - sizeof(packageSize) > totalSize) {
        // std::cerr << "Can not read the package from the end of the file - it is too large to fit." << std::endl;
        return false;
    }

    // Seek back from the end to the beginning of the package in the file
    file.seekg(totalSize - packageSize - sizeof(packageSize) - sizeof(signature), std::ios::beg);

    uint16_t fileCount = 0;
    file.read(reinterpret_cast<char*>(&fileCount), sizeof(fileCount));

    for (int i = 0; i < fileCount; i++) {
        uint16_t nameLen = 0;
        file.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
        std::string fileName(nameLen, ' ');
        file.read(&fileName[0], nameLen);

        uint32_t bytecodeLen = 0;
        file.read(reinterpret_cast<char*>(&bytecodeLen), sizeof(bytecodeLen));
        std::string bytecode(bytecodeLen, ' ');
        file.read(&bytecode[0], bytecodeLen);

        addFile(fileName, bytecode);
    }
    return true;
}

} // namespace Luwow::Engine
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace Luwow::Engine {

class Package {
public:
    Package();
    ~Package();

    void addFile(const std::string& filePath, const std::string& content);
    std::string getFile(const std::string& filePath);

    // The file's index, -1 without it. Paths match whatever their form: "./src/a.luau" finds "src/a.luau",
    // as a require resolves a path differently from how the compiler was given it.
    int indexOfFile(const std::string& filePath);
    int getFileCount();

    // Assets are the package's files kept as they are: .png, .jpeg/.jpg and .txt. Every other file is a script's bytecode.
    static bool isAssetName(const std::string& filePath);
    bool isAsset(int index);
    std::string getFileName(int index);
    std::string getFileContent(int index);

    bool load(const std::string& packagePath);
    bool save(const std::string& packagePath);

private:
    std::string packagePath;
    std::vector<std::string> fileNames;
    std::vector<std::string> fileContents;

    // Normalized file names -> their index, rebuilt after files are added
    std::unordered_map<std::string, int> fileIndex;
    bool indexed = false;
};

} // namespace Luwow::Engine
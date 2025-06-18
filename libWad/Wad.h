#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>

class Wad {
private:
    std::fstream file;
    std::string magic;
    unsigned int numOfdescriptors;
    unsigned int descriptorOffset;

    Wad(const std::string &path);

    struct DescriptorEntry {
        std::string name;
        uint32_t offset;
        uint32_t size;
        bool isDirectory = false;
    
        std::vector<DescriptorEntry*> children;
        DescriptorEntry* parent = nullptr;
    
        DescriptorEntry(const std::string &n, uint32_t o, uint32_t s, bool isDir = false)
            : name(n), offset(o), size(s), isDirectory(isDir) {}
    };

    DescriptorEntry* root;
    std::unordered_map<std::string, DescriptorEntry*> pathMap;

    std::string getFullPath(DescriptorEntry* entry);
    

public:
    static Wad* loadWad(const std::string &path);
    std::string getMagic();
    ~Wad();

    bool isContent(const std::string &path);
    bool isDirectory(const std::string &path);
    int getSize(const std::string &path);
    int getContents(const std::string &path, char *buffer, int length, int offset = 0);
    int getDirectory(const std::string &path, std::vector<std::string> *directory);

    void createDirectory(const std::string &path);
    void createFile(const std::string &path);
    int writeToFile(const std::string &path, const char *buffer, int length, int offset = 0);

};
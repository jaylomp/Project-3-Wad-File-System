#include "Wad.h"
#include <iostream>
#include <cstring>
#include <unordered_set>
#include <algorithm>
#include <cassert>
#include <functional>

std::string normalize(const std::string& path) {
    if (path.size() > 1 && path.back() == '/')
        return path.substr(0, path.size() - 1);
    return path;
}

std::string cleanDescriptorName(const char* nameBuffer) {
    
    std::string name(nameBuffer, 8);
    name.erase(std::find(name.begin(), name.end(), '\0'), name.end());
    size_t pos = name.find_last_not_of(' ');
    if (pos != std::string::npos) {
        name.resize(pos + 1);
    } else {
        name.clear();
    }
    return name;
}

std::string Wad::getFullPath(Wad::DescriptorEntry* entry) {
    if (!entry || !entry->parent) return "/";

    std::string path = entry->name;
    DescriptorEntry* current = entry->parent;
    std::unordered_set<DescriptorEntry*> visited;

    while (current && current->parent) {
        if (visited.count(current)) break;
        visited.insert(current);
        path = current->name + "/" + path;
        current = current->parent;
    }

    return "/" + path;
}

Wad* Wad::loadWad(const std::string &path) {
    std::ifstream test(path);
    if (!test.good()) return nullptr;
    test.close();
    return new Wad(path);
}

Wad::Wad(const std::string &path) {
    file.open(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) exit(1);

    char magicBuffer[4];
    file.read(magicBuffer, 4);
    magic = std::string(magicBuffer, 4);
    file.read(reinterpret_cast<char*>(&numOfdescriptors), sizeof(numOfdescriptors));
    file.read(reinterpret_cast<char*>(&descriptorOffset), sizeof(descriptorOffset));

    root = new DescriptorEntry("/", 0, 0, true);
    pathMap["/"] = root;

    file.seekg(descriptorOffset, std::ios::beg);
    std::vector<DescriptorEntry*> descriptors;

    for (unsigned int i = 0; i < numOfdescriptors; ++i) {
        uint32_t offset, size;
        char nameBuffer[8] = {0};
        file.read(reinterpret_cast<char*>(&offset), sizeof(offset));
        file.read(reinterpret_cast<char*>(&size), sizeof(size));
        file.read(nameBuffer, 8);

        std::string cleaned = cleanDescriptorName(nameBuffer);
        if (!cleaned.empty()) {
            descriptors.push_back(new DescriptorEntry(cleaned, offset, size));
        }
    }

    DescriptorEntry* currentNamespace = root;
    std::vector<DescriptorEntry*> stack;
    bool inMap = false;
    int mapElementCount = 0;

    for (auto& desc : descriptors) {
        if (desc->name.size() > 6 && desc->name.substr(desc->name.size() - 6) == "_START") {
            std::string dirName = desc->name.substr(0, desc->name.size() - 6);
            DescriptorEntry* dir = new DescriptorEntry(dirName, 0, 0, true);
            dir->parent = currentNamespace;
            currentNamespace->children.push_back(dir);
            pathMap[getFullPath(dir)] = dir;
            stack.push_back(currentNamespace);
            currentNamespace = dir;
        } else if (desc->name.size() > 4 && desc->name.substr(desc->name.size() - 4) == "_END") {
            if (!stack.empty()) {
                currentNamespace = stack.back();
                stack.pop_back();
            }
        } else if (desc->name.size() == 4 && desc->name[0] == 'E' && desc->name[2] == 'M') {
            DescriptorEntry* mapDir = new DescriptorEntry(desc->name, 0, 0, true);
            mapDir->parent = currentNamespace;
            currentNamespace->children.push_back(mapDir);
            pathMap[getFullPath(mapDir)] = mapDir;
            inMap = true;
            mapElementCount = 0;
            currentNamespace = mapDir;
        } else if (inMap && mapElementCount < 10) {
            desc->parent = currentNamespace;
            currentNamespace->children.push_back(desc);
            pathMap[getFullPath(desc)] = desc;
            ++mapElementCount;
            if (mapElementCount == 10) {
                currentNamespace = currentNamespace->parent;
                inMap = false;
            }
        } else {
            desc->parent = currentNamespace;
            currentNamespace->children.push_back(desc);
            pathMap[getFullPath(desc)] = desc;
        }
    }
}

std::string Wad::getMagic() {
    return magic;
}

Wad::~Wad() {
    if (file.is_open()) {
        file.close();
    }
}

bool Wad::isContent(const std::string &path) {
    std::string normalized = normalize(path);
    auto it = pathMap.find(normalized);
    return it != pathMap.end() && !it->second->isDirectory;
}

bool Wad::isDirectory(const std::string &path) {
    std::string normalized = normalize(path);
    auto it = pathMap.find(normalized);
    return it != pathMap.end() && it->second->isDirectory;
}

int Wad::getSize(const std::string &path) {
    std::string normalized = normalize(path);
    auto it = pathMap.find(normalized);

    if (it == pathMap.end() || it->second->isDirectory) return -1;
    return static_cast<int>(it->second->size);
}

int Wad::getContents(const std::string &path, char *buffer, int length, int offset) {
    std::string normalized = normalize(path);
    auto it = pathMap.find(normalized);
    if (it == pathMap.end()) return -1;
    DescriptorEntry* entry = it->second;
    if (entry->isDirectory) return -1;

    if (offset >= static_cast<int>(entry->size)) return 0;
    int bytesToRead = std::min(length, static_cast<int>(entry->size) - offset);

    file.seekg(entry->offset + offset, std::ios::beg);
    file.read(buffer, bytesToRead);
    return bytesToRead;
}

int Wad::getDirectory(const std::string &path, std::vector<std::string> *directory) {
    std::string normalized = normalize(path);
    auto it = pathMap.find(normalized);
    if (it == pathMap.end() || !it->second->isDirectory)
        return -1;

    directory->clear();
    for (auto child : it->second->children)
        directory->push_back(child->name);

    return static_cast<int>(directory->size());
}

void Wad::createDirectory(const std::string &path) {
    std::string normalized = normalize(path);
    size_t lastSlash = normalized.find_last_of('/');
    std::string parentPath = (lastSlash == 0) ? "/" : normalized.substr(0, lastSlash);
    std::string newDirName = normalized.substr(lastSlash + 1);

    if (pathMap.find(parentPath) == pathMap.end() || !pathMap[parentPath]->isDirectory) return;
    if (newDirName.length() > 2 || pathMap.find(normalized) != pathMap.end()) return;

    DescriptorEntry* parent = pathMap[parentPath];
    std::string expectedEndMarker = parent->name + "_END";

    file.clear();
    file.seekg(descriptorOffset);
    std::vector<char> descriptors(numOfdescriptors * 16);
    file.read(descriptors.data(), descriptors.size());

    int insertIndex = -1;
    for (int i = 0; i < static_cast<int>(numOfdescriptors); ++i) {
        std::string name = cleanDescriptorName(&descriptors[i * 16 + 8]);
        if (name == expectedEndMarker) {
            insertIndex = i;
            break;
        }
    }
    if (insertIndex == -1 && parentPath != "/") return;
    if (insertIndex == -1) insertIndex = numOfdescriptors;
    std::vector<char> newDescriptors;
    uint32_t zero = 0;
    std::string startName = newDirName + "_START";
    std::string endName = newDirName + "_END";

    auto writeDescriptor = [&](const std::string &name) {
        newDescriptors.insert(newDescriptors.end(), reinterpret_cast<char*>(&zero), reinterpret_cast<char*>(&zero) + 4);
        newDescriptors.insert(newDescriptors.end(), reinterpret_cast<char*>(&zero), reinterpret_cast<char*>(&zero) + 4);
        char nameBuf[8] = {0};
        strncpy(nameBuf, name.c_str(), 8);
        newDescriptors.insert(newDescriptors.end(), nameBuf, nameBuf + 8);
    };

    writeDescriptor(startName);
    writeDescriptor(endName);
    descriptors.insert(descriptors.begin() + insertIndex * 16, newDescriptors.begin(), newDescriptors.end());

    numOfdescriptors += 2;
    descriptorOffset += 32;
    file.seekp(4);
    file.write(reinterpret_cast<char*>(&numOfdescriptors), 4);
    file.write(reinterpret_cast<char*>(&descriptorOffset), 4);

    file.seekp(descriptorOffset);
    file.write(descriptors.data(), descriptors.size());
    file.flush();

    DescriptorEntry* newDir = new DescriptorEntry(newDirName, 0, 0, true);
    newDir->parent = parent;
    parent->children.push_back(newDir);
    pathMap[normalized] = newDir;
}

void Wad::createFile(const std::string &path) {
    std::string normalized = normalize(path);
    size_t lastSlash = normalized.find_last_of('/');
    std::string parentPath = (lastSlash == 0) ? "/" : normalized.substr(0, lastSlash);
    std::string fileName = normalized.substr(lastSlash + 1);

    if (fileName.length() > 8 || fileName.find('/') != std::string::npos)
        return;

    if (fileName.length() == 4 && fileName[0] == 'E' && fileName[2] == 'M' &&
        std::isdigit(fileName[1]) && std::isdigit(fileName[3])) {
        return;
    }

    if (pathMap.find(parentPath) == pathMap.end() || !pathMap[parentPath]->isDirectory)
        return;

    if (pathMap.find(normalized) != pathMap.end())
        return;

    DescriptorEntry* parent = pathMap[parentPath];
    std::string expectedEndMarker = parent->name + "_END";

    file.clear();
    file.seekg(descriptorOffset);
    std::vector<char> descriptors(numOfdescriptors * 16);
    file.read(descriptors.data(), descriptors.size());
    int insertIndex = -1;
    for (int i = 0; i < static_cast<int>(numOfdescriptors); ++i) {
        std::string name = cleanDescriptorName(&descriptors[i * 16 + 8]);
        if (name == expectedEndMarker) {
            insertIndex = i;
            break;
        }
    }
    if (insertIndex == -1 && parentPath != "/") return;
    if (insertIndex == -1) insertIndex = numOfdescriptors;

    uint32_t zero = 0;
    std::vector<char> newDescriptor;
    newDescriptor.insert(newDescriptor.end(), reinterpret_cast<char*>(&zero), reinterpret_cast<char*>(&zero) + 4);
    newDescriptor.insert(newDescriptor.end(), reinterpret_cast<char*>(&zero), reinterpret_cast<char*>(&zero) + 4);
    char nameBuf[8] = {0};
    strncpy(nameBuf, fileName.c_str(), 8);
    newDescriptor.insert(newDescriptor.end(), nameBuf, nameBuf + 8);

    descriptors.insert(descriptors.begin() + insertIndex * 16,
                       newDescriptor.begin(), newDescriptor.end());

    numOfdescriptors += 1;
    descriptorOffset = static_cast<uint32_t>(file.tellp());

    file.seekp(4);
    file.write(reinterpret_cast<char*>(&numOfdescriptors), 4);
    file.write(reinterpret_cast<char*>(&descriptorOffset), 4);

    file.seekp(descriptorOffset);
    file.write(descriptors.data(), descriptors.size());
    file.flush();
    DescriptorEntry* newFile = new DescriptorEntry(fileName, 0, 0, false);
    newFile->parent = parent;
    parent->children.push_back(newFile);
    pathMap[normalized] = newFile;
}

int Wad::writeToFile(const std::string &path, const char *buffer, int length, int offset) {
    std::string normalized = normalize(path);
    auto it = pathMap.find(normalized);
    if (it == pathMap.end() || it->second->isDirectory)
        return -1;

    DescriptorEntry* fileEntry = it->second;
    if (fileEntry->size != 0)
        return 0;

    file.seekp(0, std::ios::end);
    int dataOffset = static_cast<int>(file.tellp());
    file.write(buffer, length);
    file.flush();

    fileEntry->offset = dataOffset;
    fileEntry->size = length;

    std::vector<char> newDescriptors;
    std::function<void(DescriptorEntry*)> writeDescriptorTree = [&](DescriptorEntry* entry) {
        if (entry->isDirectory) {
            if (entry->name != "/") {
                std::string start = entry->name + "_START";
                std::string end = entry->name + "_END";
                uint32_t zero = 0;
                char nameBuf[8] = {0};
                strncpy(nameBuf, start.c_str(), 8);
                newDescriptors.insert(newDescriptors.end(), reinterpret_cast<char*>(&zero), reinterpret_cast<char*>(&zero) + 4);
                newDescriptors.insert(newDescriptors.end(), reinterpret_cast<char*>(&zero), reinterpret_cast<char*>(&zero) + 4);
                newDescriptors.insert(newDescriptors.end(), nameBuf, nameBuf + 8);
            }

            for (auto child : entry->children)
                writeDescriptorTree(child);

            if (entry->name != "/") {
                uint32_t zero = 0;
                char nameBuf[8] = {0};
                std::string end = entry->name + "_END";
                strncpy(nameBuf, end.c_str(), 8);
                newDescriptors.insert(newDescriptors.end(), reinterpret_cast<char*>(&zero), reinterpret_cast<char*>(&zero) + 4);
                newDescriptors.insert(newDescriptors.end(), reinterpret_cast<char*>(&zero), reinterpret_cast<char*>(&zero) + 4);
                newDescriptors.insert(newDescriptors.end(), nameBuf, nameBuf + 8);
            }
        } else {
            char nameBuf[8] = {0};
            strncpy(nameBuf, entry->name.c_str(), 8);
            newDescriptors.insert(newDescriptors.end(), reinterpret_cast<char*>(&entry->offset), reinterpret_cast<char*>(&entry->offset) + 4);
            newDescriptors.insert(newDescriptors.end(), reinterpret_cast<char*>(&entry->size), reinterpret_cast<char*>(&entry->size) + 4);
            newDescriptors.insert(newDescriptors.end(), nameBuf, nameBuf + 8);
        }
    };

    writeDescriptorTree(root);

    numOfdescriptors = static_cast<uint32_t>(newDescriptors.size() / 16);
    file.seekp(0, std::ios::end);
    descriptorOffset = static_cast<uint32_t>(file.tellp());

    file.seekp(4);
    file.write(reinterpret_cast<char*>(&numOfdescriptors), 4);
    file.write(reinterpret_cast<char*>(&descriptorOffset), 4);

    file.seekp(descriptorOffset);
    file.write(newDescriptors.data(), newDescriptors.size());
    file.flush();

    return length;
}
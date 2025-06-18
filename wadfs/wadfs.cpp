#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <cstring>
#include <string>
#include <iostream>
#include "../libWad/Wad.h"
#include <sys/stat.h>
#include <sstream>

Wad* wad = nullptr;

int wadfs_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));
    std::string p(path);

    if (wad->isDirectory(p)) {
        stbuf->st_mode = S_IFDIR | 0777;
        stbuf->st_nlink = 2;
        return 0;
    }

    if (wad->isContent(p)) {
        stbuf->st_mode = S_IFREG | 0777;
        stbuf->st_nlink = 1;
        stbuf->st_size = wad->getSize(p);
        return 0;
    }

    return -ENOENT;
}

int wadfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                  off_t offset, struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;
    std::string p(path);
    if (!wad->isDirectory(p)) return -ENOENT;

    filler(buf, ".", nullptr, 0);
    filler(buf, "..", nullptr, 0);

    std::vector<std::string> contents;
    int count = wad->getDirectory(p, &contents);
    if (count < 0) return -ENOENT;

    for (const auto& name : contents) {
        filler(buf, name.c_str(), nullptr, 0);
    }

    return 0;
}

int wadfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    std::string p(path);
    if (!wad->isContent(p)) return -EISDIR;

    int bytesRead = wad->getContents(p, buf, size, offset);
    if (bytesRead < 0) return -ENOENT;

    return bytesRead;
}

int wadfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    std::string p(path);
    std::cerr << "FUSE: write called for path = " << p
              << " with size = " << size << ", offset = " << offset << std::endl;

    if (!wad->isContent(p)) {
        std::cerr << "FUSE: write failed — not content\n";
        return -EISDIR;
    }

    int written = wad->writeToFile(p, buf, size, offset);
    if (written < 0) {
        std::cerr << "FUSE: writeToFile failed\n";
        return -EIO;
    }

    std::cerr << "FUSE: write succeeded — wrote " << written << " bytes\n";
    return written;
}

int wadfs_open(const char *path, struct fuse_file_info *fi) {
    std::string p(path);
    if (!wad->isContent(p)) return -EISDIR;
    return 0;
}

int wadfs_mkdir(const char *path, mode_t mode) {
    std::string p(path);

    if (wad->isDirectory(p) || wad->isContent(p)) return -EEXIST;

    std::string parentPath = p.substr(0, p.find_last_of('/'));
    if (parentPath.empty()) parentPath = "/";

    if (!wad->isDirectory(parentPath)) return -ENOENT;

    size_t lastSlash = p.find_last_of('/');
    std::string newDirName = p.substr(lastSlash + 1);
    if (newDirName.length() > 2) return -EINVAL;

    wad->createDirectory(p);

    if (!wad->isDirectory(p)) return -EIO;
    return 0;
}

int wadfs_mknod(const char *path, mode_t mode, dev_t rdev) {
    std::string p(path);

    if (!S_ISREG(mode)) return -EINVAL;
    if (wad->isDirectory(p) || wad->isContent(p)) return -EEXIST;

    std::string parentPath = p.substr(0, p.find_last_of('/'));
    if (parentPath.empty()) parentPath = "/";

    if (!wad->isDirectory(parentPath)) return -ENOENT;

    wad->createFile(p);
    if (!wad->isContent(p)) return -EIO;

    return 0;
}

int wadfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    std::string p(path);
    if (wad->isDirectory(p) || wad->isContent(p)) return -EEXIST;

    std::string parentPath = p.substr(0, p.find_last_of('/'));
    if (parentPath.empty()) parentPath = "/";

    if (!wad->isDirectory(parentPath)) return -ENOENT;

    wad->createFile(p);
    if (!wad->isContent(p)) return -EIO;

    return 0;
}

int wadfs_utimens(const char *path, const struct timespec tv[2]) {
    std::string p(path);
    if (!wad->isContent(p)) return -EISDIR;
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " -s <wadfile.wad> <mountpoint>\n";
        return 1;
    }

    wad = Wad::loadWad(argv[2]);
    if (!wad) {
        std::cerr << "Failed to load WAD file: " << argv[2] << std::endl;
        return 1;
    }

    struct fuse_operations ops;
    memset(&ops, 0, sizeof(ops));
    ops.getattr = wadfs_getattr;
    ops.readdir = wadfs_readdir;
    ops.read = wadfs_read;
    ops.write = wadfs_write;
    ops.open = wadfs_open;
    ops.mkdir = wadfs_mkdir;
    ops.mknod = wadfs_mknod;
    ops.create = wadfs_create;
    ops.utimens = wadfs_utimens;

    argv[2] = argv[3];
    argc--;

    return fuse_main(argc, argv, &ops, nullptr);
}
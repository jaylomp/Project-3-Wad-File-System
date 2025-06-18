# WAD Filesystem (libWad + wadfs)  
**COP4600 Project 3: File Systems**  

---

## Overview  
This project implements a userspace filesystem daemon to access and manipulate WAD files — the classic game data archive format used in titles like DOOM. The goal is to provide full read and write access to files and directories within WAD archives, exposing them as a mounted filesystem using FUSE.

---

## Project Components  

- **libWad**  
  A C++ library that loads, parses, and manipulates WAD files, representing their internal structure as a hierarchical directory and file system.

- **wadfs**  
  A FUSE-based userspace daemon that mounts a WAD file as a virtual filesystem, exposing its contents with full read/write access to files and directories.

---

## Key Features  

- Load and parse WAD files, including headers, descriptors, and lumps.  
- Represent marker elements as directories:  
  - **Map markers** (e.g., E1M1) are read-only directories containing fixed lumps.  
  - **Namespace markers** (_START and _END pairs) define modifiable directories.  
- Read and write file contents within the WAD.  
- Create new directories and files inside namespace directories (not allowed inside map marker directories).  
- Provide full read, write, and execute permissions on files and directories.  
- Persist all changes across mounts.

---

## Usage  

### Build  
```bash
tar zxvf wad.tar.gz
cd libWad
make
cd ../wadfs
make
cd ..
```

RUN

Mount a WAD file at a mount point:
```
./wadfs/wadfs -s somewadfile.wad /some/mount/directory
```
Example Commands after Mount
```
ls /home/reptilian/mountdir/F/F1
cat /home/reptilian/mountdir/F/F1/LOLWUT
mkdir /home/reptilian/mountdir/F/NewDir
touch /home/reptilian/mountdir/F/NewFile
```
Library Highlights
```
loadWad(const string &path) — Load WAD data into memory.

isContent(const string &path) — Check if a path is a file.

isDirectory(const string &path) — Check if a path is a directory.

getContents(const string &path, char *buffer, int length, int offset = 0) — Read file contents.

getDirectory(const string &path, vector<string> *directory) — List directory entries.

createDirectory(const string &path) — Create a new namespace directory.

createFile(const string &path) — Create a new empty file.

writeToFile(const string &path, const char *buffer, int length, int offset = 0) — Write data to a file.
```

Implementation Notes
Namespace directories are defined by pairs of _START and _END markers; new files and directories are inserted just before the parent directory’s _END marker.

Map marker directories are immutable and represent fixed lumps in the WAD file.

The FUSE daemon implements essential callbacks:

getattr — Retrieve file/directory attributes

mknod — Create a new file

mkdir — Create a new directory

read — Read file data

write — Write file data

readdir — Read directory entries

Author Jaylom Pairol

CXX = g++
CXXFLAGS = -Wall -std=c++11 -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26
LDFLAGS = -lfuse

all: wadfs

wadfs: wadfs.cpp
	$(CXX) $(CXXFLAGS) wadfs.cpp -o wadfs $(LDFLAGS) -L../libWad -lWad

clean:
	rm -f wadfs *.o
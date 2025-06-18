CXX = g++
CXXFLAGS = -Wall -std=c++11 -c

OBJECTS = Wad.o
LIBRARY = libWad.a

all: $(LIBRARY)

$(LIBRARY): $(OBJECTS)
	ar rcs $(LIBRARY) $(OBJECTS)

Wad.o: Wad.cpp Wad.h
	$(CXX) $(CXXFLAGS) Wad.cpp

clean:
	rm -f *.o *.a

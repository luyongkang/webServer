SOURCE:= $(wildcard *.cpp)
OBJS:= $(patsubst %.cpp,%.o,$(SOURCE))

TARGET:= myserver
CC:= g++
LIBS:= -lpthread
CFLAGS:= -std=c++11 -g -Wall
CXXFLAGS:=$(CFLAGS)

.PHONY : objs clean all
all:$(TARGET)

objs:$(OBJS)

clean:
	rm -rf *.o


$(TARGET):$(OBJS)
	$(CC) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)
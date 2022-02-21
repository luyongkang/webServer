SOURCES := $(wildcard *.cpp)
OBJECTS := $(patsubst %.cpp,%.o,$(SOURCES))

all: contains $(OBJECTS)

contains:
	cd time && make

$(OBJECTS): %.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $(tmp_dir)$@ -I $(INCLUDE_PATH)
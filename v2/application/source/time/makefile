SOURCES := $(wildcard *.cpp)
OBJECTS := $(patsubst %.cpp,%.o,$(SOURCES))

all: $(OBJECTS)

$(OBJECTS): %.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $(tmp_dir)$@ -I $(INCLUDE_PATH)
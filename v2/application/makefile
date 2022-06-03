#makefile所在的完全路径
mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
mkfile_dir := $(dir $(mkfile_path))
export tmp_dir := $(mkfile_dir)tmp/

#编译器与选项
TARGET := myserver 
export CC := clang++
export CFLAGS := -std=c++20 -g -Wall
export INCLUDE_PATH := $(mkfile_dir)headers

.PHONY: all clean cleanAll

all:
	test -d tmp || mkdir tmp
	cd source && make
	cd tmp && $(CC) $(CFLAGS) *.o -o ../$(TARGET)

clean:
	-rm -rf tmp

cleanAll:
	-rm -rf tmp
	-rm -rf myserver
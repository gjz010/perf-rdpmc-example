CFLAGS = -O3 -std=gnu17
CXXFLAGS = -Wall -Werror -pedantic -Wextra -Wconversion -O3 -std=c++20
LDFLAGS = -lpfm
all: test_rdpmc showevtinfo hello_perf
test_rdpmc: test_rdpmc.cpp
showevtinfo: showevtinfo.c
hello_perf: hello_perf.cpp
run: test_rdpmc
	./test_rdpmc
clean:
	rm -f test_rdpmc showevtinfo hello_perf
.PHONY: all clean
all: test_rdpmc showevtinfo
test_rdpmc: test_rdpmc.o
	g++ -o test_rdpmc test_rdpmc.o -lpthread -lpfm
showevtinfo: showevtinfo.c
	gcc -o showevtinfo showevtinfo.c -lpfm
test_rdpmc.o: test_rdpmc.cpp
	g++ -c test_rdpmc.cpp
run: test_rdpmc
	./test_rdpmc
clean:
	rm -f test_rdpmc test_rdpmc.o showevtinfo
.PHONY: all clean
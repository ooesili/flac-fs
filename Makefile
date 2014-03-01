all: flac-fs

flac-fs: main.c flac.o
	gcc -g -Wall `pkg-config fuse --cflags --libs` `pkg-config flac --cflags --libs` $^ -o $@

flac.o: flac.c
	gcc -g -Wall -c `pkg-config flac --cflags --libs` $^ -o $@

clean:
	rm *.o flac-fs

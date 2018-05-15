all:
	gcc -std=gnu99 -ggdb -O0 -g -o test mp4_parse.c stream.c main.c

clean:
	rm test

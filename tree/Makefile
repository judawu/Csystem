CFLAGS  = -Wall -O2 -std=c2x
ldflags=

.PHONY: all clean

all: clean tree


tree:tree.o
	$(CC) $(CFLAGS) $^ -o $@ ${ldflags}


tree.o: tree.c
	$(CC) $(CFLAGS) -c $^

clean:
	rm -f *.o *.so tree


CFLAGS   = -O2 -Wall -std=c2x -I. -Wno-address-of-packed-member


.PHONY: all clean

all: clean ethernet

ethernet: ethernet.o
	$(CC) $^ -o $@    

ethernet.o: ethernet.c ethernet.h
	$(CC) $(CFLAGS) -c $< -o $@   

clean:
	rm -f *.o ethernet            
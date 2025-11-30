CFLAGS   = -O2 -Wall -std=c2x -I. -Wno-address-of-packed-member


.PHONY: all clean

all: clean ether

ether: ether.o
	$(CC) $^ -o $@    

ether.o: ether.c ether.h
	$(CC) $(CFLAGS) -c $< -o $@   

clean:
	rm -f *.o ether            
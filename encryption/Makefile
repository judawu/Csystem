CFLAGS  = -O2 -Wall -fPIC -D_GNU_SOURCE

all: arc4.so example fse

fse: fse.o arc4.so
	$(CC) fse.o arc4.so -o fse -ldl -Wl,-rpath=.
fse.o:fse.c fse.h arc4.h
	$(CC) $(CFLAGS) -c $< -o $@

example: example.o arc4.so
	$(CC) example.o arc4.so -o example -ldl -Wl,-rpath=.


arc4.so: arc4.o
	$(CC) -shared arc4.o -o arc4.so -ldl

example.o: example.c arc4.h
	$(CC) $(CFLAGS) -c $< -o $@

arc4.o: arc4.c arc4.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.so example

.PHONY: all clean
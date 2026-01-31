flags=-O2 -Wall -std=c2x
ldflags=
.PHONY:all clean
all:clean vm
vm:vm.o
	cc $(flags) $< -o $@ $(ldflags)
vm.o:vm.c vm.h
	cc $(flags) -c $<
clean:
	rm -f *.o vm
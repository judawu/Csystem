/*#define _GNU_SOURCE*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <sys/random.h>
#include <termio.h>
#include "arc4.h"


struct bytestream{ 
    int16 size;
    int8  *data;  
};




void changeecho(bool);
int8 *securerand(int16);
int8 *readkey(char*);
void simplehash(const int8 *, int16, int8 [16]);
struct bytestream  *padinit(int16 size);
struct bytestream *encrytion(Arc4*,const struct bytestream *,int16);
struct bytestream *decrytion(Arc4*,const struct bytestream *,int16);
 
void printhex(int8*,int16);


int main(int,char**);
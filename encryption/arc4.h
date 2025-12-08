/*ar4.h*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#define WHITECOUNT 50
typedef unsigned char int8;
typedef unsigned short int16;
typedef unsigned int int32;
typedef unsigned long long int64;

struct s_arc4 {
    int8 i;
    int8 j;
    int8 s[256];
};
typedef struct s_arc4 Arc4;



#define export __attribute__((visibility("default")))
#define rc4decrypt(x,y,z) rc4encrypt(x,y,z)
#define rc4uninit(x) free(x)
#define rc4whitewash(x,y)  for(x=0;x<(WHITECOUNT)*1000000;x++) \
                                   (volatile int8)rc4byte(y)

export Arc4 *rc4init(int8*,int16);
int8 rc4byte(Arc4*);
export int8 *rc4encrypt(Arc4 *,int8*,int16);





/*safestring struct

Makefile
all: clean safestring.so
 safetring.so:safestring.o
    cc $^ -o $@ ${opt}

safestring.o:safestring.c
    cc -c $^ -o $@
clean:
    rm -f *.0 *.so safestring
*/
#define _GNC_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#define stringUninit(x)  free(x)
struct safestring {
  unsigned int count;
  char data[0];
};
typedef struct safestring String;
void copy(void *,const void *,const unsigned int);
unsigned int stringLength(const char *);
String *stringInit(char *);
bool stringConcat(String*,char*);
char *stringFold(String*);
int main(int, char**);

void copy(void *dst,const void *src,const unsigned int n){
    int i;
    char *psrc;
    char *pdst;
    for(i=n,psrc=(char*)src,pdst=(char*)dst;i;i--,*pdst++=*psrc++);

};
unsigned int stringLength(const char *str){
    int n;
    const char *p;
    for(n=0,p=str;*p;n++,p++);
    return n;
};

String *stringInit(char *str){
    unsigned int n,size;
    String *p;
    n=stringLength(str);
    size=n+sizeof(String);
    p=(String *)malloc(size);
    assert(p);
    *p=(String){};
    copy(p->data,str,n);
    p->count=n;
    return p;
};
 bool stringConcat(String *dst,char *src){
   unsigned int n,size;
   String *p;
   char *cp;
   n=stringLength(src);
   size=dst->count+sizeof(String)+n;
   p=(String*)realloc(dst,size);
   if(!p)
     return false;
   cp=p->data+p->count;
   copy(cp,src,n);
   p->count+=n;
   return true;
 };
 char *stringFold(String *str){
    return str->data;
 };
int main(int argc, char *argv[]){
    String *str;
    str=stringInit("Hello");
    printf("%s\n",str->data);
    stringConcat(str," there!");
    printf("%s\n",str->data);
    printf("%s\n",stringFold(str));
    stringUninit(str);
    return 0;
}
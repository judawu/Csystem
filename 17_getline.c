/*getline.c
(for i in $(seq 0 10000);do printf "a";done;echo) | ./test
*/
#include <stdio.h>
#include <unistd.h> 
#include <stdlib.h>
#define BUFSIZE 2048
char *gnl(void);
char *zero(char*);
int main(void);
char *zero(char *p){
    unsigned int n;
    for(n=0;n<BUFSIZE;*(p+(n++))=0);
    return p;
};
char *gnl(void){
    unsigned int n;
    char *p,*s;
    int r;
    n=0;
    s=p=malloc(BUFSIZE);
    zero(s);
    while(1){
      r= read(0,p,1);
      if(r<1){
           if(n<1){
            return 0;
           }else{
            return s;
           };
        }
        else if(n>BUFSIZE-2){
            return s;

        };
         switch(*p){
      case 0:
      case '\n':
        *p=0;
         return s;
         break;
      case '\r':
        *p=0;
         break;
       default:
        p++;
        n++;
    };
    };

 return (n<1)?0:s;

};
int main(void){
    char *p1,*p2;
    p1=gnl();
    p2=gnl();

    printf(
        "'%s'\r\n"
        "'%s'\n",
        p1,
        p2
    );
    free(p1);
    free(p2);
    return 0;

}
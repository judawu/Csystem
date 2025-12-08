#include "arc4.h"

#define f fflush(stdout)

int main(void);

void printbin(int8 *input,const int16 size){
    int16 i;
    int8 *p;
    assert(size>0);
    for (i=size,p=input;i;i--,p++){
        if(!(i%2))
         printf(" ");
        printf("%.02x",*p);
    };
    printf("\n");

};
int main(void){
    Arc4 *arc4;
   int16 skey,stext;
   char *key,*from;
   int8 *encrypted,*decrypted;
   
   key="thisiskey"; 
   skey=strlen(key);
   from="this is a example to be used for arc4 encryption.";
   stext=strlen(from);
   printf("Initializing encryyption ...");
   arc4=rc4init((int8*)key,skey);
   f;
   printf("done\n");
   printf("'%s'\n->",from);
   encrypted=rc4encrypt(arc4,(int8*)from,stext);
   printbin(encrypted,stext);
   //rc4uninit(arc4);
   arc4=rc4init((int8*)key,skey);
   decrypted=rc4decrypt(arc4,encrypted,stext);
   printf("   ->'%s'\n->",decrypted);
   
   rc4uninit(arc4);

   return 0;

}
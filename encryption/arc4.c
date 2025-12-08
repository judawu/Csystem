#include "arc4.h"
export Arc4 *rc4init(int8 *key,int16 size){
   int8 i,j,tmp;
    Arc4 *p;
    int32 n;
    p=malloc(sizeof(Arc4));
    if(!p){
        assert_perror(errno);
    };

    /*
    for i from 0 to 255
    S[i] := i
endfor
*/
   for (i=0;i<255;i++)
        p->s[i]=i;
/*
j := 0
for i from 0 to 255
    j := (j + S[i] + key[i mod keylength]) mod 256
    swap values of S[i] and S[j]
endfor
*/
    j=tmp=0;
    for (i=0;i<255;i++){
     
     j+=(p->s[i]+ key[i % size]) % 256;
     tmp=p->s[i];
     p->s[i]=p->s[j];
     p->s[j]=tmp;

};
   p->i=p->j=0;
   rc4whitewash(n,p);
   return p;

};

int8 rc4byte(Arc4 *p){
    int8 tmp;

/*

    i := (i + 1) mod 256
    j := (j + S[i]) mod 256
    swap values of S[i] and S[j]
    t := (S[i] + S[j]) mod 256
    K := S[t]
    output K

*/
    p->i= (p->i+1)%256;
    p->j= (p->j+p->s[p->i])%256;
    tmp=p->s[p->i];
    p->s[p->i]=p->s[p->j];
    p->s[p->j]=tmp;
    tmp=(p->s[p->i]+p->s[p->j])%256;
    return p->s[tmp];

};

export int8 *rc4encrypt(Arc4 *p,int8 *text,int16 size){
    int8 *cipertext;
    int i;

    cipertext=(int8 *)malloc(size);
    if(!cipertext){
        assert_perror(errno);
    };
    for(i=0;i<size;i++){
        cipertext[i]=text[i]^rc4byte(p);
    };
    return cipertext;

};
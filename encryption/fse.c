#include "fse.h"
 void printhex(int8 *data, int16 size){
    int16 i;
    int8 *p;
    for(p=data, i=0; i<size; i++){
        printf("%02x ", *p++);
    }
    printf("\n");
    return;
    };
void changeecho(bool enable){
   struct termios *t;

   t=(struct termios *)malloc(sizeof(struct termios));
   /* int tcgetattr(int fd, struct termios *termios_p);
       int tcsetattr(int fd, int optional_actions,
                     const struct termios *termios_p);*/

   tcgetattr(0,t);
   if(enable)
     t->c_lflag |= ECHO;
   else
      t->c_lflag  &= ~ECHO;
    tcsetattr(0,0,t);
    free(t);
    return;
   
};
int8 *securerand(int16 size){
    int8 *p,*start;
    size_t n;
    assert(size);
    p=(int8*)malloc(size);
    assert(p);
    start=p;
    n=getrandom(p,(size_t)size,GRND_RANDOM|GRND_NONBLOCK);
    if(n==size)
      return p;
    else if (n<0){
        free(p);
        return 0;
    }
    else 
      fprintf(stderr,"warning:entropy pool is"
    "empty, this way take longer than usual.\n");
    p+=n;
    n=getrandom(p,(size_t)(size-n),GRND_RANDOM);
    if (n==size)
     return start;
    else {
        free(start);
        return 0;
    }

};
int8 *readkey(char *prompt){
    char buf[256] = {0};
    printf("%s", prompt); 
    fflush(stdout);
    changeecho(false);

    int n = read(0, buf, sizeof(buf)-1);
    changeecho(true);
    printf("\n");

    if (n <= 0) return NULL;

    // 去掉换行符
    if (buf[n-1] == '\n') buf[n-1] = '\0';
    if (buf[n-2] == '\r') buf[n-2] = '\0';

    int len = strlen(buf);
    int8 *key = malloc(len + 1);
    if (key) {
        memcpy(key, buf, len + 1);
        memset(buf, 0, sizeof(buf));  // 擦除栈上痕迹
    }
    return key;
};
struct bytestream *padinit(int16 size){
    int16 r = 0;
    r=(int16)*securerand(2);
    int16 padlen = 1 + (r % size);  

    struct bytestream *p = malloc(sizeof(struct bytestream));
    p->size = padlen;
    p->data = securerand(padlen);
    assert(p->data);
    return p;
};

void simplehash(const int8 *data, int16 size, int8 out[16])
{
    int64 h1 = 0x9e3779b97f4a7c15ULL;
    int64 h2 = 0x6a09e667f3bcc909ULL;
    int16 i;
    for (i = 0; i < size; i++) {
        h1 += data[i];
        h1 *= ((int64)1<<32) - 1;
        h1 = (h1 << 13) | (h1 >> 51);
        h2 ^= h1;
    }
    memcpy(out,     &h1, 8);
    memcpy(out + 8, &h2, 8);
};

struct bytestream *encrytion(Arc4 *rc4,const struct bytestream *bufin,int16 skipbyte){
    struct bytestream *padding,*bufout;
    int16 totalsize,hashsize,encryptsize;
    int8 hashbuf[16],*rc4envrypted,*p;
  //  printf(" file  size is %d....\n",bufin->size);
    padding=padinit(2);
  //  printf(" paddding size is %d....\n",padding->size);
    hashsize=16;
    totalsize=bufin->size+padding->size+2+hashsize;
    bufout=(struct bytestream*)malloc(sizeof(struct bytestream)+totalsize+1);
    bufout->size=totalsize;
  
    bufout->data =malloc(totalsize + 1);
    p=bufout->data;
    
    memcpy(p,padding->data,padding->size);
   // printf("padding\n");
  //  printhex(p,padding->size);
    
    p+=padding->size;
    memcpy(p,bufin->data,bufin->size);
  //  printf("file\n");
    
  //  printhex(p,bufin->size);
    p+=bufin->size;
    memcpy(p,(int8*)&padding->size,2);
  //  printf("add padsize\n");
  //  printhex(p,2);
   // printf("pad size %d \n",* (int16*)(p));
    p+=2;
    simplehash(bufout->data,padding->size+bufin->size,hashbuf);
  //  printf("hash of ecncrytion \n");
   // printhex(hashbuf,16);
    memcpy(p,hashbuf,16);

    free(padding->data);
    free(padding); 

    //skip some bytes (not ecnrypted)
    p=bufout->data;
  //  printf("bufout  is %d.\n");
   // printhex(bufout->data,bufout->size);
   // printf("skip bytes %d \n", skipbyte);
    p+=skipbyte;
    encryptsize=totalsize - skipbyte;
   // printhex(p,encryptsize);
    rc4envrypted=rc4encrypt(rc4,p,encryptsize);
//    printf("encrypt size is %d \n", encryptsize);
   // printhex(rc4envrypted,encryptsize);
    memcpy(p,rc4envrypted,encryptsize);

  //  printhex(p,encryptsize);
    free(rc4envrypted);
    
    bufout->data[totalsize]=0;
   printf("bufout bytes :\n");
   printhex(bufout->data,bufout->size);
    return bufout;
};
struct bytestream *decrytion(Arc4 *rc4,const struct bytestream *bufin,int16 skipbyte){
    struct bytestream *bufout;
   
    int16 totalsize,hashsize,padsize,filesize,decryptsize   ;
    int8 hashbuf1[16],hashbuf2[16],*p,*s,*rc4decrypted;
    hashsize=16;
  
    
 
    totalsize=bufin->size;
    s=p=(int8*)malloc(totalsize);
    memcpy(p,bufin->data,totalsize);
    printf("bufin bytes\n");
    printhex(p,totalsize);
    p+=skipbyte;
    decryptsize=totalsize - skipbyte;
  //  printf("decryptsize bytes %d....\n", decryptsize);
  //  printhex(p,decryptsize);
    rc4decrypted=rc4encrypt(rc4,p,decryptsize);
    memcpy(p,rc4decrypted,decryptsize);
 //   printhex(p,decryptsize);
    free(rc4decrypted);
    p=s;
  //  printhex(p,totalsize);
    p+=totalsize-hashsize;
    memcpy(hashbuf1,p,hashsize);
 //   printf("data hash of decrytion \n");
   // printhex(hashbuf1,16);  
   // printf(" hash %s \n",(char*)hashbuf1);
    p-=2;
    padsize=(int16)*p;

 //   printf(" the pad size is %d bytes....\n",padsize);
    filesize=totalsize - hashsize -2 - padsize;
    p=s;
  //  printf("the file size is %d bytes.\n",filesize);
    simplehash(p,padsize+filesize,hashbuf2);
   // printf("hash of decrytion \n");
  //  printhex(hashbuf2,16);
   
    if (memcmp(hashbuf1,hashbuf2,hashsize)!=0){
        fprintf(stderr,"error: data integrity check failed.\n");
      

        return NULL;
    };
    p=s;
    p+=padsize;
    
    bufout=(struct bytestream*)malloc(sizeof(struct bytestream));
    bufout->size=filesize;
    bufout->data =(int8*)malloc(filesize+1);
    memcpy(bufout->data,p,filesize);
    bufout->data[filesize]=0;
    printf("decrypted file bytes :\n");
    printhex(bufout->data,filesize);
    free(s);
    
    

    return bufout;
};
int main(int argc,char*argv[]){
    Arc4 *rc4;
    char *infile,*outfile;
    int infd,outfd;
    int8 *keya,*keyb;
    struct bytestream *bufin,*bufout;
    int16 keysize,filesize,skipsize;
    int n;
  


    

    if (argc<3)
    {
        fprintf(stderr,"usage: %s <infile> <outfile>\n",*argv);
        return -1;
    };
 
    infile=argv[1];
    outfile=argv[2];
    infd=open(infile,O_RDONLY);
    if (infd<1){
         fprintf(stderr,"open() inflie error\n");
         return -1;
    };
    filesize=lseek(infd, 0, SEEK_END);
   if (filesize <= 0){
     fprintf(stderr,"lseek() inflie error\n");
        close(infd);
         return -2;
   };
    printf("the file size is %d bytes\n",filesize);  
    bufin=(struct bytestream*)malloc(sizeof(struct bytestream)+filesize+1);
    bufin->size=filesize;
    bufin->data = (int8 *)(bufin + 1);
    lseek(infd, 0, SEEK_SET);
    n = read(infd,bufin->data,filesize);
    if (n < 0 || n != filesize){
     fprintf(stderr,"read() inflie error\n");
         close(infd);
         free(bufin);
         return -3;
    };
    printf("the file read return  is %d\n",n);
     bufin->data[filesize]=0;

   
    
    
    printf("buf size:%d,buf data: %s\n",bufin->size,(char*)bufin->data);
 
    keya=(char*)readkey("Key:");
    //keya="thisiskey"; 
    assert(keya);
    printf("key is %s\n",keya);
    keysize=(int16)strlen((char*)keya);
    //encrypt
    printf("encrypting....\n");
    rc4=rc4init(keya,keysize);
    assert(rc4);
     memset(keya, 0, keysize);  // Wipe key from memory
    // free(keya);
    skipsize=16;
    
     
    bufout=encrytion(rc4,bufin,skipsize);
    free(bufin);
    
  
    //write outfile 
          
   
  
    printf("save encrypted file....\n");
     outfd=open(outfile,O_WRONLY | O_CREAT | O_TRUNC,0600);
    if (outfd<1){
        close(infd);
        fprintf(stderr,"open() outfile error\n");
        return -1;
    };
    printf("write %d bytes....\n",bufout->size);
    printhex(bufout->data,bufout->size);
    n=write(outfd,bufout->data,bufout->size);
    if (n<0){
        fprintf(stderr,"write() outfile error\n");
        close(infd);
        close(outfd);
        free(bufout);
        return -2;
    };
    printf("write %d bytes done.\n",n);
    free(bufout);
    close(infd);
    close(outfd);  

    printf("open encrypted file....\n");
    
    infd=open(outfile,O_RDONLY);
    if (infd<1){
         fprintf(stderr,"open() inflie error\n");
         return -1;
    };
    filesize=lseek(infd, 0, SEEK_END);
   if (filesize <= 0){
     fprintf(stderr,"lseek() inflie error\n");
        close(infd);
         return -2;
   };
    bufin=(struct bytestream*)malloc(sizeof(struct bytestream)+filesize);
    bufin->size=filesize;
    bufin->data = (int8 *)(bufin + 1);
    lseek(infd, 0, SEEK_SET);
    n=read(infd, bufin->data, filesize);
    if (n <= 0 || n != filesize){
     fprintf(stderr,"read() inflie error\n");
         close(infd);
         return -3;
    };
   
    keyb=(char*)readkey("Key:");
    //keyab"thisiskey"; 
    assert(keyb);
    printf("key is %s\n",keyb);
    keysize=(int16)strlen((char*)keyb);

  
   // decrypt
    printf("decrypting....\n");
    rc4=rc4init(keyb,keysize);
    assert(rc4);
    memset(keyb, 0, keysize);  // Wipe key from memory
    free(keya);
    free(keyb);
    skipsize=16;
    
    
    bufout=decrytion(rc4,bufin,skipsize);
  
    if(!bufout){
        close(infd);
        return -4;
    }
 
    printf("buf size:%d,*buf data: %s\n",bufout->size,bufout->data);
    free(bufin);
    free(rc4);
    free(bufout);

    return 0;
};
/*ethernet.h */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
 #include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <sys/types.h>
#include <ifaddrs.h>
//common macro
#define TIMEOUT 5
#define packed __attribute__((__packed__))
typedef unsigned char int8;
typedef unsigned short int int16;
typedef unsigned int int32;
typedef unsigned long long int int64;
typedef int8 L4Pkt;

//show macro
#define show(x) _Generic((x),\
      Mac*:showmac((Mac*)(x)),\
      Ether*:showether((int8*)#x,(Ether*)(x)),\
      Ip*:showip((int8*)#x,(Ip*)(x)),\
      Icmp*:showicmp((int8*)#x,(Icmp*)(x)),\
      L4Pkt*:showl4raw((int8*)#x,(L4Pkt*)(x)),\
      default: printf("Type of " #x " is not supported.\n")\
)

//eval macro
#define eval(x) _Generic((x),\
    Ether*:evalether((Ether*)(x)),\
    Ip*: evalip((Ip*)(x)),\
    Icmp*: evalicmp((Icmp*)(x)),\
    L4Pkt*: evalL4raw((L4Pkt*)(x)),\
    default: printf("eval of " #x " is not supported.\n")\
)

// mac macro
#define mkmac(x) _Generic((x),\
    int64: redmacint((int64)(x)),\
    char* : redmacstr((char*)(x)) \
)


#define printerr(x) do { \
        fprintf(stderr, \
        "Usage %s <interface> <src mac> <dst mac> <src ip> <dst ip> <msg>\n",\
        x);\
        return -1;\
    } while(false);

//bytestring struct
struct s_bytestring;
typedef struct s_bytestring ByteString;
typedef ByteString *(byteStringFunction)(ByteString*,ByteString*);
struct s_bytestring {
    int16 size;
    int8 *data;
    byteStringFunction *merge;
}packed;

//PING struct
struct s_ping {
    int16 id;
    int16 seq;
    int8 data[];
} packed;

//ICMP struct
enum e_icmptype {
    unassigned,
    echo,
    echoreply
} packed;
typedef enum e_icmptype IcmpType;
struct s_rawicmp {
    int8 type;
    int8 code;
    int16 checksum;
    int8 data[];  
} packed;
struct s_icmp {
    IcmpType kind:3; 
    int16 size; 
    int8 *data;  
};
typedef struct s_icmp Icmp;
union u_layer4 {
    Icmp *icmppkt;
    L4Pkt *l4pkt;
};
typedef union u_layer4 L4;


//IP struct
enum e_iptype {
    unknown,
    L4icmp,
    L4udp,
    L4tcp,
    L4raw
} packed;
typedef enum e_iptype IpType;
struct s_rawip {
    int8 ihl:4;
    int8 version:4;  
    int8 dscp:6;
    int8 ecn:2; 
    int16 length;    
    int16 id;
    int8 flags:3;
    int16 offset:13;
    int8 ttl;   
    int8 protocol;  
    int16 checksum;
    int32 src;
    int32 dst;
    int8 options[];
} packed;

struct s_ip {
    IpType kind:3;
    int32 src;
    int32 dst;
    int16 id;
    L4 payload;  
};
typedef struct s_ip Ip;






//MAC struct
struct s_mac {
    int64 addr:48;
}packed ;

typedef struct s_mac Mac;

//Ethernet struct
enum e_ethertype{
    unset=0,
    tIP=0x0800,
    tARP=0x0806
   
}packed;
typedef enum e_ethertype ethertype;
struct s_ether {
  ethertype protocol;
  Mac src;
  Mac dst;  
  Ip  *payload;
} packed;
typedef struct s_ether Ether;  
struct s_rawether{
  Mac dst;
  Mac src;
  int16 ethertype;
}packed;



//icmp functions
Icmp *mkicmp(IcmpType,const int8*, int16);
ByteString *evalicmp(Icmp*);
void showicmp(int8*,Icmp*);

ByteString *evalL4raw(L4Pkt*);
void showl4raw(int8*,L4Pkt*);

int16 if2idx(int8*);
//Ip function
int8 *strtoipv4(const char*);
char *ipv4tostr(const int8*);
Ip *mkip(IpType,int8*,int8*,int16,int16*);
ByteString *evalip(Ip*);
int8 sendip(int32,Ip*);

//mac functuon 
int8 *showmac(Mac*);
Mac *redmacstr(const char*);
Mac *redmacint(int64);

//ethernet functuon 
ByteString *evalether(Ether*);
Ether *mkether(ethertype,Mac*,Mac*);
void showether(int8*,Ether*);
int sendether(int32,Ether*);
//memory functions
void zero(int8*, int16);
void copy(int8*, int8*, int16);
int8 compare(int8*,int8*);
int16 checksum(int8*, int16);

//comman functions
int16 endian16(int16);


//bytestring function 
ByteString *mkbytestring(int8*,int16);
static ByteString *byteStringMerge(ByteString*,ByteString*);


//print functions
void printhex(int8*, int16);


// socket function
int32 ipsocketsetup();
int32 ethernetsocketsetup(int8*,Mac*);

//test
int test1();
int test2();
int test3();
int test4(int,char**);
//main
int main(int,char**);

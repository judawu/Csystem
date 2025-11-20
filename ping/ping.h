#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
 #include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define TIMEOUT 5
#define packed __attribute__((__packed__))
typedef unsigned char int8;
typedef unsigned short int int16;
typedef unsigned int int32;
typedef unsigned long long int int64;



#define show(x) _Generic((x),\
    Ip*: showip((int8*)#x,(Ip*)(x)),\
    Icmp*: showicmp((int8*)#x,(Icmp*)(x)),\
    default: printf("Type of " #x " is not supported.\n")\
)

#define eval(x) _Generic((x),\
    Ip*: evalip((Ip*)(x)),\
    Icmp*: evalicmp((Icmp*)(x)),\
    default: printf("eval of " #x " is not supported.\n")\
)
//ICMP
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

//icmp functions
Icmp *mkicmp(IcmpType,const int8*, int16);
int8 *evalicmp(Icmp*);
void showicmp(int8*,Icmp*);

//ip
enum e_iptype {
    unknown,
    L4icmp,
    udp,
    tcp
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
    Icmp *payload;  
};
typedef struct s_ip Ip;

Ip *mkip(IpType,const int8*,const int8*,int16,int16*);
int8 *evalip(Ip*);
void showip(int8*,Ip*);
int8 sendip(int32,Ip*);
Ip *recvip(int32);

//ping 
struct s_ping {
    int16 id;
    int16 seq;
    int8 data[];
} packed;

int8 sendping(int8*,int8*,int16,int16,int8*,int16);
     


//memory functions
void zero(int8*, int16);
void copy(int8*, int8*, int16);
int16 checksum(int8*, int16);
//comman functions
int16 endian16(int16);
//print functions
void printhex(int8*, int16);


// socket
int32 setup();
int test1();
int test2();
int main1(int,char**);
int main(int,char**);

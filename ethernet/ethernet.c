    /*ethernet.c
    sudo ltrace -f ./ethernet

    */
    #include "ethernet.h"

    int16 endian16(int16 x){
        return ( (x<<8) | (x>>8) );
    };

    //memory functuon 
    void copy(int8 *dst, int8 *src, int16 size){
    int16 i;
    int8 *sp, *dp;
    for(dp=dst, sp=src, i=size; i; i--){
        *dp++ = *sp++;
    }
    return;
    };
    void zero(int8 *ptr, int16 size){
    int16 i;
    int8 *p;
    for(p=ptr, i=size; i; i--){
        *p++ = 0;
    }
    return;
    };
    //print function 
    void printhex(int8 *data, int16 size){
    int16 i;
    int8 *p;
    for(p=data, i=0; i<size; i++){
        printf("%02x ", *p++);
    }
    printf("\n");
    return;
    };

   //ByteString functuon 
    ByteString *mkbytestring(int8* data,int16 size){
        ByteString *bytestr;
        int16 n;
        if(!data || !size)
          return NULL;
        n=sizeof(ByteString);
        bytestr=(ByteString *)malloc(n);
        assert(bytestr);
        zero((int8*)bytestr,n);
        bytestr->data=data;
        bytestr->size=size;
        bytestr->merge=&byteStringMerge;
        return bytestr;
    };
    static ByteString *byteStringMerge(ByteString *bs1,ByteString *bs2){
        int16 size;
        int8 *bsptr,*ret;
        if(!bs1 ||!bs2 || (!bs1->size && !bs2->size))
          return NULL;
        if (!bs1->size)
          return bs2;
        if (!bs2->size)
          return bs1;
        size= bs1->size+bs2->size;
        bsptr=(int8*) malloc(size);
        assert(bsptr);
        ret=bsptr;
        zero(bsptr,size);
        copy(bsptr,bs1->data,bs1->size);
        bsptr+=bs1->size;
        copy(bsptr,bs2->data,bs2->size);
        free(bs1->data);
        free(bs2->data);
        free(bs1);
        free(bs2);
        return mkbytestring(ret,size);
    };
     
    //ICMP function
    Icmp *mkicmp(IcmpType kind, const int8 *data, int16 data_len) {
        int16 size;
        Icmp *icmp;
        if(!data || !data_len) {
        return (Icmp *)0;
        }
        size=sizeof(Icmp) + data_len;
        icmp = (Icmp *)malloc((int32) size);
        assert(icmp);
        zero((int8 *) icmp, size);
        icmp->kind = kind;
        icmp->size = data_len;
        icmp->data = (int8 *) data;
    
        return icmp;
    };
    void showicmp(int8 *identifier,Icmp *pkt) {
        struct s_ping *pingptr;
        if(!pkt) {
            return;
        }
    printf("ICMP identifier:(Icmp *)%s = \n{\n", identifier);
    printf("ICMP kind:\t %s\nsize:\t %d bytes\npayload:\t", 
        (pkt->kind == echo) ? "echo" : 
        (pkt->kind == echoreply) ? "echoreply" : "unassigned",
        pkt->size);
        if(pkt->data && pkt->size) {
            printhex(pkt->data, pkt->size);
            if((pkt->kind==echo) || (pkt->kind==echoreply)){
                pingptr=(struct s_ping *)pkt->data;
                printf("ICMP id:\t%d\nICMP seq:\t%d\n",
                endian16(pingptr->id),endian16(pingptr->seq));
            };
        };

        printf("}\n");
        return;
    };
    int16 checksum(int8 *pkt, int16 size){
    int32 sum = 0;
    int16 i,ret;
    int16 *ptr = (int16 *)pkt;

    /*按 16 位累加*/ 
    for(i = size/2; i; i--) sum += *ptr++;

    /*奇数字节处理*/ 
    if(size%2) sum += *((int8 *)ptr);

    /*进位折返*/ 
    while(sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);

    /* 取反 */
    ret = (int16) (~sum);  
    
    return ret; 
};
    ByteString *evalicmp(Icmp* pkt) {
        int8 *rpktptr,*ret;
        int16 size;
        int16 check;
        struct s_rawicmp rawpkt;
        struct s_rawicmp *rawpktptr;
        if(!pkt || !pkt->data || !pkt->size) {
            return NULL;
        }
        // 1. 设置 type/code
        switch(pkt->kind) {
            case echo:
                rawpkt.type = 8;
                rawpkt.code = 0;
                break;
            case echoreply:
                rawpkt.type = 0;
                rawpkt.code = 0;
                break;
            default:
                return NULL;
        };
        rawpkt.checksum = 0;
        // 2. 分配内存（可能 +1 对齐）
        size=sizeof(struct s_rawicmp) + pkt->size;
        if(size%2){
            size++;
        }; // ← FIX：偶数字节对齐
        rpktptr = (int8 *)malloc((int32) size);
        ret=rpktptr;
        assert(rpktptr);   
        zero(rpktptr, size);
        // 3. 复制头部 + 数据
        copy(rpktptr, (int8 *)&rawpkt, sizeof(struct s_rawicmp));
        rpktptr += sizeof(struct s_rawicmp);
        copy(rpktptr, pkt->data, pkt->size);
        // 4. 计算校验和（包含整个包）
        check=checksum(ret,size);
        rawpktptr = (struct s_rawicmp *)ret;
        // 5. 回填校验和
        rawpktptr->checksum = check;
        return mkbytestring(ret,size);

    };

   //IP function
   char *ipv4tostr(const int8* ipaddr) {
    char *buf = malloc(16);
    if (buf) {
       // 直接读取四个字节，注意：ipaddr 是网络字节序（大端）
        snprintf(buf, 16, "%u.%u.%u.%u",
                 (int8)ipaddr[0],
                 (int8)ipaddr[1],
                 (int8)ipaddr[2],
                 (int8)ipaddr[3]);
    }
    return buf;
    };
   int8 *strtoipv4(const char* ipstr) {
    int8* buf = malloc(4);
    if (!buf) return 0;

    int32 a, b, c, d;
    int n = sscanf(ipstr, "%u.%u.%u.%u", &a, &b, &c, &d);
    if (n != 4 || a > 255 || b > 255 || c > 255 || d > 255) {
        free(buf);
        return 0;
    }

    buf[0] = (int8)a;
    buf[1] = (int8)b;
    buf[2] = (int8)c;
    buf[3] = (int8)d;

    return buf;  
}  
   Ip *mkip(IpType kind,const int8 *src,const int8 *dst, int16 id_,int16 *cntptr){
    int16 id;
    Ip *pkt;
    int16 size;
    if(!kind || !src || !dst) {
          return (Ip *)0;
      };
    id = (id_) ? id_ : *cntptr++;
    size=sizeof(Ip);
    pkt = (Ip *)malloc((int32) size);
    assert(pkt);
    zero((int8 *)pkt, size);
    pkt->kind = kind;
    pkt->id = id;
    copy((int8 *)&(pkt->src), (int8 *)src, 4);
    copy((int8 *)&(pkt->dst), (int8 *)dst, 4);   
    if( !pkt->dst) {
        free(pkt);
        return (Ip *)0;
        
    };
    pkt->payload = (Icmp *)0;
    return pkt; 
   };
   ByteString *evalip(Ip *pkt){
     
     int8 *rpktptr,*ret;
     struct s_rawip rawpkt;
     struct s_rawip *rawpktptr;
     int16 check;
     int8 protocol;
     int16 length_le,length_be;
     int8 *icmpptr;
     int16 size;
     ByteString *bs;

     if(!pkt) {
        return NULL;
     };
    protocol=0;
    switch(pkt->kind) {
        case L4icmp:
            protocol = 1; 
            break;
        default:
            return NULL; 
    };
    rawpkt.checksum = 0;
    rawpkt.dscp = 0;
    rawpkt.dst= pkt->dst;
    rawpkt.ecn = 0;
    rawpkt.flags = 0; 
    rawpkt.version = 4; 
    rawpkt.ihl = sizeof(struct s_rawip)/4; 
 
    rawpkt.id = endian16(pkt->id);
    length_le=0;
    if(pkt->payload) {
        length_le=rawpkt.ihl*4+pkt->payload->size + sizeof(struct s_rawicmp);   
        length_be = endian16(length_le);
        rawpkt.length = length_be; 
   }else{
        length_le=rawpkt.ihl*4;  
        rawpkt.length = length_le;    
    };
      
    rawpkt.offset = 0;

    rawpkt.protocol = protocol;
    rawpkt.src = pkt->src;
    rawpkt.ttl = 64;

    // 2. 分配内存
    if (length_le%2) {
        length_le++; 
    };
    size=sizeof(struct s_rawip);
    rpktptr = (int8 *)malloc((int32) length_le);
    assert(rpktptr);  
    ret=rpktptr; 
    zero(rpktptr, length_le);
    // 3. 复制头部 + 数据
    copy(rpktptr, (int8 *)&rawpkt, size);
    rpktptr += size;
    if(pkt->payload) {
        bs = eval(pkt->payload);
        if(bs) {
            icmpptr=bs->data;
            copy(rpktptr, icmpptr, sizeof(struct s_rawicmp)+pkt->payload->size);
            free(icmpptr);
            free(bs);
        };    
    };
    // 4. 计算校验和（仅头部）
    check=checksum(ret, length_le);
    rawpktptr = (struct s_rawip *)ret;
    // 5. 回填校验和
    rawpktptr->checksum = check;
    
    ;
    return mkbytestring(ret,length_le);

   
   };
   void showip(int8 *identifier,Ip *pkt){
    int16 size;
    if(!pkt) {
        return;
    };
    if(pkt->payload){
        size=sizeof(struct s_rawip)+pkt->payload->size;
    }
    else{
         size=sizeof(struct s_rawip);
        };
       
    

    printf("IP identifier:(Ip *)%s = \n{\n", identifier);
    printf("IP size:\t %d bytes\n", size);
    printf("IP kind \t %s\n", (pkt->kind == L4icmp) ? "ICMP" : 
        (pkt->kind == udp) ? "UDP" :(pkt->kind == tcp)? "TCP": "unassigned");
    printf("IP id \t %d\n", pkt->id);
    printf("IP src:\t %s\n", ipv4tostr((int8*)&pkt->src));
    printf("IP dst:\t %s\n", ipv4tostr((int8*)&pkt->dst));
    if(pkt->payload) {
        show(pkt->payload);
    };
    printf("}\n");
    return;     
   };
   /* ssize_t sendto(int sockfd, const void buf[.len], size_t len, int flags,
                      const struct sockaddr *dest_addr, socklen_t addrlen); */
   int8 sendip(int32 s,Ip *pkt){
    int8 *raw;
    int16 size;
    signed int ret;
    struct sockaddr_in  sock;
    ByteString *bs;
    if (!s || !pkt)
          return 0;
    zero((int8*)&sock,sizeof(sock));
    bs=eval(pkt);
    raw=bs->data;
    size=sizeof(struct s_rawip)+sizeof(struct s_rawicmp)+pkt->payload->size;
    sock.sin_addr.s_addr=(in_addr_t)pkt->dst;
    ret=sendto((int)s,raw,(int)size,MSG_DONTWAIT,(const struct sockaddr *)&sock,sizeof(sock));
    if(ret<0)
     return 0;
    else
     return 1;
   };
 

   //MAC function 
   int8 *showmac(Mac *macaddr){
     int8 *macptr;
     int8 a,b,c,d,e,f;
     int64 imac;

     if(!macaddr)
       return NULL;
    
      imac=macaddr->addr & 0x0000ffffffffffff;
      f= (imac&0xff);
      e= (imac>>8 & 0xff);
      d= (imac>>16 & 0xff);
      c= (imac>>24 & 0xff);
      b= (imac>>32 & 0xff);
      a= (imac>>40 & 0xff);
      macptr=(int8 *)malloc(18);
      assert(macptr);
      zero(macptr,17);
      snprintf((char *)macptr,18,"%x:%x:%x:%x:%x:%x",a,b,c,d,e,f);
     return macptr;
   };
Mac *redmacstr(const char* macstr){
   Mac *macptr;
   int64 imac;  

   int a,b,c,d,e,f;
   
   if (!macstr) return NULL;
   if (
       sscanf(macstr, "%2x%2x-%2x%2x-%2x%2x", &a,&b,&c,&d,&e,&f) != 6 &&
       sscanf(macstr, "%2x-%2x-%2x-%2x-%2x-%2x", &a,&b,&c,&d,&e,&f) != 6 &&
       sscanf(macstr, "%2x%2x:%2x%2x:%2x%2x", &a,&b,&c,&d,&e,&f) != 6 &&
       sscanf(macstr, "%2x %2x %2x %2x %2x %2x", &a,&b,&c,&d,&e,&f) != 6 &&
       sscanf(macstr, "%2x%2x.%2x%2x.%2x%2x", &a,&b,&c,&d,&e,&f) != 6 &&
       sscanf(macstr, "%2x.%2x.%2x.%2x.%2x.%2x", &a,&b,&c,&d,&e,&f) != 6 &&
       sscanf(macstr, "%2x%2x %2x%2x %2x%2x", &a,&b,&c,&d,&e,&f) != 6 &&      
       sscanf(macstr, "%2x:%2x:%2x:%2x:%2x:%2x", &a,&b,&c,&d,&e,&f) != 6 && 
        sscanf(macstr, "%2x%2x%2x%2x%2x%2x", &a,&b,&c,&d,&e,&f) != 6 
       ) {
        return NULL;
    }
   if (a > 0xFF || b > 0xFF || c > 0xFF || d > 0xFF || e > 0xFF || f > 0xFF)
        return NULL;
    imac = ((int64)a << 40) |
              ((int64)b << 32) |
              ((int64)c << 24) |
              ((int64)d << 16) |
              ((int64)e <<  8) |
              ((int64)f      );
    macptr= redmacint(imac);
    return macptr;
};
Mac *redmacint(int64 macint){
    Mac *macptr;
    int16 size;
    size=sizeof(struct s_mac);
    macptr=(Mac *)malloc(size);
    assert(macptr);
    macptr->addr= (macint &  0x0000ffffffffffff);
    return macptr;
 };

 //Ethernet function

Ethernet *mkethernet(ethertype protocol,Mac *dst,Mac *src){
    Ethernet *e;
    int16 size;
    if(!src || !dst)
      return NULL;
   size= sizeof(Ethernet);
   e=(Ethernet *)malloc(size);
   assert(e);
   zero((int8*)e,size);
   e->protocol=protocol;
   e->dst=*dst;
   e->src=*src;
   e->payload=(Ip *)0;
   return e;
};
ByteString *evalethernet(Ethernet *e){
    int8 *ret;
    int16 size;
    ByteString *bs1,*bs2;
    struct s_rawethernet *raw;
    if(!e)
     return NULL;
    size=sizeof(struct s_rawethernet);
    ret=(int8 *)malloc(size);
    assert(ret);
    zero((int8*)ret,size);
    raw=(struct s_rawethernet *)ret;
    raw->dst=e->dst;
    raw->src=e->src;
    raw->ethertype=e->protocol;
    bs1= mkbytestring(ret,size);
    if(!bs1)
     return NULL;
    if(!e->payload)
       return bs1;
    bs2=eval(e->payload);
    if(!bs2){
      free(bs1->data);
      free(bs1);
      return NULL;
    };
    return bs1->merge(bs1,bs2);
};
void showethernet(int8 *identifier,Ethernet* e){
    Ip *pkt;
    int16 size;
    if(!e) {
        return;
    };
    size=sizeof(Ethernet);
    printf("Ethernet identifier:(Ethernet *)%s = \n{\n", identifier);
    printf("Ethernet protocol:\t %.04hx\n", e->protocol);
    printf("Ethernet source mac address :\t %s\n", (char*)show(&(e->src)));
    printf("Ethernet destination mac address :\t %s\n", (char*)show(&(e->dst)));
    
    if(e->payload){
        size+=sizeof(Ip);
        
       
        pkt=e->payload;
        if(pkt->payload)
          size+=pkt->payload->size;
        printf("Ethernet packet size :\t %d\n", size);
    
        printf("Ethernet payload:\n");
        show(pkt);
    }else{
        printf("Ethernet packet size :\t %d\n", size);
    };

    printf("}\n");
    return; 
};




//socket function
   int32 ipsocketsetup(){
     int32 s,one;
    signed int tmp;
    struct timeval timeout;
    timeout.tv_sec=TIMEOUT;
    timeout.tv_usec=0;
    one=(int32)1;
    tmp =socket(AF_INET,SOCK_RAW,IPPROTO_ICMP);//IPPROTO_ICMP=1 using sudo ./ping
    if(tmp>2){
        s=(int32)tmp;
    }
    else{
        s=(int32)0;
    };
    setsockopt((int)s,IPPROTO_IP,IP_HDRINCL,(int *)&one,sizeof(int32));
    setsockopt((int)s,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    setsockopt((int)s,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
  
    return s;
 };

int test1(){
    Mac *macaddr;
       char *str,*newstr;
       str= "1a.23.cf.16.2d.be";
       macaddr=redmacstr(str);
       newstr=(char *)showmac(macaddr);
       printf("%s\n",newstr);   
       str= "1a23:cf16:2dbe";
       macaddr=redmacstr(str);
       newstr=(char *)showmac(macaddr);
       printf("%s\n",newstr);
       str= "1a-23-cf-16-2d-be";
       macaddr=redmacstr(str);
       newstr=(char *)showmac(macaddr);
       printf("%s\n",newstr);
       str= "1a43-cfc6-2dbe";
       macaddr=redmacstr(str);
       newstr=(char *)showmac(macaddr);
       printf("%s\n",newstr);
        str= "8a 23 af 16 2d fe";
       macaddr=redmacstr(str);
       newstr=(char *)showmac(macaddr);
       printf("%s\n",newstr);
        str= "ba23 cf16 2d7e";
       macaddr=redmacstr(str);
       newstr=(char *)showmac(macaddr);
       printf("%s\n",newstr);
       str= "da23cfe62dbe";
       macaddr=redmacstr(str);
       newstr=(char *)showmac(macaddr);
       printf("%s\n",newstr);
       str= "ba:43:ff:16:2d:be";
       macaddr=mkmac(str);
       newstr=(char *)show(macaddr);
       printf("%s\n",newstr);
         str= "1a23.cf16.2dbe";
       macaddr=mkmac(str);
       newstr=(char *)show(macaddr);
       printf("%s\n",newstr);

    return 0;


 };
 int test2(){
    ByteString *bs1,*bs2;
    int8 *data1,*data2;
    data1=(int8*)strdup("hello ");
    data2=(int8*)strdup("world");
    bs1=mkbytestring(data1,6);
    printf("bs1 address = %p\n",bs1);
    printf("size: %d bytes\n",bs1->size);
    printf("data: %s\n",(char*)bs1->data);
    bs2=mkbytestring(data2,6);
    printf("bs2 address = %p\n",bs2);
    printf("size: %d bytes\n",bs2->size);
    printf("data: %s\n",(char*)bs2->data);
    bs1= bs1->merge(bs1,bs2);
    if(!bs1){
        printf("error\n");
        return -1;
    };
    printf("merge bs2 to bs1 \n");
    printf("bs1 address = %p\n",bs1);
    printf("size: %d bytes\n",bs1->size);
    printf("data: %s\n",(char*)bs1->data);
    free(bs1);
    return 0;


 };
int test3(){
    Ethernet *e;
    Ip *ippkt;
    Icmp *icmppkt;
    Mac *macsrc,*macdst;
    ByteString *raw;
    int16 cnt;
    int16 *cntptr;
    cnt=5;
    cntptr=&cnt;
    macsrc=mkmac("ba23 cf16 2d7e");
    macdst=mkmac("ea-12-cd-e6-7d-7d");
    e=mkethernet(tIP,macsrc,macdst);
    if(!e)
      {
        printf("error\n");
        return -1;
      };
    //show(e);
    ippkt=mkip(L4icmp,strtoipv4("10.4.0.6"),strtoipv4("8.8.8.8"),0,cntptr);
   
    icmppkt=mkicmp(echo,(int8*)"hello",5);
    ippkt->payload=icmppkt;
    e->payload=ippkt;
    show(e);
    raw=eval(e);
    if(!raw)
      printf("error\n");
    printf("total ethternet packet size is: \t %d\n",raw->size);
    printf("ethternet packet stream in bytes is:\n");
    printhex((int8*)raw->data,raw->size);
    free(raw->data);
    free(raw);
    free(icmppkt);
    free(ippkt);
    free(macdst);
    free(macsrc);
    free(e);
    return 0;


};
  int main(int argc,char *argv[]){
      // test1();
      // test2();
      test3();
       return 0;
    };

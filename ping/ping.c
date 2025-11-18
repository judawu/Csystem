    /*ping.c*/
    #include "ping.h"

    int16 endian16(int16 x){
        return ( (x<<8) | (x>>8) );
    };
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
    void printhex(int8 *data, int16 size){
    int16 i;
    int8 *p;
    for(p=data, i=0; i<size; i++){
        printf("%02x ", *p++);
    }
    printf("\n");
    return;
    };
    int8 *ipv4tostr(const int8* ipaddr) {
    int8* buf = malloc(16);
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
 
   int8 *strtoipv4(const int8* ipstr) {
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
        if(!pkt) {
            return;
        }
    printf("ICMP identifier:(Icmp *)%s = {\n", identifier);
    printf("ICMP kind:\t %s\nsize:\t %d bytes\npayload:\t", 
        (pkt->kind == echo) ? "echo" : 
        (pkt->kind == echoreply) ? "echoreply" : "unassigned",
        pkt->size);
        if(pkt->data && pkt->size) {
            printhex(pkt->data, pkt->size);
        };
        printf("}\n");
        return;
    };
    int16 checksum(int8 *pkt, int16 size){
    int32 sum = 0;
    int16 i,ret;
    int16 *ptr = (int16 *)pkt;

    // 按 16 位累加
    for(i = size/2; i; i--) sum += *ptr++;

    // 奇数字节处理
    if(size%2) sum += *((int8 *)ptr);

    // 进位折返
    while(sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);

    // 取反
    ret = (int16) (~sum);  
    //checksum 使用网络字节序   
    return htons(ret); 
};
    int8 *evalicmp(Icmp* pkt) {
        int8 *rpktptr,*ret;
        int16 size;
        int16 check;
        struct s_rawicmp rawpkt;
        struct s_rawicmp *rawpktptr;
        if(!pkt || !pkt->data || !pkt->size) {
            return (int8 *)0;
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
                return (int8 *)0;
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
        return ret;

    };



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
   int8 *evalip(Ip *pkt){
     int8 *rpktptr,*ret;
     struct s_rawip rawpkt;
     struct s_rawip *rawpktptr;
     int16 check;
     int8 protocol;
     int16 length_le,length_be;
     int8 *icmpptr;
     int16 size;

     if(!pkt) {
        return (int8 *)0;
     };
    protocol=0;
    switch(pkt->kind) {
        case L4icmp:
            protocol = 1; 
            break;
        default:
            return (int8 *)0; 
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
    rawpkt.options[0]=0;
    rawpkt.protocol = protocol;
    rawpkt.src = pkt->src;
    rawpkt.ttl = 250;

    // 2. 分配内存
    if (length_le%2) {
        length_le++; 
    };
    size=sizeof(struct s_rawip);
    rpktptr = (int8 *)malloc((int32) length_le);
    ret=rpktptr;
    assert(rpktptr);   
    zero(rpktptr, length_le);
    // 3. 复制头部 + 数据
    copy(rpktptr, (int8 *)&rawpkt, size);
    rpktptr += size;
    if(pkt->payload) {
        icmpptr = evalicmp(pkt->payload);
        if(icmpptr) {
            copy(rpktptr, icmpptr, sizeof(struct s_rawicmp)+pkt->payload->size);
            free(icmpptr);
        };    
    };
    // 4. 计算校验和（仅头部）
    check=checksum(ret, length_le);
    rawpktptr = (struct s_rawip *)ret;
    // 5. 回填校验和
    rawpktptr->checksum = check;
    return ret;

   
   };
   void showip(int8 *identifier,Ip *pkt){
    if(!pkt) {
        return;
    };
    printf("IP identifier:(Ip *)%s = {\n", identifier);
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

    if (!s || !pkt)
          return 0;
    zero((int8*)&sock,sizeof(sock));
    raw=eval(pkt);
    size=sizeof(struct s_rawip)+sizeof(struct s_rawicmp)+pkt->payload->size;
    sock.sin_addr.s_addr=(in_addr_t)pkt->dst;
    ret=sendto((int)s,raw,(int)size,MSG_DONTWAIT,(const struct sockaddr *)&sock,sizeof(sock));
    if(ret<0)
     return 0;
    else
     return 1;
 };
 
 
 
   int32 setup(){
    int32 s,one;
    signed int tmp;
    one=(int32)1;
    tmp =socket(AF_INET,SOCK_RAW,IPPROTO_ICMP);//IPPROTO_ICMP=1 using sudo ./ping
    if(tmp>2){
        s=(int32)tmp;
    }
    else{
        s=(int32)0;
    };
    tmp=setsockopt((int)s,IPPROTO_IP,IP_HDRINCL,
    (int *)&one,sizeof(int32));
  
    if(tmp)
     return (int32)0;
    return s;
 };



    int main1(int argc, char *argv[]) {
        int8 *str;
        int8 *raw;
        Icmp *icmp_packet;
        Ip  *ip_packet;
        int16 size;
        int16 rnd;
        int8 *srcip,*dstip;
        int8 ret;
        int32 s;
        (void) rnd;
        srand(getpid());
        rnd=rand()%65536;
        

        str= (int8 *) malloc(6);
        assert(str);
        zero(str,(int16) 6);
        copy(str, (int8 *)"hello", 5);
        printf("Created ICMP packet for %s:",str);
        printhex(str, (int16)5);
      
        icmp_packet = mkicmp(echo, str, (int16)5);
        assert(icmp_packet);
      
        raw = evalicmp(icmp_packet);
        assert(raw);
        srcip="10.0.3.238";
        dstip="10.1.2.6";
        ip_packet=mkip(L4icmp,strtoipv4(srcip),strtoipv4(dstip),0,&rnd);
        assert(ip_packet);
        ip_packet->payload=icmp_packet;
        
        printf("Created ICMP raw data:\n");
        raw=evalip(ip_packet);
        size= sizeof(struct s_rawip)+sizeof(struct s_rawicmp)+ip_packet->payload->size;
        show(ip_packet);
        printhex(raw,size);

        s=setup();
        if(!s){
            printf("error\n");
            return -1;
        }
        else{
             printf("socket create s=%d\n",(int)s);
        };
        ret=sendip(s,ip_packet);
        printf("sendip return=%d=%s\n",ret,(ret)?"true":"false");
        close(s);
        free(icmp_packet->data);
        free(icmp_packet);
        free(ip_packet);


        return 0;
    }

    int main2(int argc,char *argv[]){
       int32 s;
       s=setup();
       if(s)
         printf("s=%d\n",(int)s);
       else
         printf("err\n");
       close(s);
       return 0;
    }

    int main(int argc,char *argv[]){
    
       return main1(argc,argv);
    }
    //sudo strace -f ./ping 2>&1 | grep sendto
    // sudo tcpdump -i any -n icmp and src host 10.0.3.238 -vv
    //sudo tcpdump icmp and src 10.0.3.238
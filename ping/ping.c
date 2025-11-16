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

    void showicmp(Icmp *pkt) {
        if(!pkt) {
            return;
        }
    printf("ICMP kind:\t %s\nsize:\t %d\nplayload:\t", 
        (pkt->kind == echo) ? "echo" : 
        (pkt->kind == echoreply) ? "echoreply" : "unassigned",
        pkt->size);
        if(pkt->data && pkt->size) {
            printhex(pkt->data, pkt->size);
        };
        printf("\n");
        return;
    };
    int16 icmpchecksum(int8 *pkt, int16 size){
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
    return endian16(ret);  
};
    int8 *evalicmp(Icmp* pkt) {
        int8 *rpktptr,*ret;
        int16 size;
        int16 checksum;
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
        };
        rpktptr = (int8 *)malloc((int32) size);
        ret=rpktptr;
        assert(rpktptr);   
        zero(rpktptr, size);
        // 3. 复制头部 + 数据
        copy(rpktptr, (int8 *)&rawpkt, sizeof(struct s_rawicmp));
        rpktptr += sizeof(struct s_rawicmp);
        copy(rpktptr, pkt->data, pkt->size);
        // 4. 计算校验和（包含整个包）
        checksum=icmpchecksum(ret,size);
        rawpktptr = (struct s_rawicmp *)ret;
        // 5. 回填校验和
        rawpktptr->checksum = checksum;
        return ret;

    };



   Ip *mkip(IpType kind,const int8 *src,const int8 *dst, int16 id_,int16 *cntptr){
    int16 id;
    Ip *pkt;
    int16 size;
    if(!kind || !src || !dst) {
          return (Ip *)0;
      };
    id = (id_) ? id_ : (*cntptr++);
    size=sizeof(struct s_ip);
    pkt = (Ip *)malloc((int32) size);
    assert(pkt);
    zero((int8 *) pkt, size);
    pkt->kind = kind;
    pkt->id = id;
    copy((int8 *)&(pkt->src), src, sizeof(int32));
    copy((int8 *)&(pkt->dst), dst, sizeof(int32));
    if( !pkt->dst) {
        free(pkt);
        return (Ip *)0;
    };
    return pkt; 



      
   };

    int main(int argc, char *argv[]) {
        int8 *str;
        int8 *raw;
        Icmp *packet;
        int16 size;
        int16 rnd;

        srand(getpid());
        rnd=rand()%65536;

        

        str= (int8 *) malloc(6);
        assert(str);
        zero(str,(int16) 6);
        copy(str, (int8 *)"hello", 5);
        //
        printf("Created ICMP packet for %s:",str);
        printhex(str, (int16)5);
      
        packet = mkicmp(echo, str, (int16)5);
        assert(packet);
        showicmp(packet);
        raw = evalicmp(packet);
        assert(raw);
        size=sizeof(struct s_rawicmp) + packet->size;
        printf("Created ICMP raw data:\n");
        printhex(raw, size);

        free(packet->data);
        free(packet);


        return 0;
    }

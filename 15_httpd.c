/*
httpserver.c
test:
netstat -an | grep LISTEN
ps ux
curl localhost:1234
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define LSITENADDR "0.0.0.0"

/*struct to parse http request*/
struct sHttpRequest
{
    char method[8];
    char url[128]; 
};
typedef struct sHttpRequest httpreq;



/*global*/
char *error;

/*srvInit return 0 on error othwrwisw return a socket fd*/
int srvInit(int port){
    int s;
    struct sockaddr_in srv;
    s=socket(AF_INET,SOCK_STREAM,0);
    if(s<0){
        error="socket() error";
        return 0;
    };
      
    srv.sin_family=AF_INET;
    srv.sin_port=htons(port);
    srv.sin_addr.s_addr=inet_addr(LSITENADDR);

    if(bind(s,(struct sockaddr *)&srv,sizeof(srv)))
    {
      close(s);
       error="bind() error";
      return 0;
    };
    if(listen(s,5)){
        close(s);
        error="listen() error";
    };
    return s;
};
/*return 0 on error otherwise retuen httprequest*/
httpreq *parseHttp(char *str){
  httpreq *req;
  char *p;
  req=malloc(sizeof(httpreq));
  memset(req,0,sizeof(httpreq));
  for (p=str;*p && *p!=' ';p++);
  if(*p==' ')
   *p=0;
  else{
    error="parse_hhtp() NOSPACE error";
    free(req);
    return 0;
  };
  strncpy(req->method,str,7);
  
  for(str=++p;*p && *p!=' ';p++);
  if(*p==' ')
   *p=0;
  else{
    error="parse_hhtp() NOSPACE error";
    free(req);
    return 0;
  };
  strncpy(req->url,str,127);
  return req;

};


/*srvInit return 0 on error othwrwisw return a new client socket fd*/
int cliAccept(int s){
   int c;
   socklen_t addrlen;
   struct sockaddr_in cli;
   addrlen=0;
   memset(&cli,0,sizeof(cli));
   c=accept(s,(struct sockaddr *)&cli,&addrlen);
   if(c<0){
    error="accept() error";
    return 0;
   };
   return c;
 
};
char *cliRead(int c){
    static char  buf[512];
    memset(buf,0,511);
    if(read(c,buf,511)<0){
        error="read() error";
        return 0;
    }else{
        printf("\n\nHTTP Request:\n");
        printf("%s\n",buf);
        return buf;
    };


};
void httpHeader(int c,int code){
    char buf[512];
    int n;
    memset(buf,0,512);
    snprintf(buf,511,
     "HTTP/1.0 %d OK\r\n"
     "Server:httpd.c\r\n"
     "Cache-Control:no-store,no-cache,max-age=0,private\r\n"
     "Content-Lauguage:en\r\n"
     "Expires:-1\r\n",
     code);
    n=strlen(buf);
    write(c,buf,n);
    printf("\n\nHTTP Response:\n");
    write(1,buf,n);
    return;


};
void httpResponse(int c,char *contexttype,char *data){
    char buf[512];
    int n;
    n=strlen(data);
    memset(buf,0,512);
    snprintf(buf,511,
         "Content-Type:%s\r\n"
         "Content-Length:%d\r\n"
         "\r\n"     
         "%s",
        contexttype,
          n,
         data);
    n=strlen(buf);
    write(c,buf,n);
   
    write(1,buf,n);
    return;


};
void cliConn(int s,int c){
    char *cliread;
    httpreq *req;
    char *data;
    data="<HTML><HEAD><meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\">\n"
          "<TITLE>hello world</TITLE></HEAD><BODY>\n"
           "<H1>Hello World!</H1>\n"
           "Welcome to the website!\n"
"          </BODY></HTML>\n";
    printf("client %d connected:\n",c);
    cliread=cliRead(c);
  
    req=parseHttp(cliread);
    /*
    printf("\n\nparse Http handle with request to:\n Method:\t%s\n URL:\t%s\n",
        req->method,req->url);
    */
    if (!strcmp(req->method,"GET") && !strcmp(req->url,"/?test")){
        httpHeader(c,200);
        httpResponse(c,"text/html",data);
    }else{
        httpHeader(c,404);
        httpResponse(c,"text/plain","404 file not found");
  
    };
    free(req);

    printf("\n");

    close(c);
    return;
};
int main(int argc,char *argv[]){
    int s,c;
    char *port;
    char *template;
    char buf[512];
    httpreq *req;
   
    fprintf(stdout,"Example of the client request:\n");
    template=
    "GET / HTTP/1.1\n"
    "Host: 127.0.0.1:1234\n"
    "Connection: keep-alive\n"
    "sec-ch-ua: \"Chromium\";v=\"142\", \"Google Chrome\";v=\"142\", \"Not_A Brand\";v=\"99\"\n"
    "sec-ch-ua-mobile: ?0\n"
    "sec-ch-ua-platform: \"Windows\"\n"
    "Upgrade-Insecure-Requests: 1\n"
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/142.0.0.0 Safari/537.36\n"
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7\n"
    "Sec-Fetch-Site: no\n",0x00;
    memset(buf,0,511);
    strncpy(buf,template,511);
    req=parseHttp(buf);
    printf("Client Request Template:\n%s\n\nparseHttp handle with request to:\n Method:\t%s\n URL:\t%s\n\n",
        template,req->method,req->url);
    free(req);
    printf("Example end\n\n");
    if(argc<2){
        fprintf(stderr,"Usage: %s <listening port>\n",
        argv[0]);
        return -1;
    }
    else 
     port=argv[1];

    s=srvInit(atoi(port));
    if(!s){
        fprintf(stderr,"%s\n",error);
        return -1;
    };
    printf("Listening on %s at port  %s\n",LSITENADDR,port);
    while(1){
        c=cliAccept(s);
        if (!c){
            fprintf(stderr,"%s\n",error);
            continue;
        };
      
        if(!fork()){
            
            cliConn(s,c);
           
            printf("\n");
        };
       
    };
    close(s);

   return -1;
};


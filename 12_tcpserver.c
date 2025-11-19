/*tcpserver.c
netstat  -an | grep LISTEN 
env - telnet localhost 8181
curl -I localhost:8181
*/
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

       
#define PORT 8181

int main() {
  
    int s,c;
    socklen_t addrlen;
    struct sockaddr_in server,client;
    char buffer[1024];
    char *data;

    addrlen=0;
    memset(&server,0,sizeof(server));
    memset(&client,0,sizeof(client));
    server.sin_family = AF_INET;


  
    s=socket(AF_INET, SOCK_STREAM, 0);
    if(s < 0) {
        perror("Socket creation failed");
        return -1;
    }
    server.sin_addr.s_addr = 0; 
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    

    if(bind(s, (struct sockaddr *)&server, sizeof(server)) !=0){
        printf("bind failed");
        close(s);
        return -1;
    };
    
    if(listen(s, 5 !=0)){
        printf("listen failed");
        close(s);
        return -1;
    };
    printf("Server listening on port %d\n", PORT);
     
    c=accept(s, (struct sockaddr *)&client, &addrlen);
     if(c<0){
        printf("accept failed");
        close(s);
        return -1;
    };
    printf("Connection accepted from %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
    printf("Waiting for data...\n");
    memset(buffer,0,sizeof(buffer));    
    read(c, buffer, sizeof(buffer)-1);
    printf("Data received: %s\n", buffer);  
    data="Http/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!"; 
    write(c, data, strlen(data));
    printf("Response sent\n");
    close(c); 
    close(s);
    return 0;
}

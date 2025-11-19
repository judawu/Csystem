/*sockets.c */
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

       
#define HOST "google.com"
#define IP "142.251.46.206" /*google.com*/
#define PORT 80

int main() {
    printf("Connecting to IP: %s on PORT: %d\n", IP, PORT);
    // Socket connection code would go here
    int s;
    struct sockaddr_in sock;
    char buffer[1024] ;
    char *data;
    data= "HEAD / HTTP/1.0\r\n\r\n";
    s=socket(AF_INET, SOCK_STREAM, 0);
    if(s < 0) {
        perror("Socket creation failed");
        return -1;
    }
    sock.sin_addr.s_addr = inet_addr(IP);
    sock.sin_port = htons(PORT);
    sock.sin_family = AF_INET;

    if(connect(s, (struct sockaddr *)&sock, sizeof(sock)) !=0){
        printf("Connection failed");
        close(s);
        return -1;
    };
    printf("Connected successfully to %s (%s) on port %d\n", HOST,IP,PORT);
    printf("Write head:\n%s\n", data); 
    write(s, data, strlen(data));
    read(s, buffer, sizeof(buffer)-1);
    printf("Response:\n%s\n", buffer);  
    close(s);
    return 0;
}

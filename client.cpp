#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

int main()
{
    int clientsocket = socket(AF_INET,SOCK_STREAM,0);

    sockaddr_in  serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons (5000);
    serveraddr.sin_addr.s_addr = INADDR_ANY;

    connect(clientsocket, (struct sockaddr*)&serveraddr, sizeof(serveraddr));

    const char* msg = "Hallo Server!";
    send(clientsocket,msg,strlen(msg),0);

    close(clientsocket);

    return 0;
}
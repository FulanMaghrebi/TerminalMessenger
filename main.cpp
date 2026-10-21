#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

int main()
{
    //Socket erstellen
    int serversocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(5000);
    serveraddr.sin_addr.s_addr = INADDR_ANY;
    
    bind(serversocket, (struct sockaddr*)&serveraddr,
        sizeof(serveraddr));
    
    listen(serversocket,5);

    int clientsocket = accept(serversocket,nullptr,nullptr);

    char buffer[1024] = {0};
    recv(clientsocket, buffer ,sizeof(buffer),0);
    std::cout<<"Nachricht vom Client: "<< buffer << std::endl;
    
    close(serversocket);
    
    return 0;
}
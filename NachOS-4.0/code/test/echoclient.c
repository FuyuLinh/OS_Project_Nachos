#include "syscall.h"
#include "copyright.h"

char* ip = "127.0.0.1";
const int PORT_SERVER = 8080;

int main(void){
    int length, socketid, connect, send;
    char* hello;
    char* buffer;
    length = 0;
    hello = "Hello from client";
    buffer = "";
    socketid = SocketTCP();
    connect = Connect(socketid, "127.0.0.1", 8080);
    while (hello[length] != '\0')
        ++length;
    send = Send(socketid, hello, length);
    //DEBUG(dbgSys, "client finished sending message\n");

    Receive(socketid, buffer, 1024);
    CloseSocket1(socketid);

    Halt();
}
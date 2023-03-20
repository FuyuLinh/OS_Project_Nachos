#include "syscall.h"
#include "copyright.h"
#define maxlen 32

int main(int argc, char **argv){
    char *buf;
    int size;
    Read(buf,size,0);
    Write(buf,size,1);
    Create(buf);
    Halt();
}
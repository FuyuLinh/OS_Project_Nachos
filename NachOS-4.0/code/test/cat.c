#include "syscall.h"

int
main(int argc, char **argv)
{
    char *buffer;
    int size;
    int id = 0;
    char*filename;
    Read(filename,size,0);
    id = Open(filename,0);
    //Write(buffer,size,id);
    Read(buffer,30,id);
    Close(id);
    Write(buffer,size,1);
    Halt();
}

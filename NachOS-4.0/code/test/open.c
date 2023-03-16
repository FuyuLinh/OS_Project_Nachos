#include "syscall.h"

int
main()
{
    char *buffer ="write\n";
    int size=5;
    int id = 0;
    id = Open("khanh.txt",2);
    //Write(buffer,size,id);
    Read(buffer,size,id);
    Close(id);
    Halt();
}

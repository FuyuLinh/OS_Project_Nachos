#include "syscall.h"

int
main()
{
    int id = 0;
    id = Open("text.txt",2);

    Close(id);

    Halt();
    /* not reached */
}

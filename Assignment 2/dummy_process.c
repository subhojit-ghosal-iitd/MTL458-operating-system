#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>

int main(int argc, char** argv)
{
    if(argc != 2){return 1;}
    int timeInSec = atoi(argv[1]);
    for(int i=0 ; i<timeInSec ; i++) {
        sleep(1);
    }

    return 0;
}


#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define RD 0
#define WR 1
int
main(int argc, char *argv[])
{
    int p1[2];
    int p2[2];
    pipe(p1);
    pipe(p2);
    int pid = fork();
    char buf = 'a';
    if(pid < 0)
    {
        fprintf(2,"fork error.\n");
        close(p1[RD]);
        close(p2[RD]);
        close(p1[WR]);
        close(p2[WR]);
        exit(1);
    }
    if(pid == 0)
    {
        close(p1[WR]);
        close(p2[RD]);
        read(p1[RD],&buf,sizeof(char));
        fprintf(1, "%d: received ping\n", getpid());
        write(p2[WR],&buf,sizeof(char));
        exit(0);
        close(p1[RD]);
        close(p2[WR]);
    }
    else
    {
        close(p1[RD]);
        close(p2[WR]);
        write(p1[WR],&buf,sizeof(char));
        read(p2[RD],&buf,sizeof(char));
        fprintf(1, "%d: received pong\n", getpid());
        close(p1[WR]);
        close(p2[RD]);
        exit(0);
    }
}
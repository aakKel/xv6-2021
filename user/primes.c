#include "kernel/types.h"
#include "user/user.h"

#define RD 0
#define WR 1
#define MAXNUM 35

void next(int *);

int main(int argc,char *argv[])
{
    int p[2];
    pipe(p);


    if(fork() == 0)
    {
        next(p);
    }
    else
    {
        close(p[RD]);
        for(int i = 2;i <= MAXNUM; ++i)
        {
            write(p[WR],&i,sizeof(int));
        }
        close(p[WR]);
        wait((int *) 0);

    }
    exit(0);
}

void next(int *p)
{
    int np[2];
    int n ;
    close(p[WR]);
    int read_status = read(p[RD],&n,sizeof(int));

    if(read_status == 0)
        exit(0);

    pipe(np);
    if(fork() == 0)
        next(np);
    else
    {
        close(np[RD]);
        int prime = n;
        printf("prime %d\n", n);
        while(read(p[RD],&n,sizeof(int)) != 0)
            if(n % prime != 0)
                write(np[WR],&n,sizeof(int));
        close(np[WR]);

        wait((int *) 0);

        exit(0);

    }

}
#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"
#define MAX_LEN 100

int main(int argc, char *argv[])
{
    char *command = argv[1];
    char param[MAXARG][MAX_LEN];
    char *m[MAXARG];
    char bf ;

    while(1)
    {
        int cnt = argc - 1;
        memset(param,0,MAXARG * MAX_LEN);
        for(int i = 1;i < argc; ++i)
        {
            strcpy(param[i - 1],argv[i]);
        }

        int cursor = 0;
        int flag = 0;
        int read_res ;
        //from pipe or .. read data to bf and write in param one bite to ..
        while((read_res = read(0,&bf,1)) > 0 && bf != '\n')
        {
            if(bf == ' ' && flag == 1)
            {
                flag = 0;
                cnt ++;
                cursor = 0;
            }
            else if(bf != ' ')
            {
                param[cnt][cursor++] = bf;
                flag = 1;
            }
        }
        if(read_res <= 0) break;

        for(int i = 0;i < MAXARG - 1; ++i)
            m[i] = param[i];

        m[MAXARG - 1] = 0;

        if(fork() == 0)
        {
            exec(command,m);
            exit(0);
        }
        else wait((int*)0);

    }
    exit(0);

}
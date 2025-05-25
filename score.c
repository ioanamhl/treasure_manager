#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

typedef struct
{
    int id;
    char username[32];
    float longi;
    float lat;
    char clue[128];
    int value;
} Treasure;

int scores[100];
char users_scores[100][32];
int users=0;

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("eroarea numar de argumente");
        exit(-1);
    }
    int fd = open(argv[1], O_RDONLY);
    if(fd<0)
    {
        perror("open");
        exit(-1);
    }
    Treasure t;
    while((read(fd,&t,sizeof(Treasure)))>0)
    {
        int aux=0;
        for(int i=0;i<users;i++)
        {
            if(strcmp(users_scores[i],t.username)==0)
            {
                scores[i]+=t.value;
                aux=1;
            }
        }
        if(aux==0)
        {
            strcpy(users_scores[users],t.username);
            scores[users]+=t.value;
            users++;
        }
    }
    for(int i=0;i<users;i++)
    {
        printf("users: %s  |  score: %d\n",users_scores[i],scores[i]);
    }
}

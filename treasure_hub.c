#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

#define CMD_FILE "monitor.txt"
#define TREASURE_FILE "treasures.dat"

typedef struct
{
    int id;
    char username[32];
    float longi;
    float lat;
    char clue[128];
    int value;
} Treasure;

pid_t pid = -1;
int running;

void handle_command()
{
    int fd = open(CMD_FILE, O_RDONLY);
    if (fd < 0)
    {
        perror("open");
        exit(-1);
    }

    char cmd[256];
    read(fd, cmd, 256);
    close(fd);

    char *command = strtok(cmd, " \n");

    if (strcmp(cmd, "list_hunts") == 0)
    {
        DIR *dir = opendir(".");
        struct dirent *d;
        while ((d = readdir(dir)) != NULL)
        {
            if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
                continue;
            struct stat sb;

            if (lstat(d->d_name, &sb) == -1)
            {
                perror("lstat");
                exit(EXIT_FAILURE);
            }

            if (S_ISDIR(sb.st_mode))
            {
                char path[512];
                sprintf(path, "%s/%s", d->d_name, TREASURE_FILE);
                int count = 0;
                int fd1 = open(path, O_RDONLY);
                if (fd1 < 0)
                {
                    perror("open");
                    exit(-1);
                }

                Treasure t;
                while (read(fd1, &t, sizeof(Treasure)) == sizeof(Treasure))
                {
                    count++;
                }
                close(fd1);

                printf("Hunt: %s, Treasures: %d\n", d->d_name, count);
            }
        }
        closedir(dir);
        printf("hub> ");
        fflush(stdout);
    }
    else if (strcmp(command, "list_treasures") == 0)
    {
        char *arg1 = strtok(NULL, " \n");

        char path[512];
        sprintf(path, "%s/%s", arg1, TREASURE_FILE);
        int fd2 = open(path, O_RDONLY);
        if (fd2 < 0)
        {
            perror("open");
            return;
        }
        Treasure t;
        while (read(fd2, &t, sizeof(Treasure)) == sizeof(Treasure))
        {
            printf("id: %d | user: %s \n", t.id, t.username);
        }
        close(fd2);
        printf("hub> ");
        fflush(stdout);
    }
    else if (strcmp(command, "view_treasure") == 0)
    {
        char *hunt = strtok(NULL, " \n");
        char *id_string = strtok(NULL, " \n");

        int id = atoi(id_string);

        char path[1024];
        sprintf(path, "%s/%s", hunt, TREASURE_FILE);
        int fd3 = open(path, O_RDONLY);
        if (fd3 < 0)
        {
            perror("open");
            return;
        }
        Treasure t;
        while (read(fd3, &t, sizeof(Treasure)) == sizeof(Treasure))
        {
            if (t.id == id)
            {
                printf("id: %d | user: %s | lat: %.2f | long: %.2f | value: %d\n", t.id, t.username, t.lat, t.longi, t.value);
                break;
            }
        }
        close(fd3);
        printf("hub> ");
        fflush(stdout);
    }
}

void sigusr1_handler(int sig)
{
    handle_command();
}

void sigterm_handler(int sig)
{
    printf("monitor is stopping\n");
    usleep(100000);
    exit(0);
}

void sigchld_handler(int sig)
{
    wait(NULL);
    running = 0;
    printf("monitor terminated\n");
}

void write_to_file(const char *cmd)
{
    int fd = open(CMD_FILE, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (fd < 0)
    {
        perror("open");
        exit(-1);
    }
    char command[256];
    sprintf(command, "%s\n", cmd);
    write(fd, command, strlen(command));
    close(fd);
    kill(pid, SIGUSR1);
}

int main()
{
    char input[128];

    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = 0;
    sigaction(SIGCHLD, &sa, NULL);

    while (1)
    {
        printf("hub> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL)
            break;
        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "start_monitor") == 0)
        {
            if (running)
            {
                printf("monitor already running\n");
                continue;
            }
            pid = fork();
            if (pid < 0)
            {
                perror("fork");
                exit(-1);
            }
            else if (pid == 0)
            {
                struct sigaction sig;
                memset(&sig, 0, sizeof(struct sigaction));
                sig.sa_handler = sigusr1_handler;
                sig.sa_flags = 0;
                sigaction(SIGUSR1, &sig, NULL);

                struct sigaction sig_term;
                memset(&sig_term, 0, sizeof(struct sigaction));
                sig_term.sa_handler = sigterm_handler;
                sig_term.sa_flags = 0;
                sigaction(SIGTERM, &sig_term, NULL);

                while (1)
                    pause();
                exit(0);
            }
            else
            {
                running = 1;
                printf("monitor on with PID %d\n", pid);
            }
        }
        else if (!running)
        {
            printf("monitor not running\n");
        }
        else if (strcmp(input, "list_hunts") == 0)
        {
            write_to_file("list_hunts");
        }
        else if (strncmp(input, "list_treasures ", 15) == 0)
        {
            write_to_file(input);
        }
        else if (strncmp(input, "view_treasure ", 14) == 0)
        {
            write_to_file(input);
        }
        else if (strcmp(input, "stop_monitor") == 0)
        {
            kill(pid, SIGTERM);
        }
        else if (strcmp(input, "exit") == 0)
        {
            if (running)
            {
                printf("monitor still running, stop it\n");
            }
            else
            {
                break;
            }
        }
        else
        {
            printf("comanda necunoscuta\n");
        }
    }

    return 0;
}

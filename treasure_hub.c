#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define COM_FILE "command.txt"

pid_t pid;
int running = 0;

void sigchld_handler(int signum)
{
    int status;
    pid_t pid = waitpid(pid, &status, 1);
    if (pid > 0)
    {
        running = 0;
        printf("process ended\n");
    }
}

void send_signal(int signal)
{
    if (running)
    {
        kill(pid, signal);
    }
    else
    {
        printf("not running\n");
    }
}

int main()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    char command[128];
    while (1)
    {
        printf("treasure_hub> ");
        fflush(stdout);
        if (!fgets(command, sizeof(command), stdin))
        {
            break;
        }
        command[strcspn(command, "\n")] = '\0';

        if (strcmp(command, "start_monitor") == 0)
        {
            if (running)
            {
                printf("monitor already running\n");
                continue;
            }
            pid = fork();
            if (pid == 0)
            {
                execl("./monitor", "monitor", NULL);
                perror("execl");
                exit(1);
            }
            else if (pid > 0)
            {
                running = 1;
                printf("monitor started with PID %d.\n", pid);
            }
            else
            {
                perror("fork");
            }
        }
        else if (strcmp(command, "list_hunts") == 0)
        {
            int fd = open(COM_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            write(fd, "LIST_HUNTS", strlen("LIST_HUNTS"));
            close(fd);
            send_signal(SIGUSR1);
        }
        else if (strcmp(command, "list_treasures") == 0)
        {
            int fd = open(COM_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            write(fd, "LIST_TREASURES", strlen("LIST_TREASURES"));
            close(fd);
            send_signal(SIGUSR1);
        }
        else if (strcmp(command, "view_treasure") == 0)
        {
            int fd = open(COM_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            write(fd, "VIEW_TREASURE", strlen("VIEW_TREASURE"));
            close(fd);
            send_signal(SIGUSR1);
        }
        else if (strcmp(command, "stop_monitor") == 0)
        {
            if (!running)
            {
                printf("monitor already stopped\n");
            }
            else
            {
                int fd = open(COM_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                write(fd, "STOP", strlen("STOP"));
                close(fd);
                send_signal(SIGUSR1);
            }
        }
        else if (strcmp(command, "exit") == 0)
        {
            if (running)
            {
                printf("monitor still running\n");
            }
            else
            {
                printf("exiting...\n");
                break;
            }
        }
        else
        {
            printf("unknown command\n");
        }
    }
    return 0;
}

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

int pfd[2];
int cmd_pipe[2];
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
    if (!command)
    {
        return;
    }
    if (strcmp(command, "list_hunts") == 0)
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
                char output[1024];
                sprintf(output, "Hunt: %s, Treasures: %d\n", d->d_name, count);
                write(pfd[1], output, strlen(output));
            }
        }
        closedir(dir);
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
            char output[1024];
            sprintf(output, "id: %d | user: %s \n", t.id, t.username);
            write(pfd[1], output, strlen(output));
        }
        close(fd2);
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
                char output[1024];
                sprintf(output, "id: %d | user: %s | lat: %.2f | long: %.2f | value: %d\n", t.id, t.username, t.lat, t.longi, t.value);
                write(pfd[1], output, strlen(output));
                break;
            }
        }
        close(fd3);
    }
    else if (strcmp(command, "calculate_score") == 0)
    {
        DIR *dir = opendir(".");
        if (!dir)
        {
            perror("opendir");
            return;
        }
        struct dirent *d;
        while ((d = readdir(dir)) != NULL)
        {
            if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
                continue;
            struct stat sb;
            if (stat(d->d_name, &sb) == -1 || !S_ISDIR(sb.st_mode))
                continue;
            char path[512];
            sprintf(path, "%s/%s", d->d_name, TREASURE_FILE);
            int pfd2[2];
            if (pipe(pfd2) < 0)
            {
                perror("pipe");
                exit(-1);
            }
            pid_t pid2 = fork();
            if (pid2 < 0)
            {
                perror("fork");
                exit(-1);
            }
            if (pid2 == 0)
            {
                close(pfd2[0]);
                dup2(pfd2[1], 1);
                close(pfd2[1]);

                execl("./score", "score", path, NULL);
                perror("execl");
                exit(EXIT_FAILURE);
            }
            else
            {
                close(pfd2[1]);
                char buffer[1024];
                ssize_t r;
                while ((r = read(pfd2[0], buffer, sizeof(buffer) - 1)) > 0)
                {
                    buffer[r] = '\0';
                    char output[2048];
                    snprintf(output, sizeof(output), "hunt %s:\n%s", d->d_name, buffer);
                    write(pfd[1], output, strlen(output));
                }
                close(pfd2[0]);
                waitpid(pid2, NULL, 0);
            }
        }

        closedir(dir);
    }
}

void sigusr1_handler(int sig)
{
    handle_command();
    close(pfd[1]);
    exit(0);
}

void sigterm_handler(int sig)
{
    printf("monitor is stopping\n");
    exit(0);
}
void sigchld_handler(int sig)
{
    int status;
    pid_t child_pid;
    while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        if (child_pid == pid)
        {
            running = 0;
        }
    }
}
void write_to_file(const char *cmd)
{

    if (pipe(pfd) < 0) {
        perror("pipe");
        exit(-1);
    }
    int fd = open(CMD_FILE, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (fd < 0)
    {
        perror("open");
        exit(-1);
    }
    write(fd, cmd, strlen(cmd));
    close(fd);

    if (!running)
    {
        pid = fork();
        if (pid < 0)
        {
            perror("fork");
            return;
        }
        else if (pid == 0)
        {
            struct sigaction sa;
            sa.sa_handler = sigusr1_handler;
            sa.sa_flags = 0;
            sigaction(SIGUSR1, &sa, NULL);

            struct sigaction sa_term;
            sa_term.sa_handler = sigterm_handler;
            sa_term.sa_flags = 0;
            sigaction(SIGTERM, &sa_term, NULL);

            pause();
            exit(0);
        }
        else
        {
            running = 1;
            usleep(10000);
        }
    }

    kill(pid, SIGUSR1);

    close(pfd[1]);
    char buff[1024];
    ssize_t n;
    while ((n = read(pfd[0], buff, sizeof(buff) - 1)) > 0)
    {
        buff[n] = '\0';
        printf("%s", buff);
    }
    close(pfd[0]);
}
int main()
{

    struct sigaction sig;
    memset(&sig, 0, sizeof(struct sigaction));
    sig.sa_handler = sigchld_handler;
    sig.sa_flags = SA_NOCLDSTOP | SA_RESTART;
    sigaction(SIGCHLD, &sig, NULL);

    while (1)
    {
        printf("hub> ");
        fflush(stdout);
        char input[128];
        if (!fgets(input, sizeof(input), stdin))
            break;
        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "start_monitor") == 0)
        {
            if (running)
            {
                printf("monitor already running\n");
                continue;
            }
            else
            {
                
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
                }
                else
                {
                    running = 1;
                    printf("monitor on with PID %d\n", pid);
                }
            }
        }
        else if (strcmp(input, "stop_monitor") == 0)
        {
          
                kill(pid, SIGTERM);
                int status;
                waitpid(pid, &status, 0);
                running = 0;
                printf("monitor stopped\n");
        
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
        else if (strncmp(input, "calculate_score", 15) == 0)
        {
            write_to_file(input);
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

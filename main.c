#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>

#define TREASURE_FILE "treasures.dat"
#define LOG_FILE "logged_hunt.txt"

typedef struct
{
    int id;
    char username[32];
    float longi;
    float lat;
    char clue[128];
    int value;
} Treasure;

void log_action(char *hunt_id, char *operation, int id)
{
    char log_file[256];
    sprintf(log_file, "%s/%s", hunt_id, LOG_FILE);

    int log_fd = open(log_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd == -1)
    {
        perror("log open");
        exit(-1);
    }

    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0';

    char log_message[512];
    sprintf(log_message, "[%s] %s treasure ID %d\n", time_str, operation, id);

    write(log_fd, log_message, strlen(log_message));
    close(log_fd);
}

void update_symlink(char *hunt_id)
{
    char symlink_name[256];
    sprintf(symlink_name, "logged_hunt-%s", hunt_id);

    char log_path[256];
    sprintf(log_path, "%s/%s", hunt_id, LOG_FILE);

    unlink(symlink_name);
    if (symlink(log_path, symlink_name) == -1)
    {
        perror("symlink");
    }
}

void add_treasure(char *hunt_id)
{
    struct stat sb;
    if (stat(hunt_id, &sb) == -1)
    {
        if (mkdir(hunt_id, 0755) == -1)
        {
            perror("mkdir");
            exit(-1);
        }
    }

    Treasure t;
    printf("enter treasure ID: ");
    scanf("%d", &t.id);
    getchar();

    printf("enter username: ");
    fgets(t.username, 32, stdin);
    t.username[strcspn(t.username, "\n")] = 0;

    printf("enter longitude and latitude: ");
    scanf("%f %f", &t.longi, &t.lat);
    getchar();

    printf("enter clue: ");
    fgets(t.clue, 128, stdin);
    t.clue[strcspn(t.clue, "\n")] = 0;

    printf("Enter value: ");
    scanf("%d", &t.value);

    char path[256];
    sprintf(path, "%s/%s", hunt_id, TREASURE_FILE);

    int fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd == -1)
    {
        perror("open");
        exit(-1);
    }

    if (write(fd, &t, sizeof(Treasure)) <0)
    {
        perror("write");
        close(fd);
        exit(-1);
    }
    close(fd);

    log_action(hunt_id, "ADD", t.id);
    update_symlink(hunt_id);
}

void list_treasures(const char *hunt_id)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", hunt_id, TREASURE_FILE);

    struct stat sb;
    if (stat(path, &sb) == -1)
    {
        perror("stat");
        exit(-1);
    }

    printf("hunt: %s\n", hunt_id);
    printf("size: %ld bytes\n", sb.st_size);
    printf("last modified: %s", ctime(&sb.st_mtime));

    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        perror("open");
        exit(-1);
    }

    Treasure t;
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure))
    {
        printf("id: %d | user: %s | lat: %.2f | long: %.2f | value: %d\n", t.id, t.username, t.lat, t.longi, t.value);
    }
    close(fd);
}
void view_treasures(char *hunt_id, int id)
{
    char path[256];
    sprintf(path, "%s/%s", hunt_id, TREASURE_FILE);

    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        perror("open");
        exit(-1);
    }
    Treasure tr;
    int ok = 1;
    while ((read(fd, &tr, sizeof(Treasure))) > 0 && ok == 1)
    {
        if (tr.id == id)
        {
            printf("id: %d | user: %s | lat: %.2f | long: %.2f | value: %d\n", tr.id, tr.username, tr.lat, tr.longi, tr.value);
            ok = 0;
        }
    }
}
void remove_treasure(char *hunt_id, int id)
{
    char path[256];
    sprintf(path, "%s/%s", hunt_id, TREASURE_FILE);

    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        perror("open");
        exit(-1);
    }

    char temp_path[256];
    sprintf(temp_path, "%s/temp.dat", hunt_id);

    int temp_fd = open(temp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (temp_fd < 0)
    {
        perror("open");
        exit(-1);
    }

    Treasure t;
    int ok = 0;
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure))
    {
        if (t.id == id)
        {
            ok = 1;
            continue;
        }
        write(temp_fd, &t, sizeof(Treasure));
    }

    close(fd);
    close(temp_fd);

    if (ok)
    {
        if (rename(temp_path, path) != 0)
        {
            perror("rename");
        }
        log_action(hunt_id, "REMOVE", id);
    }
    else
    {
        printf("treasure ID %d not found\n", id);
        unlink(temp_path);
    }
}
void remove_hunt(char *hunt_id)
{
    DIR *dir = opendir(hunt_id);
    if (!dir)
    {
        perror("opendir");
        exit(-1);
    }

    struct dirent *d;
    char file[512];

    while ((d = readdir(dir)) != NULL)
    {
        if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
            continue;

        sprintf(file, "%s/%s", hunt_id, d->d_name);
        if (unlink(file) != 0)
        {
            perror("unlink");
        }
    }

    closedir(dir);

    if (rmdir(hunt_id) != 0)
    {
        perror("rmdir");
    }

    char symlink_name[256];
    sprintf(symlink_name, "logged_hunt-%s", hunt_id);
    unlink(symlink_name);
}
int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("eroare numar de armunete");
        exit(-1);
    }

    if (strcmp(argv[1], "--add") == 0)
    {
        add_treasure(argv[2]);
    }
    else if (strcmp(argv[1], "--list") == 0)
    {
        list_treasures(argv[2]);
    }
    else if (strcmp(argv[1], "--view") == 0)
    {
        int id = atoi(argv[3]);
        view_treasures(argv[2], id);
    }
    else if (strcmp(argv[1], "--remove_treasure") == 0)
    {
        int id = atoi(argv[3]);
        remove_treasure(argv[2], id);
    }
    else if (strcmp(argv[1], "--remove_hunt") == 0)
    {
        remove_hunt(argv[2]);
    }
    else
    {
        printf("comanda necunoscuta\n");
    }

    return 0;
}

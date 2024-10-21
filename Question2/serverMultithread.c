#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <fcntl.h>
#include <dirent.h>

#define PORT 8080
#define BUFFER_SIZE 2048
#define MAX_PROCESSES 2

// Structure to store process information
typedef struct
{
    char name[256];
    int pid;
    long long unsigned int user_time;
    long long unsigned int kernel_time;
} ProcessInfo;

// Function to get the top two CPU-consuming processes
void get_top_cpu_processes(ProcessInfo *top_processes)
{
    struct dirent *entry;
    DIR *dp = opendir("/proc");

    if (dp == NULL)
    {
        perror("opendir");
        return;
    }

    char path[1024];
    char stat[4096];
    char buffer[4096];

    ProcessInfo proc;
    long long unsigned int total_time;

    int i;
    for (i = 0; i < MAX_PROCESSES; i++)
    {
        top_processes[i].user_time = 0;
        top_processes[i].kernel_time = 0;
    }

    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_type == DT_DIR && atoi(entry->d_name) > 0)
        {
            sprintf(path, "/proc/%s/stat", entry->d_name);
            int fd = open(path, O_RDONLY);
            if (fd < 0)
            {
                continue;
            }
            read(fd, stat, sizeof(stat));
            sscanf(stat, "%d %s %*c %*d %*d %*d %*d %*d %*d %*d %*u %*u %*u %*u %llu %llu",
                   &proc.pid, proc.name, &proc.user_time, &proc.kernel_time);

            total_time = proc.user_time + proc.kernel_time;
            if (total_time > top_processes[1].user_time + top_processes[1].kernel_time)
            {
                if (total_time > top_processes[0].user_time + top_processes[0].kernel_time)
                {
                    top_processes[1] = top_processes[0];
                    top_processes[0] = proc;
                }
                else
                {
                    top_processes[1] = proc;
                }
            }
            close(fd);
        }
    }

    closedir(dp);
}

// Function to handle each client connection
void *handle_client(void *client_sock_ptr)
{
    int client_sock = *(int *)client_sock_ptr;
    free(client_sock_ptr);

    ProcessInfo top_processes[MAX_PROCESSES];
    get_top_cpu_processes(top_processes);

    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer),
             "Top 2 CPU-consuming processes:\n"
             "1. Name: %s, PID: %d, User Time: %llu, Kernel Time: %llu\n"
             "2. Name: %s, PID: %d, User Time: %llu, Kernel Time: %llu\n",
             top_processes[0].name, top_processes[0].pid,
             top_processes[0].user_time, top_processes[0].kernel_time,
             top_processes[1].name, top_processes[1].pid,
             top_processes[1].user_time, top_processes[1].kernel_time);

    send(client_sock, buffer, strlen(buffer), 0);
    close(client_sock);

    return NULL;
}

int main()
{
    int server_sock, client_sock, *new_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_size = sizeof(struct sockaddr_in);

    // Create socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Prepare server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind the socket
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    listen(server_sock, 5);
    printf("Server listening on port %d...\n", PORT);

    while ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_size)))
    {
        printf("Client connected.\n");

        pthread_t client_thread;
        new_sock = malloc(sizeof(int));
        *new_sock = client_sock;

        if (pthread_create(&client_thread, NULL, handle_client, (void *)new_sock) < 0)
        {
            perror("Thread creation failed");
            exit(EXIT_FAILURE);
        }

        pthread_detach(client_thread); // Detach the thread to clean up after it's done
    }

    if (client_sock < 0)
    {
        perror("Accept failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    close(server_sock);
    return 0;
}

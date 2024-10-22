#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
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

void *handle_client(void *client_sock_ptr)
{
    char buffer[BUFFER_SIZE];
    memset(buffer, '\0', sizeof(buffer));
    int client_sock = *(int *)client_sock_ptr;
    free(client_sock_ptr);

     int read_size = recv(client_sock, buffer, sizeof(buffer), 0);
     if (read_size > 0)
     {
         if (strcmp(buffer, "Request_CPU_Information\n") == 0)
        {
          ProcessInfo top_processes[MAX_PROCESSES];
      get_top_cpu_processes(top_processes);

      snprintf(buffer, sizeof(buffer),
               "Top 2 CPU-consuming processes:\n"
               "1. Name: %s, PID: %d, User Time: %llu, Kernel Time: %llu\n"
               "2. Name: %s, PID: %d, User Time: %llu, Kernel Time: %llu\n",
               top_processes[0].name, top_processes[0].pid,
               top_processes[0].user_time, top_processes[0].kernel_time,
               top_processes[1].name, top_processes[1].pid,
               top_processes[1].user_time, top_processes[1].kernel_time);
        }
        else {
          printf("INVALID REQUEST");
        }
     }

    ProcessInfo top_processes[MAX_PROCESSES];
    get_top_cpu_processes(top_processes);

    //snprintf(buffer, sizeof(buffer),
      //       "Top 2 CPU-consuming processes:\n"
        //     "1. Name: %s, PID: %d, User Time: %llu, Kernel Time: %llu\n"
          //   "2. Name: %s, PID: %d, User Time: %llu, Kernel Time: %llu\n",
            // top_processes[0].name, top_processes[0].pid,
             //top_processes[0].user_time, top_processes[0].kernel_time,
             //top_processes[1].name, top_processes[1].pid,
             //top_processes[1].user_time, top_processes[1].kernel_time);

    send(client_sock, buffer, strlen(buffer), 0);
    // close(client_sock);

    return NULL;
}

int main()
{
    int server_fd, new_socket, client_sockets[MAX_CLIENTS], max_sd, sd;
    struct sockaddr_in address;
    fd_set readfds;
    fd_set writefds;
    char buffer[BUFFER_SIZE];

    // Initialize client sockets
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        client_sockets[i] = 0;
    }

    // Create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the address and port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 10) < 0)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Listening on port %d\n", PORT);

    int addrlen = sizeof(address);

    while (1)
    {
        // Clear the socket set
        FD_ZERO(&readfds);

        // Add server socket to set
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        // Add client sockets to set
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            sd = client_sockets[i];
            if (sd > 0)
            {
                FD_SET(sd, &readfds);
            }
            if (sd > max_sd)
            {
                max_sd = sd;
            }
        }

        // Wait for activity on any of the sockets
        FD_ZERO(&writefds);
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            sd = client_sockets[i];
            if (sd > 0)
            {
                FD_SET(sd, &writefds);
            }
        }
        int activity = select(max_sd + 1, &readfds, &writefds, NULL, NULL);
        if ((activity < 0))
        {
            perror("Select error");
        }

        // If something happened on the server socket, it's an incoming connection
        if (FD_ISSET(server_fd, &readfds))
        {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
            {
                perror("Accept failed");
                exit(EXIT_FAILURE);
            }

            printf("New connection, socket fd is %d\n", new_socket);

            // Add new socket to the client_sockets array
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (client_sockets[i] == 0)
                {
                    client_sockets[i] = new_socket;
                    printf("Adding to list of sockets as %d\n", i);
                    break;
                }
            }
        }

        // Check all client sockets
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            sd = client_sockets[i];
            if (FD_ISSET(sd, &writefds))
            {
                // Handle writing to the client (sending message and closing the socket)
                int *new_sock = malloc(sizeof(int));
                *new_sock = sd;
                handle_client((void *)new_sock); // Handle sending message
                client_sockets[i] = 0;           // Remove the socket after handling
            }
        }
    }

    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 2048

// Structure to pass arguments to the thread
typedef struct
{
    int client_id;
    char server_ip[16];
} client_data_t;

// Function to handle each client's connection
void *client_thread(void *arg)
{
    client_data_t *data = (client_data_t *)arg;
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("Socket creation failed");
        pthread_exit(NULL);
    }

    // Server address setup
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, data->server_ip, &server_addr.sin_addr) <= 0)
    {
        perror("Invalid address/Address not supported");
        pthread_exit(NULL);
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        pthread_exit(NULL);
    }

    printf("Client %d connected...\n", data->client_id);
    char message[1024] = "Request_CPU_Information\n";
    //while (1)
    //{
        if (send(sock, message, strlen(message), 0) < 0)
        {
            perror("Send failed");
            return NULL;
        }
        // Receive response from server
        int read_size = recv(sock, buffer, BUFFER_SIZE, 0);
        if (read_size > 0)
        {
            buffer[read_size] = '\0';
            printf("Server response for client %d:\n%s\n", data->client_id, buffer);
            //break;
        }
        else
        {
            printf("No response received for client %d\n", data->client_id);
        }
    //}

    close(sock);
    free(data); // Free allocated memory for client data
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage: %s <server_ip> <n_clients>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *server_ip = argv[1];
    int n_clients = atoi(argv[2]);
    pthread_t threads[n_clients];

    // Create threads for each client
    for (int i = 0; i < n_clients; i++)
    {
        client_data_t *data = malloc(sizeof(client_data_t)); // Allocate memory for each client
        data->client_id = i + 1;
        strncpy(data->server_ip, server_ip, sizeof(data->server_ip) - 1);
        data->server_ip[15] = '\0'; // Ensure null-termination

        if (pthread_create(&threads[i], NULL, client_thread, (void *)data) != 0)
        {
            perror("Failed to create thread");
            free(data); // Free memory if thread creation fails
            exit(EXIT_FAILURE);
        }
        // pthread_detach(threads[i]);
    }

    // Wait for all threads to finish
    for (int i = 0; i < n_clients; i++)
    {
        pthread_join(threads[i], NULL);
    }

    return 0;
}

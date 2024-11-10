// web_server.c
// Jacob Gray
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <dirent.h>

#define LISTEN_BACKLOG 5
#define STATIC_DIR "./static"

// Global Variables
int total_requests = 0;
int total_received_bytes = 0;
int total_sent_bytes = 0;

// Connection Handler
void *handleConnection(void *client_fd_ptr)
{
    int client_fd = *(int *)client_fd_ptr;
    free(client_fd_ptr);
    char buffer[4096];
    int bytes_read;
    char response[4096];

    // Read Request
    bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0)
    {
        close(client_fd);
        return NULL;
    }
    buffer[bytes_read] = '\0';

    // Stats update
    total_requests++;
    total_received_bytes += bytes_read;

    // Parse the request line
    char method[16], path[256], version[16];
    sscanf(buffer, "%s %s %s", method, path, version);

    // Check if the method is GET
    if (strcmp(method, "GET") != 0)
    {
        snprintf(response, sizeof(response), "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n");
        write(client_fd, response, strlen(response));
        close(client_fd);
        return NULL;
    }

    // Static File Handling
    if (strncmp(path, "/static", 7) == 0)
    {
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s%s", STATIC_DIR, path + 7);
        FILE *file = fopen(filepath, "rb");
        if (file == NULL)
        {
            snprintf(response, sizeof(response), "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n");
            write(client_fd, response, strlen(response));
        }
        else
        {
            fseek(file, 0, SEEK_END);
            long filesize = ftell(file);
            fseek(file, 0, SEEK_SET);

            snprintf(response, sizeof(response), "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n", filesize);
            write(client_fd, response, strlen(response));

            char file_buffer[1024];
            int bytes_sent;
            while ((bytes_sent = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0)
            {
                write(client_fd, file_buffer, bytes_sent);
                total_sent_bytes += bytes_sent;
            }
            fclose(file);
        }
    }
    // Stats Handling
    else if (strcmp(path, "/stats") == 0)
    {
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                 "<html><body><h1>Server Stats</h1>"
                 "<p>Total Requests: %d</p>"
                 "<p>Total Received Bytes: %d</p>"
                 "<p>Total Sent Bytes: %d</p>"
                 "</body></html>",
                 total_requests, total_received_bytes, total_sent_bytes);
        write(client_fd, response, strlen(response));
    }
    // Calculation Handling
    else if (strncmp(path, "/calc", 5) == 0)
    {
        int a = 0;
        int b = 0;
        sscanf(path, "/calc?a=%d&b=%d", &a, &b);
        int sum = a + b;
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                 "<html><body><h1>Calculation Result</h1>"
                 "<p>%d + %d = %d</p>"
                 "</body></html>",
                 a, b, sum);
        write(client_fd, response, strlen(response));
    }
    // 404 Handling
    else
    {
        snprintf(response, sizeof(response), "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n");
        write(client_fd, response, strlen(response));
    }

    close(client_fd); // Close Connection
    return NULL;
}

// Main function
int main(int argc, char *argv[])
{
    int port = 80; // Default Port
    int opt;

    // Port Number parsing
    while ((opt = getopt(argc, argv, "p:")) != -1)
    {
        switch (opt)
        {
        case 'p':
            port = atoi(optarg);
            break;
        default: // Incorrect input
            fprintf(stderr, "Usage: %s -p <port>\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    // Socket Creation
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Socket Setup
    struct sockaddr_in socket_address;
    memset(&socket_address, '\0', sizeof(socket_address)); // Clear Structure
    socket_address.sin_family = AF_INET;                   // Use IPv4
    socket_address.sin_addr.s_addr = htonl(INADDR_ANY);    // Accept all incoming connections
    socket_address.sin_port = htons(port);                 // Set port num

    // Socket Port Binding
    if (bind(socket_fd, (struct sockaddr *)&socket_address, sizeof(socket_address)) == -1)
    {
        // Error Handling
        perror("bind");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(socket_fd, LISTEN_BACKLOG) == -1)
    {
        // Error Handling
        perror("listen");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    // Setup Confirmation
    printf("Server is listening on port %d\n", port);

    // Main Loop
    while (1)
    {
        struct sockaddr_in client_address; // Client Address
        socklen_t client_address_len = sizeof(client_address);
        int client_fd = accept(socket_fd, (struct sockaddr *)&client_address, &client_address_len);
        if (client_fd == -1)
        {
            // Error Handling
            perror("accept");
            continue;
        }
        printf("Accepted connection from %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

        // Thread Creation for handling connections
        pthread_t thread_id;
        int *client_fd_ptr = malloc(sizeof(int));
        *client_fd_ptr = client_fd;
        pthread_create(&thread_id, NULL, handleConnection, client_fd_ptr); // Create thread
        pthread_detach(thread_id);                                         // Detach thread
    }

    // Close the server socket
    close(socket_fd);
    return 0;
}
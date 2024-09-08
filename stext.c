#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 9079
#define BUFFER_SIZE 1024

// Function prototypes
void save_file(int client_sock, char *filename, char *dest_path);
void delete_file(char *filepath);
void send_file(int client_sock, char *filepath);

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int opt = 1;

    // Create a socket
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options to reuse address and port
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Set up the server address structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind the socket to the address
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_sock, 3) < 0) {
        perror("Listen failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Stext server listening on port %d...\n", PORT);

    // Main loop to accept and handle client connections
    while (1) {
        // Accept a new client connection
        if ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len)) < 0) {
            perror("Accept failed");
            continue;
        }

        char buffer[BUFFER_SIZE];
        ssize_t bytes_read;

        // Read the command and file details from the client
        bytes_read = read(client_sock, buffer, BUFFER_SIZE);
        buffer[bytes_read] = '\0';  // Null-terminate the string

        // Check and handle the command
        if (strncmp(buffer, "ufile", 5) == 0) {
            char filename[BUFFER_SIZE];
            char dest_path[BUFFER_SIZE];
            sscanf(buffer, "ufile %s %s", filename, dest_path);
            save_file(client_sock, filename, dest_path);
        } else if (strncmp(buffer, "rmfile", 6) == 0) {
            char filepath[BUFFER_SIZE];
            sscanf(buffer, "rmfile %s", filepath);
            delete_file(filepath);
        } else if (strncmp(buffer, "dfile", 5) == 0) {
            char filepath[BUFFER_SIZE];
            sscanf(buffer, "dfile %s", filepath);
            send_file(client_sock, filepath);
        } else {
            printf("Unknown command: %s\n", buffer);
        }

        // Close the client socket
        close(client_sock);
    }

    // Close the server socket (unreachable code in this example)
    close(server_sock);
    return 0;
}

// Function to save a file received from the client
void save_file(int client_sock, char *filename, char *dest_path) {
    char filepath[BUFFER_SIZE];
    const char *base_smain = "/home/harmand/Project/smain";
    const char *base_stext = "/home/harmand/Project/stext";

    // Replace base path from "smain" to "stext"
    if (strncmp(dest_path, base_smain, strlen(base_smain)) == 0) {
        snprintf(filepath, BUFFER_SIZE, "%s%s/%s", base_stext, dest_path + strlen(base_smain), filename);
    } else {
        snprintf(filepath, BUFFER_SIZE, "%s/%s", dest_path, filename);
    }

    // Log the file path where the file will be saved
    printf("Saving file to: %s\n", filepath);

    // Extract and create the directory path if it doesn't exist
    char dirpath[BUFFER_SIZE];
    strncpy(dirpath, filepath, strrchr(filepath, '/') - filepath);
    dirpath[strrchr(filepath, '/') - filepath] = '\0';

    // Create the destination directory
    char command[BUFFER_SIZE];
    snprintf(command, BUFFER_SIZE, "mkdir -p %s", dirpath);
    system(command);

    // Open the file for writing
    int file_fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0) {
        perror("Failed to open file for writing");
        return;
    }

    // Notify client that the server is ready to receive the file
    char response[] = "ready";
    send(client_sock, response, strlen(response), 0);

    // Read file data from the client and write it to the file
    ssize_t bytes_read;
    char file_buffer[BUFFER_SIZE];
    while ((bytes_read = read(client_sock, file_buffer, BUFFER_SIZE)) > 0) {
        if (write(file_fd, file_buffer, bytes_read) != bytes_read) {
            perror("Failed to write to file");
            close(file_fd);
            return;
        }
    }
    // Log successful file saving
    printf("File saved successfully: %s\n", filepath);

    close(file_fd);
}

// Function to delete a file
void delete_file(char *filepath) {
    char newPath[256];  // Ensure this is large enough for the new path
    char *replacePos;

    // Replace "smain" with "stext" in the file path
    replacePos = strstr(filepath, "/smain/");
    if (replacePos != NULL) {
        strncpy(newPath, filepath, replacePos - filepath);
        newPath[replacePos - filepath] = '\0';  // Null-terminate the string
        sprintf(newPath + (replacePos - filepath), "/stext%s", replacePos + 6);
    } else {
        strcpy(newPath, filepath);
    }
    // Log the file path being deleted
    printf("Attempting to delete file: %s\n", newPath);
    if (remove(newPath) == 0) {
        printf("File deleted successfully: %s\n", newPath);
    } else {
        perror("Failed to delete file");
    }
}

// Function to send a file to the client
void send_file(int client_sock, char *filepath) {
    char newPath[256];  // Ensure this is large enough for the new path
    char *replacePos;

    // Replace "smain" with "stext" in the file path
    replacePos = strstr(filepath, "/smain/");
    if (replacePos != NULL) {
        strncpy(newPath, filepath, replacePos - filepath);
        newPath[replacePos - filepath] = '\0';  // Null-terminate the string
        sprintf(newPath + (replacePos - filepath), "/stext%s", replacePos + 6);
    } else {
        strcpy(newPath, filepath);
    }
    // Open the file for reading
    int file_fd = open(newPath, O_RDONLY);
    if (file_fd < 0) {
        perror("Failed to open file for reading");
        return;
    }

    // Read the file and send it to the client
    ssize_t bytes_read;
    char file_buffer[BUFFER_SIZE];
    while ((bytes_read = read(file_fd, file_buffer, BUFFER_SIZE)) > 0) {
        if (send(client_sock, file_buffer, bytes_read, 0) != bytes_read) {
            perror("Failed to send file");
            close(file_fd);
            return;
        }
    }

    // Log successful file sending
    printf("File sent successfully: %s\n", filepath);

    close(file_fd);
}

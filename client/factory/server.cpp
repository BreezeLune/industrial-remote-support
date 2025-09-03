#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <vector>
#include <sys/select.h>

int main() {
    int server_fd, soc_socket, qt_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Create socket file descriptor
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8081);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "Server listening on port 8081...\n";

    // Accept soc.cpp client first
    soc_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
    if (soc_socket < 0) {
        perror("accept failed");
        exit(EXIT_FAILURE);
    }
    std::cout << "soc.cpp connected!\n";

    // Accept Qt client next
    qt_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
    if (qt_socket < 0) {
        perror("accept failed");
        exit(EXIT_FAILURE);
    }
    std::cout << "Qt client connected!\n";

    char buffer[1024];
    while (true) {
        int valread = read(soc_socket, buffer, sizeof(buffer)-1);
        if (valread > 0) {
            buffer[valread] = '\0';
            std::cout << "Received from soc.cpp: " << buffer;
            // Forward to Qt client
            send(qt_socket, buffer, strlen(buffer), 0);
        }
    }

    close(soc_socket);
    close(qt_socket);
    close(server_fd);
    return 0;
}
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <vector>
#include <sys/select.h>

int main() {
    int server_fd;
    std::vector<int> client_sockets;
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

    // Accept clients dynamically, start when at least 2 are connected
    int max_clients = 3;
    while (client_sockets.size() < 2) {
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("accept failed");
            exit(EXIT_FAILURE);
        }
        client_sockets.push_back(new_socket);
        std::cout << "Client " << client_sockets.size() << " connected!\n";
    }

    char buffer[1024];
    fd_set readfds;
    while (true) {
        // Si aún no hay 3 clientes, aceptar más
        if (client_sockets.size() < max_clients) {
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 100000; // 0.1 segundos
            FD_ZERO(&readfds);
            FD_SET(server_fd, &readfds);
            int activity = select(server_fd + 1, &readfds, NULL, NULL, &tv);
            if (activity > 0 && FD_ISSET(server_fd, &readfds)) {
                int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
                if (new_socket >= 0) {
                    client_sockets.push_back(new_socket);
                    std::cout << "Client " << client_sockets.size() << " connected!\n";
                }
            }
        }
        // Manejar mensajes de los clientes
        FD_ZERO(&readfds);
        int max_sd = 0;
        for (int sock : client_sockets) {
            FD_SET(sock, &readfds);
            if (sock > max_sd) max_sd = sock;
        }
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("select error");
            break;
        }
        for (size_t i = 0; i < client_sockets.size(); ++i) {
            int sock = client_sockets[i];
            if (FD_ISSET(sock, &readfds)) {
                int valread = read(sock, buffer, sizeof(buffer)-1);
                if (valread > 0) {
                    buffer[valread] = '\0';
                    std::cout << "Received from client " << (i+1) << ": " << buffer;
                    // Forward to other clients
                    for (size_t j = 0; j < client_sockets.size(); ++j) {
                        if (j != i) {
                            send(client_sockets[j], buffer, strlen(buffer), 0);
                        }
                    }
                }
            }
        }
    }

    for (int sock : client_sockets) {
        close(sock);
    }
    close(server_fd);
    return 0;
}
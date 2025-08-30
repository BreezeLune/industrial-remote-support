#include "auth.h"
#include <iostream>

int main() {
    std::string username, password;

    // Get user input
    std::cout << "Enter username: ";
    std::cin >> username;
    std::cout << "Enter password: ";
    std::cin >> password;

    // Authenticate
    bool isAuthenticated = authenticateUser(username, password);

    // Show result
    if (isAuthenticated) {
        std::cout << "Authentication successful!\n";
    } else {
        std::cout << "Authentication failed. Invalid username or password.\n";
    }

    return 0;
}
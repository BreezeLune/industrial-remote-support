#include "auth.h"
#include <iostream>


int main() {
    std::string username, password, token;

    // Test authenticateUser
    std::cout << "Enter username: ";
    std::cin >> username;
    std::cout << "Enter password: ";
    std::cin >> password;

    AuthResult authRes = authenticateUser(username, password);

    if (authRes.ok) {
        std::cout << "Authentication successful!\n";
        std::cout << "userId: " << authRes.userId << "\n";
        std::cout << "role: " << authRes.role << "\n";
        std::cout << "token: " << authRes.token << "\n";
        token = authRes.token;
    } else {
        std::cout << "Authentication failed. Error: " << authRes.error << "\n";
        return 1;
    }

    // Test validateToken
    std::cout << "\nTesting token validation...\n";
    AuthResult tokenRes = validateToken(token);

    if (tokenRes.ok) {
        std::cout << "Token valid!\n";
        std::cout << "userId: " << tokenRes.userId << "\n";
        std::cout << "role: " << tokenRes.role << "\n";
    } else {
        std::cout << "Token invalid. Error: " << tokenRes.error << "\n";
    }
}
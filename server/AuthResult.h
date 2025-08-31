#include <string>

struct AuthResult {
    bool ok = false;
    int userId = 0;
    std::string role;
    std::string token;
    std::string error;
    std::string approved;
    std::string status;
};
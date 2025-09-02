#include <string>
#ifndef AUTHRESULT_H // 如果没有定义 AUTHRESULT_H
#define AUTHRESULT_H // 那么定义 AUTHRESULT_H

struct AuthResult {
    bool ok = false;
    int userId = 0;
    std::string role;
    std::string token;
    std::string error;
    std::string status;
};
#endif

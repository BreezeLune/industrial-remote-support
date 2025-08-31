#ifndef AUTH_H
#define AUTH_H

#include <string>
#include "AuthResult.h"

AuthResult authenticateUser(const std::string& username, const std::string& password);
AuthResult validateToken(const std::string& token);
AuthResult registerUser(const std::string& username, const std::string& password, const std::string& email, const std::string& company, const std::string& requestedRole);
bool isUsernameExists(const std::string& username);
#endif 
#ifndef AUTH_H
#define AUTH_H

#include <string>
#include <AuthResult.cpp>

AuthResult authenticateUser(const std::string& username, const std::string& password);

#endif 
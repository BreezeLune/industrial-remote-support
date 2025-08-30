#ifndef AUTH_H
#define AUTH_H

#include <string>
#include "AuthResult.h"

AuthResult authenticateUser(const std::string& username, const std::string& password);
AuthResult validateToken(const std::string& token);


#endif 
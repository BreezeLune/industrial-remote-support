#ifndef AUTH_H
#define AUTH_H

#include <string>
#include "AuthResult.h"
#include "UserInfo.h"
#include <vector>

AuthResult authenticateUser(const std::string& username, const std::string& password);
AuthResult validateToken(const std::string& token);
AuthResult registerUser(const std::string& username, const std::string& password, const std::string& email, const std::string& company, const std::string& requestedRole);
bool isUsernameExists(const std::string& username);
std::vector<UserInfo> getPendingUser();
std::vector<UserInfo> getAllUsers();
bool approveUser(int userId, int adminId);
bool rejectUser(int userId, int adminId);


#endif 
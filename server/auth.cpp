#include "auth.h"
#include <mysql/mysql.h>
#include <iostream>
#include <cstring>
#include <random>
#include <sstream>
#include <ctime>
#include <bcrypt/BCrypt.hpp>


const char* HOST = "39.106.12.91";
const char* USER = "qtuser";
const char* PASS = "QtPassw0rd!";
const char* DB_NAME = "dongRuanSystem";
const unsigned int PORT = 3306;

std::string hashPassword(const std::string& password) {
    // 生成盐并哈希
    return BCrypt::generateHash(password);
}

bool checkPassword(const std::string& password, const std::string& hash) {
    // 校验密码
    return BCrypt::validatePassword(password, hash);
}

std::string generateToken() {
    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, 15);
    std::stringstream ss;
    for (int i = 0; i < 32; ++i) {
        ss << std::hex << dist(rd);
    }
    return ss.str();
}


bool isUsernameExists(const std::string& username) {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
        return false; // 数据库初始化失败，按接口规范返回 false
    }
    if (!mysql_real_connect(conn, HOST, USER, PASS, DB_NAME, PORT, nullptr, 0)) {
        mysql_close(conn);
        return false; // 数据库连接失败，按接口规范返回 false
    }

    bool exists = false;
    try {
        MYSQL_STMT* stmt = mysql_stmt_init(conn);
        if (!stmt) {
            mysql_close(conn);
            return false;
        }

        const char* query = "SELECT id FROM users WHERE username = ? LIMIT 1";
        if (mysql_stmt_prepare(stmt, query, strlen(query)) != 0) {
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return false;
        }

        MYSQL_BIND bind[1];
        memset(bind, 0, sizeof(bind));
        bind[0].buffer_type = MYSQL_TYPE_STRING;
        bind[0].buffer = (char*)username.c_str();
        bind[0].buffer_length = username.length();

        if (mysql_stmt_bind_param(stmt, bind) != 0) {
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return false;
        }

        if (mysql_stmt_execute(stmt) != 0) {
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return false;
        }

        int userId = -1;
        MYSQL_BIND resultBind[1];
        memset(resultBind, 0, sizeof(resultBind));
        resultBind[0].buffer_type = MYSQL_TYPE_LONG;
        resultBind[0].buffer = (char*)&userId;

        if (mysql_stmt_bind_result(stmt, resultBind) != 0) {
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return false;
        }

        if (mysql_stmt_store_result(stmt) != 0) {
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return false;
        }

        if (mysql_stmt_fetch(stmt) == 0) {
            exists = true; // 查到数据，用户名已存在
        }

        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return exists;
    }
    catch (...) {
        mysql_close(conn);
        return false;
    }
}

AuthResult validateToken(const std::string& token) {
    AuthResult result;
    result.ok = false;
    result.userId = -1;
    result.role = "";
    result.token = "";
    result.error = "";

    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
        result.error = "DB_ERROR";
        return result;
    }
    if (!mysql_real_connect(conn, HOST, USER, PASS, DB_NAME, PORT, nullptr, 0)) {
        mysql_close(conn);
        result.error = "DB_ERROR";
        return result;
    }

    try {
        MYSQL_STMT* stmt = mysql_stmt_init(conn);
        if (!stmt) {
            result.error = "DB_ERROR";
            mysql_close(conn);
            return result;
        }

        const char* query = "SELECT user_id, role, expire_time FROM user_tokens WHERE token = ? LIMIT 1";
        if (mysql_stmt_prepare(stmt, query, strlen(query)) != 0) {
            result.error = "DB_ERROR";
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return result;
        }

        MYSQL_BIND bind[1];
            mysql_close(conn);
        memset(bind, 0, sizeof(bind));
        bind[0].buffer_type = MYSQL_TYPE_STRING;
        bind[0].buffer = (char*)token.c_str();
        bind[0].buffer_length = token.length();

        if (mysql_stmt_bind_param(stmt, bind) != 0) {
            mysql_close(conn);
            result.error = "DB_ERROR";
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return result;
        }

        if (mysql_stmt_execute(stmt) != 0) {
            result.error = "DB_ERROR";
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return result;
        }

        int userId = -1;
        char role[32] = {0};
        char expireTimeStr[20] = {0};
        unsigned long roleLen = 0, expireLen = 0;

        MYSQL_BIND resultBind[3];
        memset(resultBind, 0, sizeof(resultBind));
        resultBind[0].buffer_type = MYSQL_TYPE_LONG;
        resultBind[0].buffer = (char*)&userId;
        resultBind[1].buffer_type = MYSQL_TYPE_STRING;
        resultBind[1].buffer = (char*)role;
        resultBind[1].buffer_length = sizeof(role);
        resultBind[1].length = &roleLen;
        resultBind[2].buffer_type = MYSQL_TYPE_STRING;
        resultBind[2].buffer = (char*)expireTimeStr;
        resultBind[2].buffer_length = sizeof(expireTimeStr);
        resultBind[2].length = &expireLen;

        if (mysql_stmt_bind_result(stmt, resultBind) != 0) {
            result.error = "DB_ERROR";
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return result;
        }

        if (mysql_stmt_store_result(stmt) != 0) {
            result.error = "DB_ERROR";
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return result;
        }

        int fetchRet = mysql_stmt_fetch(stmt);
        if (fetchRet == MYSQL_NO_DATA) {
            result.error = "TOKEN_INVALID";
            mysql_stmt_free_result(stmt);
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return result;
        } else if (fetchRet != 0 && fetchRet != MYSQL_DATA_TRUNCATED) {
            result.error = "DB_ERROR";
            mysql_stmt_free_result(stmt);
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return result;
        }

        // Check expiration
        struct tm tm_expire = {0};
        strptime(expireTimeStr, "%Y-%m-%d %H:%M:%S", &tm_expire);
        time_t expireTime = mktime(&tm_expire);
        time_t now = time(nullptr);

        if (now > expireTime) {
            result.error = "TOKEN_EXPIRED";
            mysql_stmt_free_result(stmt);
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return result;
        }

        // Success
        result.ok = true;
        result.userId = userId;
        result.role = std::string(role, roleLen);

        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return result;
    }
    catch (const std::exception& e) {
        std::cerr << "Token validation error: " << e.what() << std::endl;
        result.error = "DB_ERROR";
        mysql_close(conn);
        return result;
    }
}

AuthResult registerUser(const std::string& username, const std::string& password, const std::string& email, const std::string& company, const std::string& requestedRole) {
    AuthResult result;
    result.ok = false;
    result.userId = -1;
    result.role = "";
    result.token = "";
    result.error = "";
    result.approved = false;
    result.status = "pending";

    // 1. 检查用户名是否已存在
    if (isUsernameExists(username)) {
        result.error = "USERNAME_EXISTS";
        return result;
    }

    // 2. 校验角色是否合法
    if (requestedRole != "admin" && requestedRole != "factory" && requestedRole != "expert" && requestedRole != "auditor") {
        result.error = "INVALID_ROLE";
        return result;
    }

    // 3. 数据库连接
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
        result.error = "DB_ERROR";
        return result;
    }
    if (!mysql_real_connect(conn, HOST, USER, PASS, DB_NAME, PORT, nullptr, 0)) {
        mysql_close(conn);
        result.error = "DB_ERROR";
        return result;
    }

    try {
        // 4. 密码加密
        std::string hashedPwd = hashPassword(password);

        // 5. 插入用户
        MYSQL_STMT* stmt = mysql_stmt_init(conn);
        if (!stmt) {
            result.error = "DB_ERROR";
            mysql_close(conn);
            return result;
        }

        const char* query = "INSERT INTO users (username, password, role, email, company, approved, status) VALUES (?, ?, ?, ?, ?, false, 'pending')";
        if (mysql_stmt_prepare(stmt, query, strlen(query)) != 0) {
            result.error = "DB_ERROR";
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return result;
        }

        MYSQL_BIND bind[5];
        memset(bind, 0, sizeof(bind));
        // username
        bind[0].buffer_type = MYSQL_TYPE_STRING;
        bind[0].buffer = (char*)username.c_str();
        bind[0].buffer_length = username.length();
        // password
        bind[1].buffer_type = MYSQL_TYPE_STRING;
        bind[1].buffer = (char*)hashedPwd.c_str();
        bind[1].buffer_length = hashedPwd.length();
        // role
        bind[2].buffer_type = MYSQL_TYPE_STRING;
        bind[2].buffer = (char*)requestedRole.c_str();
        bind[2].buffer_length = requestedRole.length();
        // email
        bind[3].buffer_type = MYSQL_TYPE_STRING;
        bind[3].buffer = (char*)email.c_str();
        bind[3].buffer_length = email.length();
        // company
        bind[4].buffer_type = MYSQL_TYPE_STRING;
        bind[4].buffer = (char*)company.c_str();
        bind[4].buffer_length = company.length();

        if (mysql_stmt_bind_param(stmt, bind) != 0) {
            result.error = "DB_ERROR";
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return result;
        }

        if (mysql_stmt_execute(stmt) != 0) {
            result.error = "DB_ERROR";
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return result;
        }

        // 6. 获取新用户ID
        int userId = (int)mysql_insert_id(conn);

        result.ok = true;
        result.userId = userId;
        result.role = requestedRole;
        result.approved = false;
        result.status = "pending";

        mysql_stmt_close(stmt);
        mysql_close(conn);
        return result;
    }
    catch (...) {
        result.error = "DB_ERROR";
        mysql_close(conn);
        return result;
    }
}

AuthResult authenticateUser(const std::string& username, const std::string& password) {
    
    AuthResult result;
    result.ok = false;
    result.userId = -1;
    result.role = "";
    result.token = "";
    result.error = "";

    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
        result.error = "DB_ERROR";
        return result;
    }
    if (!mysql_real_connect(conn, HOST, USER, PASS, DB_NAME, PORT, nullptr, 0)) {
        mysql_close(conn);
        result.error = "DB_ERROR";
        return result;
    }

    try {
        // Step 1: Query user info by username
        MYSQL_STMT* stmt = mysql_stmt_init(conn);
        if (!stmt) {
            result.error = "DB_ERROR";
            mysql_close(conn);
            return result;
        }

        const char* query = "SELECT id, role, password FROM users WHERE username = ? LIMIT 1";
        if (mysql_stmt_prepare(stmt, query, strlen(query)) != 0) {
            result.error = "DB_ERROR";
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return result;
        }

        MYSQL_BIND bind[1];
        memset(bind, 0, sizeof(bind));
        bind[0].buffer_type = MYSQL_TYPE_STRING;
        bind[0].buffer = (char*)username.c_str();
        bind[0].buffer_length = username.length();

        if (mysql_stmt_bind_param(stmt, bind) != 0) {
            result.error = "DB_ERROR";
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return result;
        }

        if (mysql_stmt_execute(stmt) != 0) {
            result.error = "DB_ERROR";
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return result;
        }

        // Bind result
        int userId = -1;
        char role[32] = {0};
        char dbPassword[128] = {0};
        unsigned long roleLen = 0, pwdLen = 0;

        MYSQL_BIND resultBind[3];
        memset(resultBind, 0, sizeof(resultBind));
        resultBind[0].buffer_type = MYSQL_TYPE_LONG;
        resultBind[0].buffer = (char*)&userId;
        resultBind[1].buffer_type = MYSQL_TYPE_STRING;
        resultBind[1].buffer = (char*)role;
        resultBind[1].buffer_length = sizeof(role);
        resultBind[1].length = &roleLen;
        resultBind[2].buffer_type = MYSQL_TYPE_STRING;
        resultBind[2].buffer = (char*)dbPassword;
        resultBind[2].buffer_length = sizeof(dbPassword);
        resultBind[2].length = &pwdLen;

        if (mysql_stmt_bind_result(stmt, resultBind) != 0) {
            result.error = "DB_ERROR";
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return result;
        }

        if (mysql_stmt_store_result(stmt) != 0) {
            result.error = "DB_ERROR";
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return result;
        }

        int fetchRet = mysql_stmt_fetch(stmt);
        if (fetchRet == MYSQL_NO_DATA) {
            // Username not found
            result.error = "USER_NOT_FOUND";
            mysql_stmt_free_result(stmt);
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return result;
        } else if (fetchRet != 0 && fetchRet != MYSQL_DATA_TRUNCATED) {
            result.error = "DB_ERROR";
            mysql_stmt_free_result(stmt);
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return result;
        }

        // Check password
        std::string dbPwdStr(dbPassword, pwdLen);
        if (!checkPassword(password, dbPwdStr)) {
            result.error = "BAD_PASSWORD";
            mysql_stmt_free_result(stmt);
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return result;
        }

        // Success: generate token and store in DB
        result.ok = true;
        result.userId = userId;
        result.role = std::string(role, roleLen);

        std::string token = generateToken();
        result.token = token;

        // Expire time: now + 2 hours
        time_t now = time(nullptr);
        time_t expire = now + 2 * 60 * 60;
        struct tm* tm_expire = localtime(&expire);
        char expireStr[20];
        strftime(expireStr, sizeof(expireStr), "%Y-%m-%d %H:%M:%S", tm_expire);

        // Insert token into user_tokens table
        MYSQL_STMT* tokenStmt = mysql_stmt_init(conn);
        const char* tokenQuery = "INSERT INTO user_tokens (token, user_id, role, expire_time) VALUES (?, ?, ?, ?)";
        if (mysql_stmt_prepare(tokenStmt, tokenQuery, strlen(tokenQuery)) == 0) {
            MYSQL_BIND tokenBind[4];
            memset(tokenBind, 0, sizeof(tokenBind));
            // token
            tokenBind[0].buffer_type = MYSQL_TYPE_STRING;
            tokenBind[0].buffer = (char*)token.c_str();
            tokenBind[0].buffer_length = token.length();
            // user_id
            tokenBind[1].buffer_type = MYSQL_TYPE_LONG;
            tokenBind[1].buffer = (char*)&userId;
            // role
            tokenBind[2].buffer_type = MYSQL_TYPE_STRING;
            tokenBind[2].buffer = (char*)result.role.c_str();
            tokenBind[2].buffer_length = result.role.length();
            // expire_time
            tokenBind[3].buffer_type = MYSQL_TYPE_STRING;
            tokenBind[3].buffer = (char*)expireStr;
            tokenBind[3].buffer_length = strlen(expireStr);

            mysql_stmt_bind_param(tokenStmt, tokenBind);
            mysql_stmt_execute(tokenStmt);
            mysql_stmt_close(tokenStmt);
        }

        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return result;
    }
    catch (const std::exception& e) {
        std::cerr << "Authentication error: " << e.what() << std::endl;
        result.error = "DB_ERROR";
        mysql_close(conn);
        return result;
    }
}
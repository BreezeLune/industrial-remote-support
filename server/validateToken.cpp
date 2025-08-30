#include "auth.h"
#include <mysql/mysql.h>
#include <iostream>
#include <cstring>
#include <ctime>

// Database config
const char* HOST = "39.106.12.91";
const char* USER = "qtuser";
const char* PASS = "QtPassw0rd!";
const char* DB_NAME = "dongRuanSystem";
const unsigned int PORT = 3306;

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
        memset(bind, 0, sizeof(bind));
        bind[0].buffer_type = MYSQL_TYPE_STRING;
        bind[0].buffer = (char*)token.c_str();
        bind[0].buffer_length = token.length();

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
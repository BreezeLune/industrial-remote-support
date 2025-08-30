#include "auth.h"
#include <mysql/mysql.h>
#include <iostream>
#include <cstring>

// 数据库连接信息
const char* HOST = "39.106.12.91";
const char* USER = "qtuser";
const char* PASS = "QtPassw0rd!";
const char* DB_NAME = "dongRuanSystem";
const unsigned int PORT = 3306;

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
        // 第一步：查找用户名
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

        // 绑定结果
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
            // 用户名不存在
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

        // 检查密码
        std::string dbPwdStr(dbPassword, pwdLen);
        if (password != dbPwdStr) {
            result.error = "BAD_PASSWORD";
            mysql_stmt_free_result(stmt);
            mysql_stmt_close(stmt);
            mysql_close(conn);
            return result;
        }

        // 登录成功
        result.ok = true;
        result.userId = userId;
        result.role = std::string(role, roleLen);
        // token 生成逻辑后续补充
        result.token = ""; // 先留空

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
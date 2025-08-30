#include "auth.h"
#include <mysql/mysql.h>
#include <iostream>
#include <cstring>


const char* HOST = "39.106.12.91";
const char* USER = "qtuser";
const char* PASS = "QtPassw0rd!";
const char* DB_NAME = "dongRuanSystem";
const unsigned int PORT = 3306; 
bool authenticateUser(const std::string& username, const std::string& password) {
    MYSQL* conn = mysql_init(nullptr);
    bool authSuccess = false;

    if (!conn) {
        std::cerr << "MySQL initialization failed: " << mysql_error(conn) << std::endl;
        return false;
    }

    // Connect to Aliyun MySQL server
    if (!mysql_real_connect(conn, HOST, USER, PASS, DB_NAME, PORT, nullptr, 0)) {
        std::cerr << "Connection to MySQL server failed: " << mysql_error(conn) << std::endl;
        mysql_close(conn);
        return false;
    }

    try {
        // Prepare parameterized query to prevent SQL injection
        MYSQL_STMT* stmt = mysql_stmt_init(conn);
        if (!stmt) {
            throw std::runtime_error("Failed to initialize statement: " + std::string(mysql_error(conn)));
        }

        const char* query = "SELECT 1 FROM users WHERE username = ? AND password = ? LIMIT 1";
        if (mysql_stmt_prepare(stmt, query, strlen(query)) != 0) {
            throw std::runtime_error("Failed to prepare statement: " + std::string(mysql_stmt_error(stmt)));
        }

        // Bind parameters
        MYSQL_BIND bind[2];
        memset(bind, 0, sizeof(bind));

        // Username parameter
        bind[0].buffer_type = MYSQL_TYPE_STRING;
        bind[0].buffer = (char*)username.c_str();
        bind[0].buffer_length = username.length();

        // Password parameter
        bind[1].buffer_type = MYSQL_TYPE_STRING;
        bind[1].buffer = (char*)password.c_str();
        bind[1].buffer_length = password.length();

        if (mysql_stmt_bind_param(stmt, bind) != 0) {
            throw std::runtime_error("Parameter binding failed: " + std::string(mysql_stmt_error(stmt)));
        }

        // Execute query
        if (mysql_stmt_execute(stmt) != 0) {
            throw std::runtime_error("Query execution failed: " + std::string(mysql_stmt_error(stmt)));
        }

        // Store result (fixed line - check return code instead of assigning to MYSQL_RES*)
        if (mysql_stmt_store_result(stmt) != 0) {
            throw std::runtime_error("Failed to store result: " + std::string(mysql_stmt_error(stmt)));
        }

        // Check if any row was returned
        if (mysql_stmt_num_rows(stmt) > 0) {
            authSuccess = true;
        }

        // Cleanup
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
    }
    catch (const std::exception& e) {
        std::cerr << "Authentication error: " << e.what() << std::endl;
    }

    // Close connection
    mysql_close(conn);
    return authSuccess;
}
   
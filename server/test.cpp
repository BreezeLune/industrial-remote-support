#include "auth.h"
#include <iostream>

int main() {
    std::string username, password, email, company, role, token;

    // 测试 isUsernameExists
    std::cout << "请输入要检测的用户名: ";
    std::cin >> username;
    if (isUsernameExists(username)) {
        std::cout << "用户名已存在！\n";
    } else {
        std::cout << "用户名可用。\n";
    }

    // 测试 registerUser
    std::cout << "\n注册新用户...\n";
    std::cout << "用户名: ";
    std::cin >> username;
    std::cout << "密码: ";
    std::cin >> password;
    std::cout << "邮箱(可选): ";
    std::cin >> email;
    std::cout << "公司(可选): ";
    std::cin >> company;
    std::cout << "角色(admin/factory/expert/auditor): ";
    std::cin >> role;

    AuthResult regRes = registerUser(username, password, email, company, role);
    if (regRes.ok) {
        std::cout << "注册成功！userId: " << regRes.userId << ", role: " << regRes.role
                  << ", status: " << regRes.status << "\n";
    } else {
        std::cout << "注册失败，错误码: " << regRes.error << "\n";
    }

    // 测试 authenticateUser
    std::cout << "\n登录测试...\n";
    std::cout << "用户名: ";
    std::cin >> username;
    std::cout << "密码: ";
    std::cin >> password;

    AuthResult authRes = authenticateUser(username, password);
    if (authRes.ok) {
        std::cout << "登录成功！userId: " << authRes.userId << ", role: " << authRes.role
                  << ", token: " << authRes.token << "\n";
        token = authRes.token;
    } else {
        std::cout << "登录失败，错误码: " << authRes.error << "\n";
        return 1;
    }

    // 测试 validateToken
    std::cout << "\nToken 校验测试...\n";
    AuthResult tokenRes = validateToken(token);
    if (tokenRes.ok) {
        std::cout << "Token 有效！userId: " << tokenRes.userId << ", role: " << tokenRes.role << "\n";
    } else {
        std::cout << "Token 无效，错误码: " << tokenRes.error << "\n";
    }

    // 新增：测试 getPendingUser
    std::cout << "\n待审核用户列表:\n";
    std::vector<UserInfo> pendingUsers = getPendingUser();
    for (const auto& user : pendingUsers) {
        std::cout << "ID: " << user.id << ", 用户名: " << user.username
                  << ", 邮箱: " << user.email << ", 公司: " << user.company
                  << ", 角色: " << user.role << ", 状态: " << user.status << "\n";
    }

    // 新增：测试 getAllUsers
    std::cout << "\n所有用户列表:\n";
    std::vector<UserInfo> allUsers = getAllUsers();
    for (const auto& user : allUsers) {
        std::cout << "ID: " << user.id << ", 用户名: " << user.username
                  << ", 邮箱: " << user.email << ", 公司: " << user.company
                  << ", 角色: " << user.role << ", 状态: " << user.status << "\n";
    }

    return
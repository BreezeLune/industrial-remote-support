# 密码加密和数据库依赖安装说明

本项目需要两个主要依赖：

- MySQL C++ 连接库（`#include <mysql/mysql.h>`）
- bcrypt 密码哈希库（`#include <bcrypt/BCrypt.hpp>`）

---

## 一、Ubuntu 安装方法

### 1. 安装 MySQL 开发库

```bash
sudo apt-get update
sudo apt-get install libmysqlclient-dev
```

### 2. 安装 bcrypt 库

推荐使用 [bcrypt-cpp](https://github.com/niXman/bcrypt-cpp)：

```bash
git clone https://github.com/niXman/bcrypt-cpp.git
cd bcrypt-cpp
mkdir build && cd build
cmake ..
make
sudo make install
```


# Makefile修改说明与更新makefile文件后编译使用指南

##  更新makefile后编译使用指南

### 1. 环境准备

#### 安装必要的库
```bash
# 更新包列表
sudo apt-get update

# 安装MySQL客户端开发库
sudo apt-get install -y libmysqlclient-dev

# 安装OpenSSL开发库
sudo apt-get install -y libssl-dev

# 安装编译工具
sudo apt-get install -y build-essential
```

#### 验证安装
```bash
# 检查MySQL库
pkg-config --exists mysqlclient && echo "MySQL客户端库已安装" || echo "MySQL客户端库未找到"

# 检查OpenSSL库
pkg-config --exists openssl && echo "OpenSSL库已安装" || echo "OpenSSL库未找到"
```

### 2. 项目编译

#### 进入项目目录
```bash
cd ~/project/ubuntushare/industrial-remote-support/server
```

#### 编译服务端
```bash
# 清理之前的编译文件
make clean

# 编译服务端
make
```

#### 编译测试文件（使用auth.cpp）
```bash
# 编译测试文件
make auth_test
```

### 3. 运行项目

#### 运行服务端
```bash
# 启动服务器
sudo ./app 0.0.0.0 8080 3 2
```

**参数说明**：
- `0.0.0.0`：监听所有网络接口
- `8080`：端口号
- `3`：最大连接数
- `2`：线程数

#### 运行测试
```bash
# 运行功能测试
./auth_test
```

##  Makefile修改说明

### 1. 文件过滤（解决编译冲突）
```makefile
# 修改前
SRC := $(wildcard *.cpp)

# 修改后
SRC := $(filter-out test.cpp auth.cpp, $(wildcard *.cpp))
```
**原因**：避免 `main.cpp` 和 `test.cpp` 的main函数冲突

### 2. 添加库文件链接
```makefile
# 新增
LIBS := -lmysqlclient -lcrypto -lssl
```
**原因**：项目需要MySQL和OpenSSL库

### 3. 添加测试目标
```makefile
# 新增
auth_test: test.cpp auth.cpp
	$(CC) -std=c++11 -Wall -Wextra $^ -o $@ $(LIBS)
```
**原因**：提供独立的功能测试

### 4. 改进编译选项
```makefile
# 修改前
$(CC) $< -c

# 修改后
$(CC) -std=c++11 -Wall -Wextra $< -c
```
**原因**：使用C++11标准，启用警告信息

### 5. 改进清理命令
```makefile
# 修改前
rm app *.o

# 修改后
rm -f app *.o auth_test
```
**原因**：`-f` 参数避免文件不存在时的错误


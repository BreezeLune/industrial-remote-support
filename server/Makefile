CC := g++
# 排除测试文件和validateToken.cpp，只使用auth.cpp作为正式运行文件
SRC := $(filter-out test.cpp validateToken.cpp, $(wildcard *.cpp))
OBJ := $(SRC:%.cpp=%.o)
HEADER := $(wildcard *.h)

# 库文件设置
LIBS := -lmysqlclient -lcrypto -lssl

# 新增：依赖文件列表（.d文件）
DEPFILES := $(SRC:%.cpp=%.d)

# 新增：编译选项中加入 -MMD 和 -MP 来生成依赖文件
CFLAGS := -std=c++11 -Wall -Wextra -MMD -MP

app: $(OBJ)
	$(CC) -pthread $(OBJ) -o $@ $(addprefix -I, $(HEADER)) $(LIBS)

# 添加测试目标
auth_test: test.cpp auth.cpp
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS) # 更新编译选项

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# 新增：包含所有.d依赖文件
-include $(DEPFILES)

.PHONY: clean

clean:
	rm -f app *.o auth_test $(DEPFILES) # 新增清理.d文件

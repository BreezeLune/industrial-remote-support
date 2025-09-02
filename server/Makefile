CC := g++
# 排除测试文件和validateToken.cpp，只使用auth.cpp作为正式运行文件
SRC := $(filter-out test.cpp validateToken.cpp, $(wildcard *.cpp))
OBJ := $(SRC:%.cpp=%.o)
HEADER := $(wildcard *.h)

# 库文件设置
LIBS := -lmysqlclient -lcrypto -lssl

app:$(OBJ)
	$(CC) -pthread $(OBJ) -o $@ $(addprefix -I, $(HEADER)) $(LIBS)

# 添加测试目标
auth_test: test.cpp auth.cpp
	$(CC) -std=c++11 -Wall -Wextra $^ -o $@ $(LIBS)

%.o:%.cpp
	$(CC) -std=c++11 -Wall -Wextra $< -c

.PHONY:clean

clean:
	rm -f app *.o auth_test 
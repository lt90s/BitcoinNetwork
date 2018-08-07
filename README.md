## Bitcoin Network

#### 目的

1. 加深对bitcoin网络部分代码的理解
2. 加深对bitcoin网络协议的理解
3. 获取参与比特币 P2P 网络的所有IP信息以及网络关系


#### 结构

包含2个部分：

1. 使用C++实现的bitcoin网络部分功能。通过`addr`协议命令获取其他节点的IP地址，并报告
2. 使用tornado实现的一个web服务，用于接受IP上报请求、提供API、可视化等

#### 依赖

1. c++ 部分依赖 `openssl`、`curl`
2. mysql 数据库
3. python3，tornado，pymysql

#### 编译

由于结构很简单，就不用Makefile或者其他工具了（不会了。-_-!!!)

##### Mac

```
g++ -std=c++11 main.cpp init.cpp network.cpp message.cpp addrseed.cpp
-I./include -I/usr/local/Cellar/openssl/1.0.2l/include
-L/usr/local/Cellar/openssl/1.0.2o_1/lib -lcrypto -L/usr/local/lib -lcurl
-O2 -o BitcoinNetwork
```

这是我机器上的，库的路径需要根据实际路径修改

##### Ubuntu

```
g++ -std=c++11 main.cpp init.cpp network.cpp message.cpp addrseed.cpp
-lcrypto -lcurl -lpthread -O2 -o BitcoinNetwork
```

#### 使用方法

1. 在Mysql上创建数据库
 
	```
	create database bitcoin_network default charset utf8mb4
	create user bitcoin@localhost identified by '123456'
	grant all privileges on bitcoin_network.* to bitcoin@localhost
	```
2. 启动web服务
	
	```
	python web/torweb.py
	```
	
3. 编译c++，执行生成的可执行文件
	

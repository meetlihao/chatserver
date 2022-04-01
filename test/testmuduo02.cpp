#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"
#include <functional>
#include <iostream>

using namespace std;
using namespace muduo;
using namespace muduo::net;
using namespace placeholders;

/*
基于muduo网络库开发服务器程序
1. 组合TcpServer对象
2. 创建EventLoop事件循环对象的指针
3. 明确TCPServer构造函数需要什么参数，输出ChatServer的构造函数
4. 在当前服务器类的构造函数中，注册处理连接的回调函数和处理读写事件的回调函数
5. 设置合适的服务端线程数量， muduo库会自己分配io线程和worker线程
*/

class ChatServer
{
public:
    ChatServer(EventLoop *loop, const InetAddress &listenAddr, const string &nameArg)
        : _server(loop, listenAddr, nameArg), _loop(loop)
    {
        //给服务器注册用户连接的创建和断开回调
        _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));

        //给服务器注册用户读写事件的回调
        _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

        //设置服务器端的线程数量  1个io线程  3个    worker线程
        _server.setThreadNum(4);
    }

    //开启事件循环
    void start()
    {
        _server.start();
    }

private:
    //处理两个事件：
    //1. 用户连接和断开
    void onConnection(const TcpConnectionPtr& conn)
    {
        if(conn->connected())
        {
            cout << conn->peerAddress().toIpPort() << " -> " << conn->localAddress().toIpPort() <<" state:online " << endl;
        }
        else
        {
            cout << conn->peerAddress().toIpPort() << " -> " << conn->localAddress().toIpPort() <<" state:offline " << endl;
            conn->shutdown();
        }
    }


    //2. 处理消息到达
    void onMessage(const TcpConnectionPtr &conn,
                   Buffer *buffer,
                   Timestamp time)
    {
        string buf = buffer->retrieveAllAsString();
        cout << time.toString() << "recv date: " << buf << endl;
        conn->send(buf);
    }
    
    TcpServer  _server; 
    EventLoop* _loop;
};

int main()
{
    EventLoop loop;
    InetAddress addr("127.0.0.1", 9999);
    ChatServer server(&loop, addr, "ChatServer");
    server.start();
    loop.loop();
    return 0;
}

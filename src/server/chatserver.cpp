#include "chatserver.hpp"
#include <functional>
#include "json.hpp"
#include "chatservice.hpp"

using namespace std;
using namespace placeholders;
using json = nlohmann::json;

ChatServer::ChatServer(EventLoop* loop,
            const InetAddress& listenAddr,
            const string& nameArg)
            :_server(loop, listenAddr, nameArg), _loop(loop)
{   
    //注册连接/断开回调函数
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));

    //注册消息到达回调函数
    _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

    //设置线程数量
    _server.setThreadNum(4);
}

void ChatServer::start()
{
    _server.start();
}

//上报连接相关信息的回调函数
void ChatServer::onConnection(const TcpConnectionPtr& conn)
{
    if(!conn->connected())
    {
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}

//上报读写事件相关信息的回调函数    
void ChatServer::onMessage(const TcpConnectionPtr& conn, Buffer* buffer, Timestamp time)
{
    string buf = buffer->retrieveAllAsString();

    //数据反序列化
    json js = json::parse(buf);

    //通过ChatService这个类，完全解耦网络模块的代码和业务模块的代码
    //获取该消息类型对应的处理函数  通过js["msgId"]获取 -> 业务handler -> 获取 conn js time
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());

    //调用这个处理函数,从而来执行不同的业务
    msgHandler(conn, js, time);
}

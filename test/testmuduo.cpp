#include "muduo/base/Logging.h"
#include "muduo/net/TcpServer.h"

#include "muduo/net/EventLoop.h"


#include <unistd.h>

using namespace muduo;
using namespace muduo::net;
#include <iostream>
#include <functional>

void onMessage(const muduo::net::TcpConnectionPtr& conn,
                           muduo::net::Buffer* buf,
                           muduo::Timestamp time)
{
  muduo::string msg(buf->retrieveAllAsString());
  LOG_INFO << conn->name() << " echo " << msg.size() << " bytes, "
           << "data received at " << time.toString();
  conn->send(msg);
}

void onConnection(const muduo::net::TcpConnectionPtr& conn)
{
    std::cout << "onMessage" << std::endl;
    LOG_INFO << "EchoServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");
}

int main(int argc, char* argv[])
{
    LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid()  ;
    EventLoop loop;
    InetAddress listenAddr(9981);
    TcpServer server(&loop, listenAddr, "server0/01");
    server.setConnectionCallback(std::bind(onConnection, _1));
    server.setMessageCallback(std::bind(onMessage, _1, _2, _3));
    server.start();
    loop.loop();
    return 0;
}
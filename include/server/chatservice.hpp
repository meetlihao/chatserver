#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include "redis.hpp"
#include <unordered_map>
#include <muduo/net/TcpConnection.h>
#include <functional>
#include <mutex>
#include "usermodel.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"


using namespace std;
using namespace muduo;
using namespace muduo::net;

#include "json.hpp"
using json = nlohmann::json;

//处理消息事件的回调函数的类型
using MsgHandler = std::function<void(const TcpConnectionPtr& conn, json& js, Timestamp time)>;

//聊天服务器业务类
class ChatService
{
public:
    //获取单例对象的接口函数
    static ChatService* instance();

    //获取消息对应的处理函数
    MsgHandler getHandler(int msgId);

    //处理注册业务
    void reg(const TcpConnectionPtr& conn, json& js, Timestamp time);

    //处理登陆业务
    void login(const TcpConnectionPtr& conn, json& js, Timestamp time);

    //处理注销业务
    void logout(const TcpConnectionPtr& conn, json& js, Timestamp time);

    //添加好友业务
    void addFriend(const TcpConnectionPtr& conn, json& js, Timestamp time);

    //处理单人聊天
    void oneChat(const TcpConnectionPtr& conn, json& js, Timestamp time);

    //创建群组业务
    void createGroup(const TcpConnectionPtr& conn, json& js, Timestamp time);

    //加入群组业务
    void addGroup(const TcpConnectionPtr& conn, json& js, Timestamp time);

    //处理群组聊天
    void groupChat(const TcpConnectionPtr& conn, json& js, Timestamp time);

    //处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr&);

    //服务器异常时重置业务
    void reset();

    //从redis消息队列中获取订阅的消息
    void handleRedisSubscribeMessage(int, string);

private:
    ChatService();

    //存储消息类型id和其对应的回调方法
    unordered_map<int, MsgHandler> _msgHandlerMap;

    //定义互斥锁，用于保证_userConnMap的线程安全
    mutex _connMutex;

    //存储在线用户的通信连接  每个用户对应一个tcp连接
    unordered_map<int, TcpConnectionPtr> _userConnMap;

    //数据操作类对象
    UserModel _userModel;
    OfflineMsgModel _offlineMsgModel;
    FriendModel _friendModel;
    GroupModel _groupModel;

    //redis操作对象
    Redis _redis;
};

#endif
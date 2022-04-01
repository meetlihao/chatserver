#include "chatservice.hpp"
#include "public.hpp"
#include "muduo/base/Logging.h"
#include <vector>
using namespace std;
using namespace muduo;

ChatService *ChatService::instance()
{
    static ChatService service;
    return &service;
}

//注册消息及对应的回调操作
ChatService::ChatService()
{
    //用户基本业务
    _msgHandlerMap[REG_MSG] = std::bind(&ChatService::reg, this, _1, _2, _3);
    _msgHandlerMap[LOGIN_MSG] = std::bind(&ChatService::login, this, _1, _2, _3);
    _msgHandlerMap[LOGOUT_MSG] = std::bind(&ChatService::logout, this, _1, _2, _3);
    _msgHandlerMap[ONE_CHAT_MSG] = std::bind(&ChatService::oneChat, this, _1, _2, _3);
    _msgHandlerMap[ADD_FRIEND_MSG] = std::bind(&ChatService::addFriend, this, _1, _2, _3);

    //群组业务
    _msgHandlerMap[CREATE_GROUP_MSG] = std::bind(&ChatService::createGroup, this, _1, _2, _3);
    _msgHandlerMap[ADD_GROUP_MSG] = std::bind(&ChatService::addGroup, this, _1, _2, _3);
    _msgHandlerMap[GROUP_CHAT_MSG] = std::bind(&ChatService::groupChat, this, _1, _2, _3);

    //连接redis服务器
    if(_redis.connect())
    {
        //设置上报消息的回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

//从redis消息队列中获取订阅的消息
 void ChatService::handleRedisSubscribeMessage(int userid, string msg)
 {
     lock_guard<mutex> lock(_connMutex);
     auto it = _userConnMap.find(userid);
     if(it != _userConnMap.end())
     {
         it->second->send(msg);
         return;
     }

    //存储该用户的离线消息
    _offlineMsgModel.insert(userid, msg);
 }

//获取消息对应的处理函数
MsgHandler ChatService::getHandler(int msgId)
{
    //当msgId没有对应的事件处理函数时，返回一个空的处理函数，什么也不做
    if (_msgHandlerMap.find(msgId) == _msgHandlerMap.end())
    {
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp time)
        {
            LOG_ERROR << "msgid : " << msgId << "can't find handler";
        };
    }
    else
    {
        return _msgHandlerMap[msgId];
    }
}

//处理注册业务
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string name = js["name"];
    string pwd = js["password"];
    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state = _userModel.insert(user);
    json response;
    response["msgid"] = REG_MSG_ACK;
    if (state)
    {
        //注册成功
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        //注册失败
        response["errno"] = 1;
        conn->send(response.dump());
    }
} //测试：{"msgid":3,"name":"zhaoyun","password":"123456"}

//处理登陆业务
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js["id"].get<int>();
    string pwd = js["password"];
    User user = _userModel.query(id);
    json response;
    response["msgid"] = LOGIN_MSG_ACK;
    if (user.getId() == id && user.getPwd() == pwd)
    {
        if (user.getState() == "online")
        {
            //若用户已在线，则提示错误：不容许重复登录
            response["errno"] = 2;
            response["errmsg"] = "the account is using, input another";
            conn->send(response.dump());
        }
        else
        {
            //登陆成功
            //记录已连接信息   大括号来改变锁的作用域
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }

            //id用户登录成功后，向redis订阅channel(id)
            _redis.subscribe(id);

            // 1. 并更新用户状态信息
            user.setState("online");
            _userModel.updateState(user);

            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            // 2. 检测该用户是否有离线消息
            vector<string> vec = _offlineMsgModel.query(id);
            if (!vec.empty())
            {
                response["offlinemsg"] = vec;
                //读取完用户的离线消息后，删除该用户的所有离线消息
                _offlineMsgModel.remove(id);
            }

            // 3. 查询该用户的所有好友信息并返回
            vector<User> userVec = _friendModel.query(id);
            if (!userVec.empty())
            {
                vector<string> vec2;
                for (User &user : userVec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }

            // 4. 查询用户的群组信息
            vector<Group> groupuserVec = _groupModel.queryGroups(id);
            if (!groupuserVec.empty())
            {
                vector<string> groupV;
                for (Group &group : groupuserVec)
                {
                    json grpjson;
                    grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for (GroupUser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupV.push_back(grpjson.dump());
                }
                response["groups"] = groupV;
            }

            conn->send(response.dump());
        }
    }
    else
    {
        //登陆失败
        response["errno"] = 1;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errmsg"] = "id or password is invalid !";
        conn->send(response.dump());
    }
} //测试：{"msgId":1,"id":13,"password":"123456"}

//处理注销业务
void ChatService::logout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if(it != _userConnMap.end())
        {
            _userConnMap.erase(it);
        }
    }

    //用户注销，相当于下线，在redis中取消订阅通道
    _redis.unsubscribe(userid);

    //并且要更新数据库中该连接对应的用户的状态
    User user(userid, "", "", "offline");
    _userModel.updateState(user);
 
}

//处理单人聊天
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toid = js["to"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);

        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end())
        {
            //在当前服务器中查找toid用户，找到则说明对方目前在线，转发消息  服务器主动推送消息给toid用户
            it->second->send(js.dump());
            return;
        }
    }

    //若在当前服务器中没找到，则查看toid对应用户的在线状态
    User user = _userModel.query(toid);
    if(user.getState() == "online") //说明toid用户在其他服务器上
    {
        _redis.publish(toid, js.dump());
        return;
    }
    //否则，接收人当前不在线，需要存储该消息，当对方上线时自动派发
    _offlineMsgModel.insert(toid, js.dump());
}

//添加好友业务   msgid  id  friendid
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();
    //存储好友信息
    _friendModel.insert(userid, friendid);
}

//创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    //存储新创建的群组信息
    Group group(-1, name, desc);
    if (_groupModel.createGroup(group))
    {
        //存储群组创建人的信息
        _groupModel.addGroup(userid, group.getId(), "creator");
    }
}

//加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"];
    _groupModel.addGroup(userid, groupid, "normal");
}

//处理群组聊天
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"];
    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);
    lock_guard<mutex> lock(_connMutex);
    for (int id : useridVec)
    {
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end())
        {
            //用户在线，且在当前服务器，则转发群消息
            it->second->send(js.dump());
        }
        else
        {
            //查用户在线状态，若在线但不在当前服务器，则说明在其他服务器上
            User user = _userModel.query(id);
            if(user.getState() == "online")
            {
                _redis.publish(id, js.dump());
            }
            else
            {
            //不在线，存储离线群消息
            _offlineMsgModel.insert(id, js.dump());
            }

        }
    }
}

//处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutex> lock(_connMutex);

        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); it++)
        {
            if (it->second == conn)
            {
                //从map表删除用户的连接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    //用户注销，相当于下线，在redis中取消订阅通道
    _redis.unsubscribe(user.getId());

    //并且要更新数据库中该连接对应的用户的状态
    if (user.getId() != -1)
    {
        user.setState("offline");
        _userModel.updateState(user);
    }
}

//服务器异常时重置业务
void ChatService::reset()
{
    //把online状态的用户设置成offline
    _userModel.resetState();
}

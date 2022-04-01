#include "json.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
using namespace std;
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

//记录当前登录系统的用户信息
User g_currentUser;

//记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;

//记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;

//控制主菜单页面程序
bool isMainMenuRunning = false;

//显示当前登录成功用户的基本信息
void showCurrentUserData();

//接受线程
void readTaskHandler(int clientfd);

//获取系统时间（聊天信息需要添加时间信息）
string getCurrentTime();

//主聊天页面程序
void mainMenu(int);

//聊天客户端程序实现，main线程用作发送线程，子线程用作接受线程
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cerr << "command invalid ! example: ./ChatClient 127.0.0.1 8000" << endl;
        exit(-1);
    }

    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1)
    {
        cerr << "socket create error" << endl;
        exit(-1);
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    if (-1 == connect(clientfd, (struct sockaddr *)&server, sizeof(server)))
    {
        cerr << "connect server error" << endl;
        close(clientfd);
        exit(-1);
    }

    // main线程用于接受用户输入，负责发送数据
    for (;;)
    {
        //显示首页面菜单  登录  注册  退出
        cout << "==================" << endl;
        cout << "1. login" << endl;
        cout << "2. register" << endl;
        cout << "3. quit" << endl;
        cout << "==================" << endl;
        cout << "choice : ";
        int choice = 0;
        cin >> choice;
        cin.get(); //读掉缓冲器残留的回车

        switch (choice)
        {
        case 1: //登录
        {
            int id = 0;
            char pwd[50] = {0};
            cout << "userid : ";
            cin >> id;
            cin.get();
            cout << "password : ";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (-1 == len)
            {
                cerr << "send login msg error!" << endl;
            }
            else
            {
                char buffer[1024] = {0};
                len = recv(clientfd, buffer, 1024, 0);
                if (-1 == len)
                {
                    cerr << "recv login response error!" << endl;
                }
                else
                {
                    json responsejs = js.parse(buffer);
                    if (0 != responsejs["errno"].get<int>()) //登录失败
                    {
                        cerr << responsejs["errmsg"] << endl;
                    }
                    else //登录成功
                    {
                        //记录当前用户的ID和name
                        g_currentUser.setId(responsejs["id"].get<int>());
                        g_currentUser.setName(responsejs["name"]);

                        //记录当前用户的好友列表信息
                        if (responsejs.contains("friends"))
                        {
                            //初始化清空，否则用户退出第二次登录时会重复加载好友信息
                            g_currentUserFriendList.clear();
                            vector<string> vec = js["friend"];
                            for (string &str : vec)
                            {
                                json js = json::parse(str);
                                User user;
                                user.setId(js["id"].get<int>());
                                user.setName(js["name"]);
                                user.setState(js["state"]);
                                g_currentUserFriendList.push_back(user);
                            }
                        }

                        //记录当前用户所作群组列表信息
                        if (responsejs.contains("groups"))
                        {
                            //初始化清空，否则用户退出第二次登录时会重复加载群组信息
                            g_currentUserGroupList.clear();
                            vector<string> vec1 = responsejs["groups"];
                            for (string &groupstr : vec1)
                            {
                                json grpjs = json::parse(groupstr);
                                Group group;
                                group.setId(grpjs["id"].get<int>());
                                group.setName(grpjs["groupname"]);
                                group.setDesc(grpjs["groupdesc"]);

                                vector<string> vec2 = grpjs["users"];
                                for (string &userstr : vec2)
                                {
                                    GroupUser user;
                                    json js = json::parse(userstr);
                                    user.setId(js["id"].get<int>());
                                    user.setName(js["name"]);
                                    user.setState(js["state"]);
                                    user.setRole(js["role"]);
                                    group.getUsers().push_back(user);
                                }

                                g_currentUserGroupList.push_back(group);
                            }
                            showCurrentUserData();
                        }

                        //显示当前用户的离线消息，包括个人消息和群消息
                        if (responsejs.contains("offlinemsg"))
                        {
                            vector<string> vec = responsejs["offlinemsg"];
                            for (string &str : vec)
                            {
                                json js = json::parse(str);
                                if (ONE_CHAT_MSG == js["msgid"].get<int>())
                                {
                                    cout << js["time"].get<string>() << " [" << js["id"] << "] " << js["name"].get<string>()
                                         << " : " << js["msg"].get<string>() << endl;
                                }
                                else 
                                {
                                    cout << "群消息[" << js["groupid"] << "] " << js["time"].get<string>() << " [" << js["id"] << "] " << js["name"].get<string>()
                                         << " : " << js["msg"].get<string>() << endl;
                                }
                            }
                        }

                        //登录成功，启动接收线程，负责接收数据，该线程一个客户端只启动一次
                        static int readthreadnum = 0;
                        if(readthreadnum == 0)
                        {
                            std::thread readTask(readTaskHandler, clientfd); // pthread_create
                            readTask.detach();                               // pthread_detach
                            readthreadnum++;
                        }


                        //主线程进入聊天主菜单页面
                        isMainMenuRunning = true;
                        mainMenu(clientfd);
                    }
                }
            }
        }
        break;
        case 2: //注册
        {
            char name[50] = {0};
            char pwd[50] = {0};
            cout << "username : ";
            cin.getline(name, 50);
            cout << "userpassword : ";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (-1 == len)
            {
                cerr << "sed reg msg error : " << request << endl;
            }
            else
            {
                char buffer[1024] = {0};
                len = recv(clientfd, buffer, 1024, 0);
                if (-1 == len)
                {
                    cerr << "recv reg response error!" << endl;
                }
                else
                {
                    json responsejs = json::parse(buffer);
                    if (0 != responsejs["errno"].get<int>()) //注册失败
                    {
                        cerr << name << "has already existed, register error !" << endl;
                    }
                    else //注册成功
                    {
                        cout << name << "register success! userid is : " << responsejs["id"] << endl;
                    }
                }
            }
        }
        break;
        case 3: //退出
        {
            close(clientfd);
            exit(0);
        }
        default:
            cerr << "invalid input !" << endl;
            break;
        }
    }
}

//显示当前登录成功用户的基本信息
void showCurrentUserData()
{
    cout << "====================login user==================" << endl;
    cout << "current login user => id : " << g_currentUser.getId() << "  name : " << g_currentUser.getName() << endl;

    cout << "-------------------friend list------------------" << endl;
    if (!g_currentUserFriendList.empty())
    {
        for (User &user : g_currentUserFriendList)
        {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
        }
    }

    cout << "-------------------group list------------------" << endl;
    if (!g_currentUserGroupList.empty())
    {
        for (Group &group : g_currentUserGroupList)
        {
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for (GroupUser &user : group.getUsers())
            {
                cout << user.getId() << " " << user.getName() << " " << user.getState() << " " << user.getRole() << endl;
            }
        }
    }
    cout << "================================================" << endl;
}

//接受线程
void readTaskHandler(int clientfd)
{
    for (;;)
    {
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, 1024, 0);
        if (-1 == len || 0 == len)
        {
            close(clientfd);
            exit(-1);
        }

        //将buffer中的数据反序列化成json对象
        json js = json::parse(buffer);
        int type = js["msgid"].get<int>();
        if (ONE_CHAT_MSG == type)
        {
            cout << js["time"].get<string>() << " [" << js["id"] << "] " << js["name"].get<string>()
                 << " : " << js["msg"].get<string>() << endl;
            continue;
        }
        if (GROUP_CHAT_MSG == type)
        {
            cout << "群消息[" << js["groupid"] << "] " << js["time"].get<string>() << " [" << js["id"] << "] " << js["name"].get<string>()
                 << " : " << js["msg"].get<string>() << endl;
            continue;
        }
    }
}

void help(int fd = 0, string str = "");
void chat(int, string);
void addfriend(int, string);
void creategroup(int, string);
void addgroup(int, string);
void groupchat(int, string);
void logout(int, string);

//系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令 格式help"},
    {"chat", "一对一聊天 格式chat:friendid:message"},
    {"addfriend", "添加好友 格式addfriend:friendid"},
    {"creategroup", "创建群组 格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组 格式addgroup:groupid"},
    {"groupchat", "群聊 格式groupchat:groupid:message"},
    {"logout", "注销 格式logout"}};

//注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"logout", logout}};
//主聊天页面程序
void mainMenu(int clientfd)
{
    help();

    char buffer[1024] = {0};
    while(isMainMenuRunning == true)
    {
        cin.getline(buffer, 1024);
        string commandbuf(buffer);
        string command;
        int idx = commandbuf.find(":");
        if (-1 == idx)
        {
            command = commandbuf;
        }
        else
        {
            command = commandbuf.substr(0, idx);
        }

        auto it = commandHandlerMap.find(command);
        if (it == commandHandlerMap.end())
        {
            cerr << "invalid input command ! "<< endl;
            continue;
        }
        it->second(clientfd, commandbuf.substr(idx + 1, commandbuf.size() - idx));
    }
}

void help(int, string)
{
    cout << "show command list >>>" << endl;
    for (auto &p : commandMap)
    {
        cout << p.first << " : " << p.second << endl;
    }
    cout << endl;
}

// addfriend:friendid
void addfriend(int clientfd, string str)
{
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send addfriend msg error ->" << buffer << endl;
    }
}

// chat:friendid:message
void chat(int clientfd, string str)
{
    int idx = str.find(":"); // friendid:message
    if (-1 == idx)
    {
        cerr << "chat command invalid!" << endl;
        return;
    }

    int friendid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);
    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["toid"] = friendid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send chat msg error ->" << buffer << endl;
    }
}

// creategroup:groupname:groupdesc
void creategroup(int clientfd, string str)
{
    int idx = str.find(":");
    if (-1 == idx)
    {
        cerr << "creategroup command is invalid!" << endl;
        return;
    }
    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1, str.size() - 1 - idx);
    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;

    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send creategroup msg error -> " << buffer << endl;
    }
}

// addgroup:groupid
void addgroup(int clientfd, string str)
{
    int groupid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;

    string buffer = js.dump();
    if (-1 == send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0))
    {
        cerr << "send addgroup msg error ! "<< endl;
    }
}

// groupchat:groupid:message
void groupchat(int clientfd, string str)
{
    int idx = str.find(":");
    if (-1 == idx)
    {
        cerr << "groupchat command is invalid !" << endl;
        return;
    }
    int groupid = atoi(str.substr(0, idx).c_str());
    string msg = str.substr(idx + 1, str.size() - 1 - idx);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = msg;
    js["time"] = getCurrentTime();
    string buffer = js.dump();
    if (-1 == send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0))
    {
        cerr << "send groupchat msg error !" << endl;
    }
}

// logout
void logout(int clientfd, string str)
{
    json js;
    js["msgid"] = LOGOUT_MSG;
    js["id"] = g_currentUser.getId();

    string buffer = js.dump();
    if (-1 == send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0))
    {
        cerr << "send logout msg error ! "<< endl;
    }
    else    //退出成功，将全局变量 isMainMenuRunning置为false
    {
        isMainMenuRunning = false;
    }
}

//获取系统时间（聊天信息需要添加时间信息）
string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm* ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d", (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1,
    (int)ptm->tm_mday, (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);
}

#include <iostream>
#include "friendmodel.hpp"
#include "db.h"
using namespace std;

//添加好友关系
void FriendModel::insert(int userid, int friendid)
{
    //组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into friend values(%d, %d)", userid, friendid);

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

//返回用户好友列表
vector<User> FriendModel::query(int userid)
{
    //组装sql语句
    char sql[1024] = {0};
    //user表和friend表的联合查询    
    sprintf(sql, "select a.id,a.name,a.state from user a inner join friend b on a.id=b.friendid where b.userid=%d", userid);

    vector<User> vec;
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        //找到了用户
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                vec.push_back(user);
            }  
        }
        //释放资源
        mysql_free_result(res);
        return vec;
    }
    //没找到返回一个默认空列表
    return vec;
}

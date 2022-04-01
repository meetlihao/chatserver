#ifndef GROUPUSER_H
#define GROUPUSER_H

#include "user.hpp"

//群组用户，多了一个role成员，从User类直接继承而来，复用User的其他信息
class GroupUser : public User
{
public:
    void setRole(string role) { this->role = role;}

    string getRole() {return this->role;}

private:
    string role;    //群成员角色：管理员还是普通成员
};

#endif
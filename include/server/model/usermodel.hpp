#ifndef USERMODEL_H
#define USERMODEL_H

#include "user.hpp"
//User表的数据操作类
class UserModel
{
public:
    //User表插入数据
    bool insert(User &user);
    User query(int id);
    bool updateState(User &user);
    void resetState();
};

#endif
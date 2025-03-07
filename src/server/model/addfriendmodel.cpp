#include "addfriendmodel.hpp"
#include "db.h"
#include <mysql/mysql.h>

void FriendModel::insert(int userid, int friendid)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into Friend values(%d, %d)", userid, friendid);
    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

std::vector<User> FriendModel::query(int userid)
{
    char sql[1024] = {0};
    sprintf(sql, "select a.id,a.name,a.state from User a inner join Friend b on ((b.friendid=a.id and b.userid=%d) or (b.userid=a.id and b.friendid=%d)) where b.userid=%d or b.friendid=%d", userid, userid, userid, userid);
    MySQL mysql;
    std::vector<User> uvec;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                uvec.push_back(user);
            }
            mysql_free_result(res);
        }
    }
    return uvec;
}
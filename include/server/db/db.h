#ifndef DB_H
#define DB_H

#include <mysql/mysql.h>
#include <string>

// 数据库操作类

class MySQL
{
public:
    MySQL();
    ~MySQL();
    //连接数据库
    bool connect();
    //更新操作
    bool update(std::string sql);
    //查询操作
    MYSQL_RES* query(std::string sql);

    MYSQL* getConnection();
private:
    MYSQL *_conn;
};

#endif
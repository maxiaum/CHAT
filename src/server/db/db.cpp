#include "db.h"
#include <muduo/base/Logging.h>

// 数据库配置信息
static std::string server = "127.0.0.1";
static std::string user = "root";
static std::string password = "123456";
static std::string dbname = "chat";

MySQL::MySQL()
{
    _conn = mysql_init(nullptr);
}

MySQL::~MySQL()
{
    if (_conn != nullptr)
    {
        mysql_close(_conn);
    }
}
// 连接数据库
bool MySQL::connect()
{
    MYSQL *p = mysql_real_connect(_conn, server.c_str(), user.c_str(),
                                  password.c_str(), dbname.c_str(), 3306, nullptr, 0);
    if (p != nullptr)
    {
        // 使中文ASCII码正常显示
        mysql_query(_conn, "set name gbk");
        LOG_INFO << "connect mysql success!";
    }
    else
    {
        LOG_INFO << "connect mysql fail!";
    }
    return p;
}
// 更新操作
bool MySQL::update(std::string sql)
{
    if (mysql_query(_conn, sql.c_str()))
    {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "更新失败！";
        return false;
    }
    return true;
}
// 查询操作
MYSQL_RES *MySQL::query(std::string sql)
{

    if (mysql_query(_conn, sql.c_str()))
    {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "更新失败！";
        return nullptr;
    }
    return mysql_use_result(_conn);
}

MYSQL *MySQL::getConnection()
{
    return _conn;
}
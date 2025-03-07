#ifndef GROUPUSER_H
#define GROUPUSER_H

#include "user.hpp"

class GroupUser : public User
{
public:
    void setRole(std::string role) { this->_role = role; }

    std::string getRole() { return this->_role; }

private:
    std::string _role;
};

#endif
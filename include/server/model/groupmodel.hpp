#ifndef GROUPMODEL_H
#define GROUPMODEL_H

#include "group.hpp"
#include <vector>
#include <string>

class GroupModel
{
public:
    bool createGroup(Group &group);
    void addGroup(int userid, int groupid, std::string role);
    std::vector<Group> queryGroups(int userid);
    std::vector<int> queryGroupUsers(int userid, int groupid);
//private:
};

#endif
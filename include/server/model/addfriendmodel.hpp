#ifndef ADDFRIENDMODEL_H
#define ADDFRIENDMODEL_H

#include "user.hpp"
#include <vector>
#include <unordered_set>
// using namespace std;
// #include <string>

class FriendModel
{
public:
    void insert(int userid, int friendid);

    std::vector<User> query(int userid);
};
#endif
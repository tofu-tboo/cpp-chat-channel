#ifndef __USER_MANAGER_H__
#define __USER_MANAGER_H__

#define IP_FULL			12001
#define UNKNOWN_NAME	11001

#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>

#include "../libs/socket.h"
#include "../libs/util.h"

class UserManager {
private:
    static std::unordered_map<fd_t, std::string> name_map;
    static std::unordered_map<fd_t, std::string> fd_ip_map;
    static std::unordered_map<std::string, int> ip_count_map;
    static std::shared_mutex name_map_mtx;

public:
    static bool get_user_name(const fd_t fd, std::string& out_user_name);
    static void set_user_name(const fd_t fd, const std::string& user_name);
    static void remove_user_name(const fd_t fd);
    static void check_and_register_ip(const fd_t fd, int max_per_ip);
};

#endif
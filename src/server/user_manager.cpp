#include "user_manager.h"

std::unordered_map<fd_t, std::string> UserManager::name_map;
std::shared_mutex UserManager::name_map_mtx;

bool UserManager::get_user_name(const fd_t fd, std::string& out_user_name) {
    std::shared_lock<std::shared_mutex> lock(name_map_mtx);
    auto it = name_map.find(fd);
    if (it == name_map.end()) {
        return false;
    }
    out_user_name = it->second;
    return true;
}

void UserManager::set_user_name(const fd_t fd, const std::string& user_name) {
    std::unique_lock<std::shared_mutex> lock(name_map_mtx);
    name_map[fd] = user_name;
}

void UserManager::remove_user_name(const fd_t fd) {
    std::unique_lock<std::shared_mutex> lock(name_map_mtx);
    name_map.erase(fd);
}
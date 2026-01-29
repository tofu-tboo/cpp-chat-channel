#include <arpa/inet.h>
#include <sys/socket.h>
#include "user_manager.h"

std::unordered_map<fd_t, std::string> UserManager::name_map;
std::unordered_map<fd_t, std::string> UserManager::fd_ip_map;
std::unordered_map<std::string, int> UserManager::ip_count_map;
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

    auto it = fd_ip_map.find(fd);
    if (it != fd_ip_map.end()) {
        ip_count_map[it->second]--;
        if (ip_count_map[it->second] <= 0) {
            ip_count_map.erase(it->second);
        }
        fd_ip_map.erase(it);
    }
}

void UserManager::check_and_register_ip(const fd_t fd, int max_per_ip) {
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);
    if (getpeername(fd, (struct sockaddr*)&addr, &addr_len) == -1)
		throw runtime_errorf("getpeername failed.");

    char ip_cstr[INET6_ADDRSTRLEN];
	std::string ip_str;
    if (addr.ss_family == AF_INET) {
        inet_ntop(AF_INET, &((struct sockaddr_in*)&addr)->sin_addr, ip_cstr, sizeof(ip_cstr));
    } else {
        inet_ntop(AF_INET6, &((struct sockaddr_in6*)&addr)->sin6_addr, ip_cstr, sizeof(ip_cstr));
    }

	ip_str = std::string(ip_cstr);
	std::unique_lock<std::shared_mutex> lock(name_map_mtx);
    if (ip_count_map[ip_str] >= max_per_ip) {
        throw runtime_errorf(IP_FULL);
    }
    ip_count_map[ip_str]++;
    fd_ip_map[fd] = ip_str;
}
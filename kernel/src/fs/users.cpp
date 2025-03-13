#include "users.hpp"

#include <cstdio>
#include <cstring>

#include "fat32.hpp"

namespace fs {

CUserManager& CUserManager::instance() {
    static CUserManager instance;
    return instance;
}

CUserManager::CUserManager() : m_user_count(0), m_current_user_index(0), m_initialized(false) {}

bool CUserManager::initialize() {
    if (m_initialized) return true;

    auto& fs = CFat32FileSystem::instance();

    uint8_t empty[0];
    fs.writeFile(ROOT_CLUSTER, empty, 0);

    if (!add_user("root", "root")) return false;

    if (!load_user_data()) save_user_data();

    m_initialized = true;
    return true;
}

bool CUserManager::add_user(const char* username, const char* password) {
    if (!username || !*username) return false;

    if (m_user_count >= MAX_USERS) return false;

    if (user_exists(username)) return false;

    SUserInfo& user = m_users[m_user_count];
    strncpy(user.username, username, MAX_USERNAME - 1);
    user.username[MAX_USERNAME - 1] = '\0';

    strncpy(user.password, password, MAX_PASSWORD - 1);
    user.password[MAX_PASSWORD - 1] = '\0';

    user.uid = m_user_count + 1;
    user.gid = m_user_count + 1;
    user.active = true;

    if (!create_home_directory(user.username)) return false;

    m_user_count++;

    save_user_data();

    return true;
}

bool CUserManager::remove_user(const char* username) {
    SUserInfo* user = find_user(username);
    if (!user) return false;

    if (user == &m_users[m_current_user_index]) return false;

    user->active = false;

    save_user_data();

    return true;
}

bool CUserManager::switch_user(const char* username, const char* password) {
    if (!username || !password) return false;

    SUserInfo* user = find_user(username);
    if (!user || !user->active) return false;

    if (strcmp(user->password, password) != 0) return false;

    m_current_user_index = user - m_users;

    auto& fs = CFat32FileSystem::instance();
    char home_path[MAX_PATH];

    if (strcmp(user->username, "root") == 0)
        strcpy(home_path, "/");
    else {
        strcpy(home_path, "/home/");
        strcat(home_path, user->username);
    }

    fs.set_current_path(home_path);

    return true;
}

const char* CUserManager::get_current_username() const {
    if (m_user_count == 0 || m_current_user_index >= m_user_count ||
        !m_users[m_current_user_index].active)
        return "";

    return m_users[m_current_user_index].username;
}

uint32_t CUserManager::get_current_uid() const {
    if (m_user_count == 0) return 0;
    return m_users[m_current_user_index].uid;
}

uint32_t CUserManager::get_current_gid() const {
    if (m_user_count == 0) return 0;
    return m_users[m_current_user_index].gid;
}

bool CUserManager::user_exists(const char* username) const {
    return find_user(username) != nullptr;
}

bool CUserManager::save_user_data() {
    auto& fs = CFat32FileSystem::instance();

    size_t total_size = sizeof(size_t) + sizeof(SUserInfo) * m_user_count;
    uint8_t* data = new uint8_t[total_size];

    *reinterpret_cast<size_t*>(data) = m_user_count;

    for (size_t i = 0; i < m_user_count; ++i) {
        memcpy(data + sizeof(size_t) + i * sizeof(SUserInfo), &m_users[i], sizeof(SUserInfo));
    }

    fs.writeFile(ROOT_CLUSTER, data, total_size);

    delete[] data;
    return true;
}

bool CUserManager::load_user_data() {
    auto& fs = CFat32FileSystem::instance();

    uint8_t buffer[1024];
    fs.readFile(ROOT_CLUSTER, buffer, sizeof(buffer));

    size_t stored_user_count = *reinterpret_cast<size_t*>(buffer);
    if (stored_user_count > MAX_USERS) return false;

    for (size_t i = 0; i < stored_user_count; ++i) {
        memcpy(&m_users[i], buffer + sizeof(size_t) + i * sizeof(SUserInfo), sizeof(SUserInfo));
    }

    m_user_count = stored_user_count;
    return true;
}

SUserInfo* CUserManager::find_user(const char* username) {
    for (size_t i = 0; i < m_user_count; ++i) {
        if (m_users[i].active && strcmp(m_users[i].username, username) == 0) return &m_users[i];
    }
    return nullptr;
}

const SUserInfo* CUserManager::find_user(const char* username) const {
    for (size_t i = 0; i < m_user_count; ++i) {
        if (m_users[i].active && strcmp(m_users[i].username, username) == 0) return &m_users[i];
    }
    return nullptr;
}

bool CUserManager::create_home_directory(const char* username) {
    if (!username || !*username) return false;
    if (strcmp(username, "root") == 0) return true;

    auto& fs = CFat32FileSystem::instance();
    uint8_t empty[0];
    fs.writeFile(ROOT_CLUSTER, empty, 0);

    return true;
}

void CUserManager::list_users(void (*callback)(const char* username, uint32_t uid,
                                               uint32_t gid)) const {
    if (!callback) return;

    for (size_t i = 0; i < m_user_count; ++i) {
        if (m_users[i].active) callback(m_users[i].username, m_users[i].uid, m_users[i].gid);
    }
}

}  // namespace fs
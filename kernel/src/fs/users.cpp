#include "users.hpp"

#include <cstdio>
#include <cstring>

#include "filesystem.hpp"

namespace fs {

CUserManager& CUserManager::instance() {
    static CUserManager instance;
    return instance;
}

CUserManager::CUserManager() : m_user_count(0), m_current_user_index(0), m_initialized(false) {}

bool CUserManager::initialize() {
    if (m_initialized) return true;

    auto& fs = FileSystem::instance();

    fs.create_file("/home", FileType::Directory);
    fs.create_file("/etc", FileType::Directory);

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

    auto& fs = FileSystem::instance();
    char home_path[MAX_PATH];

    if (strcmp(user->username, "root") == 0)
        strcpy(home_path, "/");
    else {
        strcpy(home_path, "/home/");
        strcat(home_path, user->username);
    }

    fs.change_directory(home_path);

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
    auto& fs = FileSystem::instance();

    size_t total_size = sizeof(size_t) + sizeof(SUserInfo) * m_user_count;
    uint8_t* data = new uint8_t[total_size];

    *reinterpret_cast<size_t*>(data) = m_user_count;

    for (size_t i = 0; i < m_user_count; ++i) {
        memcpy(data + sizeof(size_t) + i * sizeof(SUserInfo), &m_users[i], sizeof(SUserInfo));
    }

    bool result = fs.write_file("/etc/passwd", data, total_size);

    delete[] data;
    return result;
}

bool CUserManager::load_user_data() {
    auto& fs = FileSystem::instance();

    FileNode* passwd_file = fs.get_file("/etc/passwd");
    if (!passwd_file || passwd_file->type != FileType::Regular) return false;

    uint8_t* data = new uint8_t[passwd_file->size];
    if (!fs.read_file("/etc/passwd", data, passwd_file->size)) {
        delete[] data;
        return false;
    }

    size_t stored_user_count = *reinterpret_cast<size_t*>(data);
    if (stored_user_count > MAX_USERS) {
        delete[] data;
        return false;
    }

    for (size_t i = 0; i < stored_user_count; ++i) {
        memcpy(&m_users[i], data + sizeof(size_t) + i * sizeof(SUserInfo), sizeof(SUserInfo));
    }

    m_user_count = stored_user_count;

    delete[] data;
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

    auto& fs = FileSystem::instance();

    FileNode* home_dir = fs.get_file("/home");
    if (!home_dir) return false;

    char home_path[MAX_PATH];
    strcpy(home_path, "/home/");
    strcat(home_path, username);

    FileNode* dir = fs.create_file(home_path, FileType::Directory);
    if (!dir) return false;

    FileNode* check = fs.get_file(home_path);
    if (!check) return false;

    return true;
}

}  // namespace fs
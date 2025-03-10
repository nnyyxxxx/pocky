#pragma once

#include <cstdint>
#include <cstddef>

namespace fs {

constexpr size_t MAX_USERNAME = 32;
constexpr size_t MAX_PASSWORD = 64;
constexpr size_t MAX_USERS = 16;

struct SUserInfo {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    uint32_t uid;
    uint32_t gid;
    bool active;

    SUserInfo() : username{0}, password{0}, uid(0), gid(0), active(false) {}
};

class CUserManager {
public:
    static CUserManager& instance();

    bool initialize();

    bool add_user(const char* username, const char* password);
    bool remove_user(const char* username);
    bool switch_user(const char* username, const char* password);

    const char* get_current_username() const;
    uint32_t get_current_uid() const;
    uint32_t get_current_gid() const;

    bool user_exists(const char* username) const;

    bool save_user_data();
    bool load_user_data();

    void list_users(void (*callback)(const char* username, uint32_t uid, uint32_t gid)) const;

private:
    CUserManager();
    ~CUserManager() = default;
    CUserManager(const CUserManager&) = delete;
    CUserManager& operator=(const CUserManager&) = delete;

    SUserInfo* find_user(const char* username);
    const SUserInfo* find_user(const char* username) const;

    bool create_home_directory(const char* username);

    SUserInfo m_users[MAX_USERS];
    size_t m_user_count;
    size_t m_current_user_index;
    bool m_initialized;
};

} // namespace fs
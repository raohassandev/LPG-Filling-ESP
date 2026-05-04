#pragma once

#include <Arduino.h>
#include <Preferences.h>

// Roles — values match UserRole enum in AuthService.h
enum class UserRoleLevel : uint8_t {
    Operator     = 1,
    Manufacturer = 2,  // "Maintenance" in AuthService enum
    Admin        = 3,
};

struct UserRecord {
    String      username;
    String      passwordHash;
    UserRoleLevel role       = UserRoleLevel::Operator;
    bool        blocked      = false;
    bool        canSetRate   = false;
};

class UserStore {
public:
    static constexpr uint8_t kMaxUsers = 20;

    void   begin();

    // Returns true + fills out role/blocked/canSetRate on success
    bool   validateCredentials(const String& username, const String& password,
                               UserRoleLevel& outRole, bool& outBlocked, bool& outCanSetRate) const;

    bool   createUser(const String& username, const String& password,
                      UserRoleLevel role, bool canSetRate = false);
    bool   updatePassword(const String& username, const String& newPassword);
    bool   setBlocked(const String& username, bool blocked);
    bool   setCanSetRate(const String& username, bool canSetRate);
    bool   deleteUser(const String& username);   // admin account cannot be deleted

    // Serialised JSON array of user objects (no password hashes)
    String listJson() const;

    bool   usernameExists(const String& username) const;
    uint8_t count() const { return count_; }

    static String hashPassword(const String& password);

private:
    uint8_t    count_ = 0;
    UserRecord users_[kMaxUsers];

    int8_t  findIndex(const String& username) const;
    void    persist(uint8_t index);
    void    load();
    void    seedDefaults();
};

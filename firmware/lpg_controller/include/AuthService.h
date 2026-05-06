#pragma once

#include <Arduino.h>
#include "UserStore.h"

// User roles — values intentionally match UserRoleLevel
enum class UserRole : uint8_t
{
    None        = 0,
    Operator    = 1,
    Maintenance = 2,  // "Manufacturer" role
    Admin       = 3
};

// Session info
struct SessionInfo
{
    String   sessionId;
    UserRole role;
    uint32_t createdAt;
    uint32_t lastActivity;
    bool     active;
};

class AuthService
{
public:
    void begin();

    // Authentication
    bool     login(const String& username, const String& password);
    void     logout();
    UserRole currentRole()        const { return currentRole_; }
    String   currentUsername()    const { return currentUsername_; }
    bool     canCurrentUserSetRate() const { return currentCanSetRate_; }
    bool     hasPermission(UserRole required);

    // Session management
    SessionInfo getCurrentSession() const { return currentSession_; }
    bool        sessionValid();
    void        refreshSession();

    // Session timeout (30 minutes)
    static constexpr uint32_t kSessionTimeoutMs = 30 * 60 * 1000;

    // User management — delegated to UserStore, guarded by Admin role
    bool    createUser(const String& username, const String& password,
                       UserRoleLevel role, bool canSetRate = false);
    bool    updatePassword(const String& username, const String& newPassword);
    bool    setBlocked(const String& username, bool blocked);
    bool    setCanSetRate(const String& username, bool canSetRate);
    bool    deleteUser(const String& username);
    String  listUsersJson() const { return userStore_.listJson(); }
    uint8_t userCount()     const { return userStore_.count(); }

    // Change own password (requires old password)
    bool changePassword(const String& oldPassword, const String& newPassword);

    // Access to underlying store (for WebPortal validation helpers)
    UserStore& userStore() { return userStore_; }

private:
    UserStore   userStore_;
    UserRole    currentRole_       = UserRole::None;
    String      currentUsername_;
    bool        currentCanSetRate_ = false;
    SessionInfo currentSession_;
};

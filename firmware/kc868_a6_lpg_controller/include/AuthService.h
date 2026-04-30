#pragma once

#include <Arduino.h>
#include <Preferences.h>

// User roles
enum class UserRole : uint8_t
{
    None = 0,        // Not authenticated
    Operator = 1,    // Basic operation (start/stop)
    Maintenance = 2, // Calibration, testing
    Admin = 3        // Full access, configuration
};

// Session info
struct SessionInfo
{
    String sessionId;
    UserRole role;
    uint32_t createdAt;
    uint32_t lastActivity;
    bool active;
};

class AuthService
{
public:
    void begin();

    // Authentication
    bool login(const String &username, const String &password);
    void logout();
    UserRole currentRole() const { return currentRole_; }
    bool hasPermission(UserRole required);

    // Session management
    SessionInfo getCurrentSession() const { return currentSession_; }
    bool sessionValid();
    void refreshSession();

    // Password management
    bool changePassword(const String &oldPassword, const String &newPassword);
    bool resetPassword(const String &newPassword); // Admin only

    // Role management (admin only)
    bool setUserPassword(const String &username, const String &password);
    bool deleteUser(const String &username);
    uint8_t userCount() const { return userCount_; }

    // Session timeout (30 minutes)
    static constexpr uint32_t kSessionTimeoutMs = 30 * 60 * 1000;

private:
    Preferences prefs_;
    UserRole currentRole_ = UserRole::None;
    SessionInfo currentSession_;
    uint8_t userCount_ = 0;

    // Default credentials (should be changed on first use)
    static constexpr const char *kDefaultOperator = "operator";
    static constexpr const char *kDefaultOperatorPass = "1234";
    static constexpr const char *kDefaultMaintenance = "maintenance";
    static constexpr const char *kDefaultMaintenancePass = "5678";
    static constexpr const char *kDefaultAdmin = "admin";
    static constexpr const char *kDefaultAdminPass = "0000";

    String hashPassword(const String &password);
    bool validateUser(const String &username, const String &password, UserRole &role);
};

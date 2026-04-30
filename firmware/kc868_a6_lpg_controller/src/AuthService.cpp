#include "AuthService.h"

void AuthService::begin()
{
    prefs_.begin("auth", false);

    // Initialize default users if first run
    if (!prefs_.getString("admin_user").length())
    {
        prefs_.putString("admin_user", kDefaultAdmin);
        prefs_.putString("admin_hash", hashPassword(kDefaultAdminPass));
        prefs_.putString("operator_user", kDefaultOperator);
        prefs_.putString("operator_hash", hashPassword(kDefaultOperatorPass));
        prefs_.putString("maintenance_user", kDefaultMaintenance);
        prefs_.putString("maintenance_hash", hashPassword(kDefaultMaintenancePass));

        userCount_ = 3;
        Serial.println("[AUTH] Default users initialized");
    }
    else
    {
        // Count existing users
        userCount_ = 0;
        if (prefs_.getString("admin_user").length())
            userCount_++;
        if (prefs_.getString("operator_user").length())
            userCount_++;
        if (prefs_.getString("maintenance_user").length())
            userCount_++;
    }

    Serial.printf("[AUTH] Auth service initialized, %u users\n", userCount_);
}

bool AuthService::login(const String &username, const String &password)
{
    UserRole role;
    if (validateUser(username, password, role))
    {
        currentRole_ = role;
        currentSession_.sessionId = String(millis());
        currentSession_.role = role;
        currentSession_.createdAt = millis();
        currentSession_.lastActivity = millis();
        currentSession_.active = true;

        Serial.printf("[AUTH] User logged in: %s, role: %u\n", username.c_str(), static_cast<uint8_t>(role));
        return true;
    }

    Serial.printf("[AUTH] Login failed for user: %s\n", username.c_str());
    return false;
}

void AuthService::logout()
{
    currentRole_ = UserRole::None;
    currentSession_.active = false;
    Serial.println("[AUTH] User logged out");
}

bool AuthService::hasPermission(UserRole required)
{
    return static_cast<uint8_t>(currentRole_) >= static_cast<uint8_t>(required);
}

bool AuthService::sessionValid()
{
    if (!currentSession_.active)
    {
        return false;
    }

    if (millis() - currentSession_.lastActivity > kSessionTimeoutMs)
    {
        logout();
        return false;
    }

    return true;
}

void AuthService::refreshSession()
{
    if (currentSession_.active)
    {
        currentSession_.lastActivity = millis();
    }
}

bool AuthService::changePassword(const String &oldPassword, const String &newPassword)
{
    // Get current username based on role
    String username;
    String key;

    switch (currentRole_)
    {
    case UserRole::Admin:
        username = prefs_.getString("admin_user");
        key = "admin_hash";
        break;
    case UserRole::Maintenance:
        username = prefs_.getString("maintenance_user");
        key = "maintenance_hash";
        break;
    case UserRole::Operator:
        username = prefs_.getString("operator_user");
        key = "operator_hash";
        break;
    default:
        return false;
    }

    // Verify old password
    String storedHash = prefs_.getString(key.c_str());
    if (storedHash != hashPassword(oldPassword))
    {
        Serial.println("[AUTH] Password change failed: incorrect old password");
        return false;
    }

    // Set new password
    prefs_.putString(key.c_str(), hashPassword(newPassword));
    Serial.println("[AUTH] Password changed successfully");
    return true;
}

bool AuthService::resetPassword(const String &newPassword)
{
    // Only admin can reset passwords
    if (currentRole_ != UserRole::Admin)
    {
        return false;
    }

    // This would be used to reset other users' passwords
    // Implementation depends on specific requirements
    return false;
}

bool AuthService::setUserPassword(const String &username, const String &password)
{
    if (currentRole_ != UserRole::Admin)
    {
        return false;
    }

    String hash = hashPassword(password);

    if (username == prefs_.getString("admin_user"))
    {
        prefs_.putString("admin_hash", hash);
    }
    else if (username == prefs_.getString("operator_user"))
    {
        prefs_.putString("operator_hash", hash);
    }
    else if (username == prefs_.getString("maintenance_user"))
    {
        prefs_.putString("maintenance_hash", hash);
    }
    else
    {
        return false;
    }

    return true;
}

bool AuthService::deleteUser(const String &username)
{
    // Admin cannot be deleted
    if (username == prefs_.getString("admin_user"))
    {
        return false;
    }

    if (currentRole_ != UserRole::Admin)
    {
        return false;
    }

    if (username == prefs_.getString("operator_user"))
    {
        prefs_.remove("operator_user");
        prefs_.remove("operator_hash");
        userCount_--;
    }
    else if (username == prefs_.getString("maintenance_user"))
    {
        prefs_.remove("maintenance_user");
        prefs_.remove("maintenance_hash");
        userCount_--;
    }
    else
    {
        return false;
    }

    return true;
}

String AuthService::hashPassword(const String &password)
{
    // Simple hash - in production use proper hashing like bcrypt
    // This is just a placeholder
    uint32_t hash = 5381;
    for (size_t i = 0; i < password.length(); i++)
    {
        hash = ((hash << 5) + hash) + password.charAt(i);
    }
    return String(hash);
}

bool AuthService::validateUser(const String &username, const String &password, UserRole &role)
{
    String hash = hashPassword(password);

    if (username == prefs_.getString("admin_user") &&
        hash == prefs_.getString("admin_hash"))
    {
        role = UserRole::Admin;
        return true;
    }

    if (username == prefs_.getString("maintenance_user") &&
        hash == prefs_.getString("maintenance_hash"))
    {
        role = UserRole::Maintenance;
        return true;
    }

    if (username == prefs_.getString("operator_user") &&
        hash == prefs_.getString("operator_hash"))
    {
        role = UserRole::Operator;
        return true;
    }

    return false;
}

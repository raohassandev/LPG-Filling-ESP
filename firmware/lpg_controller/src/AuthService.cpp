#include "AuthService.h"
#include <esp_system.h>

void AuthService::begin()
{
    userStore_.begin();
    Serial.printf("[AUTH] Auth service ready, %u user(s)\n", userStore_.count());
}

bool AuthService::login(const String& username, const String& password)
{
    UserRoleLevel storeRole;
    bool blocked, canSetRate;

    if (!userStore_.validateCredentials(username, password, storeRole, blocked, canSetRate)) {
        Serial.printf("[AUTH] Login failed: %s\n", username.c_str());
        return false;
    }

    if (blocked) {
        Serial.printf("[AUTH] Login refused — blocked: %s\n", username.c_str());
        return false;
    }

    // Map UserRoleLevel → UserRole (values are identical by design)
    currentRole_       = static_cast<UserRole>(static_cast<uint8_t>(storeRole));
    currentUsername_   = username;
    currentCanSetRate_ = canSetRate || (currentRole_ == UserRole::Admin);

    // Generate cryptographically random 128-bit session token
    uint8_t rndBytes[16];
    esp_fill_random(rndBytes, sizeof(rndBytes));
    char tokenBuf[33]; tokenBuf[32] = '\0';
    for (int i = 0; i < 16; i++) snprintf(tokenBuf + i*2, 3, "%02x", rndBytes[i]);
    currentSession_.sessionId = String(tokenBuf);
    currentSession_.role         = currentRole_;
    currentSession_.createdAt    = millis();
    currentSession_.lastActivity = millis();
    currentSession_.active       = true;

    Serial.printf("[AUTH] Login: %s role=%u canSetRate=%d\n",
                  username.c_str(), (uint8_t)currentRole_, currentCanSetRate_);
    return true;
}

void AuthService::logout()
{
    Serial.printf("[AUTH] Logout: %s\n", currentUsername_.c_str());
    currentRole_       = UserRole::None;
    currentUsername_   = "";
    currentCanSetRate_ = false;
    currentSession_.active = false;
}

bool AuthService::hasPermission(UserRole required)
{
    return static_cast<uint8_t>(currentRole_) >= static_cast<uint8_t>(required);
}

bool AuthService::sessionValid()
{
    if (!currentSession_.active) return false;
    if (millis() - currentSession_.lastActivity > kSessionTimeoutMs) {
        logout();
        return false;
    }
    return true;
}

void AuthService::refreshSession()
{
    if (currentSession_.active)
        currentSession_.lastActivity = millis();
}

bool AuthService::changePassword(const String& oldPassword, const String& newPassword)
{
    if (currentUsername_.isEmpty()) return false;
    // Verify old password first
    UserRoleLevel role; bool blocked, csr;
    if (!userStore_.validateCredentials(currentUsername_, oldPassword, role, blocked, csr))
        return false;
    return userStore_.updatePassword(currentUsername_, newPassword);
}

// --- Admin-guarded user management ---

bool AuthService::createUser(const String& username, const String& password,
                              UserRoleLevel role, bool canSetRate)
{
    if (!hasPermission(UserRole::Admin)) return false;
    return userStore_.createUser(username, password, role, canSetRate);
}

bool AuthService::updatePassword(const String& username, const String& newPassword)
{
    if (!hasPermission(UserRole::Admin)) return false;
    return userStore_.updatePassword(username, newPassword);
}

bool AuthService::setBlocked(const String& username, bool blocked)
{
    if (!hasPermission(UserRole::Admin)) return false;
    return userStore_.setBlocked(username, blocked);
}

bool AuthService::setCanSetRate(const String& username, bool canSetRate)
{
    if (!hasPermission(UserRole::Admin)) return false;
    return userStore_.setCanSetRate(username, canSetRate);
}

bool AuthService::deleteUser(const String& username)
{
    if (!hasPermission(UserRole::Admin)) return false;
    // Cannot delete self
    if (username.equalsIgnoreCase(currentUsername_)) return false;
    return userStore_.deleteUser(username);
}

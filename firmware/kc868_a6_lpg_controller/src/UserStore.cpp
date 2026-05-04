#include "UserStore.h"

// NVS key helpers — all keys kept ≤ 15 chars (NVS limit)
static String kN(uint8_t i) { return "u" + String(i) + "n"; }
static String kH(uint8_t i) { return "u" + String(i) + "h"; }
static String kR(uint8_t i) { return "u" + String(i) + "r"; }
static String kB(uint8_t i) { return "u" + String(i) + "b"; }
static String kS(uint8_t i) { return "u" + String(i) + "s"; }

String UserStore::hashPassword(const String& password) {
    // djb2 — fast, deterministic, sufficient for embedded device
    uint32_t h = 5381;
    for (size_t i = 0; i < password.length(); i++)
        h = ((h << 5) + h) + (uint8_t)password.charAt(i);
    char buf[12];
    snprintf(buf, sizeof(buf), "%08X", h);
    return String(buf);
}

void UserStore::begin() {
    load();
    if (count_ == 0) {
        seedDefaults();
    }
    Serial.printf("[USERS] %u user(s) loaded\n", count_);
}

void UserStore::load() {
    Preferences p;
    p.begin("usrs", true);
    count_ = p.getUChar("cnt", 0);
    if (count_ > kMaxUsers) count_ = 0;
    for (uint8_t i = 0; i < count_; i++) {
        users_[i].username     = p.getString(kN(i).c_str(), "");
        users_[i].passwordHash = p.getString(kH(i).c_str(), "");
        users_[i].role         = static_cast<UserRoleLevel>(p.getUChar(kR(i).c_str(), 1));
        users_[i].blocked      = p.getBool(kB(i).c_str(), false);
        users_[i].canSetRate   = p.getBool(kS(i).c_str(), false);
    }
    p.end();
}

void UserStore::seedDefaults() {
    Preferences p;
    p.begin("usrs", false);
    count_ = 3;

    auto seed = [&](uint8_t i, const char* user, const char* pass, UserRoleLevel role) {
        users_[i] = { user, hashPassword(pass), role, false, false };
        p.putString(kN(i).c_str(), users_[i].username);
        p.putString(kH(i).c_str(), users_[i].passwordHash);
        p.putUChar(kR(i).c_str(),  static_cast<uint8_t>(users_[i].role));
        p.putBool(kB(i).c_str(),   false);
        p.putBool(kS(i).c_str(),   false);
    };

    seed(0, "admin",        "0000", UserRoleLevel::Admin);
    seed(1, "operator",     "1234", UserRoleLevel::Operator);
    seed(2, "manufacturer", "5678", UserRoleLevel::Manufacturer);

    p.putUChar("cnt", count_);
    p.end();
    Serial.println("[USERS] Default users seeded");
}

void UserStore::persist(uint8_t i) {
    Preferences p;
    p.begin("usrs", false);
    p.putString(kN(i).c_str(), users_[i].username);
    p.putString(kH(i).c_str(), users_[i].passwordHash);
    p.putUChar(kR(i).c_str(),  static_cast<uint8_t>(users_[i].role));
    p.putBool(kB(i).c_str(),   users_[i].blocked);
    p.putBool(kS(i).c_str(),   users_[i].canSetRate);
    p.putUChar("cnt", count_);
    p.end();
}

int8_t UserStore::findIndex(const String& username) const {
    for (uint8_t i = 0; i < count_; i++)
        if (users_[i].username.equalsIgnoreCase(username)) return (int8_t)i;
    return -1;
}

bool UserStore::usernameExists(const String& username) const {
    return findIndex(username) >= 0;
}

bool UserStore::validateCredentials(const String& username, const String& password,
                                     UserRoleLevel& outRole, bool& outBlocked, bool& outCanSetRate) const {
    int8_t idx = findIndex(username);
    if (idx < 0) return false;
    if (users_[idx].passwordHash != hashPassword(password)) return false;
    outRole       = users_[idx].role;
    outBlocked    = users_[idx].blocked;
    outCanSetRate = users_[idx].canSetRate;
    return true;
}

bool UserStore::createUser(const String& username, const String& password,
                            UserRoleLevel role, bool canSetRate) {
    if (count_ >= kMaxUsers) return false;
    if (usernameExists(username)) return false;
    if (username.isEmpty() || password.isEmpty()) return false;

    uint8_t i = count_++;
    users_[i] = { username, hashPassword(password), role, false, canSetRate };
    persist(i);
    Serial.printf("[USERS] Created user: %s role=%u\n", username.c_str(), (uint8_t)role);
    return true;
}

bool UserStore::updatePassword(const String& username, const String& newPassword) {
    int8_t idx = findIndex(username);
    if (idx < 0) return false;
    users_[idx].passwordHash = hashPassword(newPassword);
    persist(idx);
    return true;
}

bool UserStore::setBlocked(const String& username, bool blocked) {
    int8_t idx = findIndex(username);
    if (idx < 0) return false;
    // Cannot block the admin account
    if (users_[idx].role == UserRoleLevel::Admin && blocked) return false;
    users_[idx].blocked = blocked;
    persist(idx);
    Serial.printf("[USERS] User %s %s\n", username.c_str(), blocked ? "blocked" : "unblocked");
    return true;
}

bool UserStore::setCanSetRate(const String& username, bool canSetRate) {
    int8_t idx = findIndex(username);
    if (idx < 0) return false;
    users_[idx].canSetRate = canSetRate;
    persist(idx);
    return true;
}

bool UserStore::deleteUser(const String& username) {
    int8_t idx = findIndex(username);
    if (idx < 0) return false;
    if (users_[idx].role == UserRoleLevel::Admin) return false;  // protect admin

    // Shift remaining users down
    for (uint8_t i = idx; i < count_ - 1; i++) {
        users_[i] = users_[i + 1];
        persist(i);
    }
    count_--;
    // Clear the now-stale last slot in NVS
    Preferences p;
    p.begin("usrs", false);
    p.remove(kN(count_).c_str());
    p.remove(kH(count_).c_str());
    p.remove(kR(count_).c_str());
    p.remove(kB(count_).c_str());
    p.remove(kS(count_).c_str());
    p.putUChar("cnt", count_);
    p.end();
    Serial.printf("[USERS] Deleted user: %s\n", username.c_str());
    return true;
}

static const char* roleLabel(UserRoleLevel r) {
    switch (r) {
        case UserRoleLevel::Admin:        return "admin";
        case UserRoleLevel::Manufacturer: return "manufacturer";
        default:                          return "operator";
    }
}

String UserStore::listJson() const {
    String j = "[";
    for (uint8_t i = 0; i < count_; i++) {
        if (i > 0) j += ",";
        j += "{";
        j += "\"username\":\"" + users_[i].username + "\",";
        j += "\"role\":\"" + String(roleLabel(users_[i].role)) + "\",";
        j += "\"blocked\":" + String(users_[i].blocked ? "true" : "false") + ",";
        j += "\"canSetRate\":" + String(users_[i].canSetRate ? "true" : "false");
        j += "}";
    }
    j += "]";
    return j;
}

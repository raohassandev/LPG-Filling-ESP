#include "UserStore.h"
#include <mbedtls/sha256.h>
#include <esp_system.h>
#include <esp_efuse.h>
#include <esp_mac.h>

// NVS key helpers — all keys ≤ 15 chars
static String kN(uint8_t i) { return "u" + String(i) + "n"; }
static String kH(uint8_t i) { return "u" + String(i) + "h"; }
static String kSl(uint8_t i) { return "u" + String(i) + "sl"; }
static String kR(uint8_t i) { return "u" + String(i) + "r"; }
static String kB(uint8_t i) { return "u" + String(i) + "b"; }
static String kS(uint8_t i) { return "u" + String(i) + "s"; }

String UserStore::generateSalt() {
    uint8_t bytes[8];
    esp_fill_random(bytes, sizeof(bytes));
    char hex[17]; hex[16] = '\0';
    for (int i = 0; i < 8; i++) snprintf(hex + i*2, 3, "%02X", bytes[i]);
    return String(hex);
}

// SHA256(salt + ":" + password), returns 64-char hex string
String UserStore::hashPassword(const String& password, const String& salt) {
    const String input = salt + ":" + password;
    uint8_t hash[32];
    mbedtls_sha256((const unsigned char*)input.c_str(), input.length(), hash, 0);
    char hex[65]; hex[64] = '\0';
    for (int i = 0; i < 32; i++) snprintf(hex + i*2, 3, "%02x", hash[i]);
    return String(hex);
}

void UserStore::begin() {
    load();
    if (count_ == 0) {
        seedFirstBoot();
    }
    Serial.printf("[USERS] %u user(s) loaded\n", count_);
}

// seedFirstBoot: generate a unique random admin password at first boot.
// Password is printed to Serial and shown on OLED (caller must display).
// NO hardcoded passwords. Only admin user is created — operator/manufacturer
// accounts must be created by the admin after initial login.
void UserStore::seedFirstBoot() {
    // Generate a random 8-char hex password from hardware RNG
    uint8_t rnd[4];
    esp_fill_random(rnd, sizeof(rnd));
    char pwBuf[9]; pwBuf[8] = '\0';
    snprintf(pwBuf, sizeof(pwBuf), "%02X%02X%02X%02X", rnd[0], rnd[1], rnd[2], rnd[3]);
    const String pw = String(pwBuf);

    const String salt = generateSalt();
    const String hash = hashPassword(pw, salt);

    Preferences p;
    p.begin("usrs", false);
    count_ = 1;
    users_[0] = { "admin", hash, salt, UserRoleLevel::Admin, false, false };
    p.putString(kN(0).c_str(),  users_[0].username);
    p.putString(kH(0).c_str(),  users_[0].passwordHash);
    p.putString(kSl(0).c_str(), users_[0].salt);
    p.putUChar(kR(0).c_str(),   static_cast<uint8_t>(users_[0].role));
    p.putBool(kB(0).c_str(),    false);
    p.putBool(kS(0).c_str(),    false);
    p.putUChar("cnt", count_);
    p.end();

    // Print to serial — operator must record this
    Serial.println(F("\n[SETUP] ===================================================="));
    Serial.println(F("[SETUP] FIRST BOOT — Admin account created"));
    Serial.print(F("[SETUP] Username : admin"));
    Serial.println();
    Serial.print(F("[SETUP] Password : "));
    Serial.println(pw);
    Serial.println(F("[SETUP] RECORD THIS PASSWORD — it cannot be recovered."));
    Serial.println(F("[SETUP] Change it immediately after first login."));
    Serial.println(F("[SETUP] ====================================================\n"));

    // Store temporarily so caller can show it on OLED
    firstBootPassword_ = pw;
}

void UserStore::load() {
    Preferences p;
    p.begin("usrs", true);
    count_ = p.getUChar("cnt", 0);
    if (count_ > kMaxUsers) count_ = 0;
    for (uint8_t i = 0; i < count_; i++) {
        users_[i].username     = p.getString(kN(i).c_str(), "");
        users_[i].passwordHash = p.getString(kH(i).c_str(), "");
        users_[i].salt         = p.getString(kSl(i).c_str(), "");
        users_[i].role         = static_cast<UserRoleLevel>(p.getUChar(kR(i).c_str(), 1));
        users_[i].blocked      = p.getBool(kB(i).c_str(), false);
        users_[i].canSetRate   = p.getBool(kS(i).c_str(), false);
    }
    p.end();
}

void UserStore::persist(uint8_t i) {
    Preferences p;
    p.begin("usrs", false);
    p.putString(kN(i).c_str(),  users_[i].username);
    p.putString(kH(i).c_str(),  users_[i].passwordHash);
    p.putString(kSl(i).c_str(), users_[i].salt);
    p.putUChar(kR(i).c_str(),   static_cast<uint8_t>(users_[i].role));
    p.putBool(kB(i).c_str(),    users_[i].blocked);
    p.putBool(kS(i).c_str(),    users_[i].canSetRate);
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
    const String& storedHash = users_[idx].passwordHash;
    const String& salt       = users_[idx].salt;
    // Handle legacy djb2 hashes (8 uppercase hex chars) — reject them, force password reset
    if (storedHash.length() == 8) {
        Serial.printf("[USERS] Legacy hash detected for %s — password reset required\n", username.c_str());
        return false;
    }
    if (storedHash != hashPassword(password, salt)) return false;
    outRole       = users_[idx].role;
    outBlocked    = users_[idx].blocked;
    outCanSetRate = users_[idx].canSetRate;
    return true;
}

bool UserStore::createUser(const String& username, const String& password,
                            UserRoleLevel role, bool canSetRate) {
    if (count_ >= kMaxUsers) return false;
    if (usernameExists(username)) return false;
    if (username.isEmpty() || password.length() < 4) return false;
    const String salt = generateSalt();
    uint8_t i = count_++;
    users_[i] = { username, hashPassword(password, salt), salt, role, false, canSetRate };
    persist(i);
    Serial.printf("[USERS] Created user: %s role=%u\n", username.c_str(), (uint8_t)role);
    return true;
}

bool UserStore::updatePassword(const String& username, const String& newPassword) {
    int8_t idx = findIndex(username);
    if (idx < 0) return false;
    if (newPassword.length() < 4) return false;
    users_[idx].salt         = generateSalt();
    users_[idx].passwordHash = hashPassword(newPassword, users_[idx].salt);
    persist(idx);
    return true;
}

bool UserStore::setBlocked(const String& username, bool blocked) {
    int8_t idx = findIndex(username);
    if (idx < 0) return false;
    if (users_[idx].role == UserRoleLevel::Admin && blocked) return false;
    users_[idx].blocked = blocked;
    persist(idx);
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
    if (users_[idx].role == UserRoleLevel::Admin) return false;
    for (uint8_t i = idx; i < count_ - 1; i++) {
        users_[i] = users_[i + 1];
        persist(i);
    }
    count_--;
    Preferences p;
    p.begin("usrs", false);
    p.remove(kN(count_).c_str()); p.remove(kH(count_).c_str()); p.remove(kSl(count_).c_str());
    p.remove(kR(count_).c_str()); p.remove(kB(count_).c_str()); p.remove(kS(count_).c_str());
    p.putUChar("cnt", count_);
    p.end();
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
        j += "{\"username\":\"" + users_[i].username + "\",";
        j += "\"role\":\"" + String(roleLabel(users_[i].role)) + "\",";
        j += "\"blocked\":" + String(users_[i].blocked ? "true" : "false") + ",";
        j += "\"canSetRate\":" + String(users_[i].canSetRate ? "true" : "false") + "}";
    }
    j += "]"; return j;
}

const String& UserStore::firstBootPassword() const { return firstBootPassword_; }
bool UserStore::hasFirstBootPassword()         const { return !firstBootPassword_.isEmpty(); }
void UserStore::clearFirstBootPassword()              { firstBootPassword_ = ""; }

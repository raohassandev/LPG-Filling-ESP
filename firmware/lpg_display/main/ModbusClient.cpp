#include "ModbusClient.h"
#include "DisplayConfig.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

static const char* TAG = "MBUS";

static inline int64_t now_us() { return esp_timer_get_time(); }

float ModbusClient::regsToFloat(uint16_t hi, uint16_t lo) {
    uint32_t bits = ((uint32_t)hi << 16) | lo;
    float f;
    memcpy(&f, &bits, sizeof(f));
    return f;
}

void ModbusClient::begin() {
    begin(DisplaySettingsStore::load().rtu);
}

void ModbusClient::begin(const DisplayRtuSettings& rtu) {
    if (!busMutex_) busMutex_ = xSemaphoreCreateRecursiveMutex();
    rtu_ = rtu;

    uart_config_t cfg = {
        .baud_rate  = static_cast<int>(rtu_.baudRate),
        .data_bits  = UART_DATA_8_BITS,
        .parity     = rtu_.parity == 1 ? UART_PARITY_EVEN :
                      rtu_.parity == 2 ? UART_PARITY_ODD : UART_PARITY_DISABLE,
        .stop_bits  = rtu_.stopBits == 2 ? UART_STOP_BITS_2 : UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(kRtuUart, 512, 0, 0, NULL, 0);
    uart_param_config(kRtuUart, &cfg);
    uart_set_pin(kRtuUart, kRtuTxPin, kRtuRxPin,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    ESP_LOGI(TAG, "Modbus RTU UART%d TX=%d RX=%d slave=%u baud=%lu parity=%u stop=%u timeout=%u retries=%u",
             (int)kRtuUart, (int)kRtuTxPin, (int)kRtuRxPin, rtu_.slaveAddress,
             static_cast<unsigned long>(rtu_.baudRate), rtu_.parity, rtu_.stopBits,
             rtu_.timeoutMs, rtu_.retries);
}

void ModbusClient::applySettings(const DisplayRtuSettings& rtu) {
    bool locked = false;
    const int timeout = rtu_.timeoutMs > 0 ? rtu_.timeoutMs : kDefaultTimeoutMs;
    if (busMutex_ && xSemaphoreTakeRecursive(busMutex_, pdMS_TO_TICKS(timeout + 300)) == pdTRUE) {
        locked = true;
    }

    rtu_ = rtu;
    uart_config_t cfg = {
        .baud_rate  = static_cast<int>(rtu_.baudRate),
        .data_bits  = UART_DATA_8_BITS,
        .parity     = rtu_.parity == 1 ? UART_PARITY_EVEN :
                      rtu_.parity == 2 ? UART_PARITY_ODD : UART_PARITY_DISABLE,
        .stop_bits  = rtu_.stopBits == 2 ? UART_STOP_BITS_2 : UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(kRtuUart, &cfg);
    uart_flush_input(kRtuUart);
    snap_.commFailStreak = 0;
    snap_.commOkStreak = 0;
    snap_.lastOkUs = 0;
    snap_.lastFailUs = 0;
    snap_.connected = false;
    snap_.valid = false;
    snap_.commHealth = CommHealth::Offline;
    bootDeviceIdChecked_ = false;
    healthLogged_ = false;

    if (locked && busMutex_) xSemaphoreGiveRecursive(busMutex_);
    ESP_LOGI(TAG, "Applied RTU slave=%u baud=%lu parity=%u stop=%u timeout=%u retries=%u",
             rtu_.slaveAddress, static_cast<unsigned long>(rtu_.baudRate),
             rtu_.parity, rtu_.stopBits, rtu_.timeoutMs, rtu_.retries);
}

void ModbusClient::noteCommOk() {
    const int64_t now = now_us();
    if (snap_.commOkStreak < UINT16_MAX) snap_.commOkStreak++;
    snap_.commFailStreak = 0;
    snap_.connected = true;
    snap_.valid = true;
    snap_.lastOkUs = now;
    updateCommHealth();
}

void ModbusClient::noteCommFail() {
    const int64_t now = now_us();
    if (snap_.commFailStreak < UINT16_MAX) snap_.commFailStreak++;
    snap_.lastFailUs = now;
    updateCommHealth();
}

void ModbusClient::updateCommHealth() {
    const int64_t now = now_us();
    const int64_t offlineUs = static_cast<int64_t>(rtu_.offlineDebounceMs) * 1000;
    const int64_t unstableUs = static_cast<int64_t>(rtu_.unstableDebounceMs) * 1000;
    if (snap_.lastOkUs == 0 || (now - snap_.lastOkUs) > offlineUs) {
        snap_.connected = false;
        snap_.commHealth = CommHealth::Offline;
        logHealthIfChanged();
        return;
    }
    snap_.connected = true;
    snap_.commHealth = (snap_.commFailStreak >= 2 ||
                       (snap_.lastFailUs > 0 && (now - snap_.lastFailUs) < unstableUs))
                       ? CommHealth::Unstable
                       : CommHealth::Online;
    logHealthIfChanged();
}

void ModbusClient::logHealthIfChanged() {
    if (healthLogged_ && snap_.commHealth == lastLoggedHealth_) return;
    lastLoggedHealth_ = snap_.commHealth;
    healthLogged_ = true;
    const char* text = "OFFLINE";
    if (snap_.commHealth == CommHealth::Online) text = "ONLINE";
    else if (snap_.commHealth == CommHealth::Unstable) text = "UNSTABLE";
    ESP_LOGI(TAG, "RTU health=%s ok=%u fail=%u last_ok_us=%lld last_fail_us=%lld",
             text, snap_.commOkStreak, snap_.commFailStreak,
             static_cast<long long>(snap_.lastOkUs),
             static_cast<long long>(snap_.lastFailUs));
}

void ModbusClient::poll() {
    const int64_t now = now_us();

    if (!bootDeviceIdChecked_ && now > 3000000) {
        bootDeviceIdChecked_ = true;
        uint16_t id = 0;
        if (readDeviceId(id)) {
            ESP_LOGI(TAG, "Device ID test: read 0x%04X (%s)", id, id == 0xA601 ? "OK" : "MISMATCH");
        } else {
            ESP_LOGW(TAG, "Device ID test: no response from register 0x0018");
        }
    }

    // Fast — weights, state, flags (regs 0x0000–0x0017, 24 regs)
    if (now - lastFastUs_ >= kFastUs) {
        lastFastUs_ = now;
        uint16_t r[24] = {};
        if (readHRRetry(0x0000, 24, r)) {
            snap_.liveWeightKg    = regsToFloat(r[0x00], r[0x01]);
            snap_.tareWeightKg    = regsToFloat(r[0x02], r[0x03]);
            snap_.netWeightKg     = regsToFloat(r[0x04], r[0x05]);
            snap_.targetWeightKg  = regsToFloat(r[0x06], r[0x07]);
            snap_.ratePerKg       = regsToFloat(r[0x08], r[0x09]);
            snap_.targetAmount    = regsToFloat(r[0x0A], r[0x0B]);
            snap_.currentAmount   = regsToFloat(r[0x0C], r[0x0D]);
            snap_.state           = static_cast<FillState>(r[0x0E]);
            snap_.eStopOk         = r[0x0F] != 0;
            snap_.cylinderPresent = r[0x10] != 0;
            snap_.nozzleEngaged   = r[0x11] != 0;
            snap_.weightStable    = r[0x12] != 0;
        } else {
            updateCommHealth();
        }
    }

    // Slow — RTC (regs 0x0020–0x0025)
    if (now - lastSlowUs_ >= kSlowUs) {
        lastSlowUs_ = now;
        uint16_t r[6] = {};
        if (readHRRetry(0x0020, 6, r)) {
            snap_.rtcYear   = r[0];
            snap_.rtcMonth  = r[1];
            snap_.rtcDay    = r[2];
            snap_.rtcHour   = r[3];
            snap_.rtcMinute = r[4];
            snap_.rtcSecond = r[5];
        }
    }

    // Diagnostics / alarm summary (regs 0x0048-0x004B)
    if (now - lastDiagUs_ >= kDiagUs) {
        lastDiagUs_ = now;
        uint16_t r[4] = {};
        if (readHRRetry(0x0048, 4, r)) {
            snap_.alarmCode     = r[0];
            snap_.alarmSeverity = r[1];
            snap_.readinessMask = r[2];
            snap_.blockerMask   = r[3];
        }
    }

    // Stats — today (regs 0x0030–0x0035)
    if (now - lastStatUs_ >= kStatUs) {
        lastStatUs_ = now;
        uint16_t r[6] = {};
        if (readHRRetry(0x0030, 6, r)) {
            snap_.todayFills  = r[0];
            snap_.todayFails  = r[1];
            snap_.todayKg     = regsToFloat(r[2], r[3]);
            snap_.todayAmount = regsToFloat(r[4], r[5]);
        }
    }
}

bool ModbusClient::writeRegister(uint16_t reg, uint16_t val) {
    uint8_t req[8];
    req[0] = rtu_.slaveAddress;
    req[1] = 0x06;
    req[2] = reg >> 8;   req[3] = reg & 0xFF;
    req[4] = val >> 8;   req[5] = val & 0xFF;
    uint16_t c = crc16(req, 6);
    req[6] = c & 0xFF;   req[7] = c >> 8;
    uint8_t resp[8];
    return sendRecv(req, 8, resp, 8);
}

bool ModbusClient::writeRegisters(uint16_t startReg, const uint16_t* values, uint16_t count) {
    if (!values || count == 0 || count > 123) return false;

    uint8_t req[256];
    const int byteCount = count * 2;
    const int reqLen = 9 + byteCount;
    req[0] = rtu_.slaveAddress;
    req[1] = 0x10;
    req[2] = startReg >> 8; req[3] = startReg & 0xFF;
    req[4] = count >> 8;    req[5] = count & 0xFF;
    req[6] = byteCount;
    for (uint16_t i = 0; i < count; i++) {
        req[7 + i * 2] = values[i] >> 8;
        req[8 + i * 2] = values[i] & 0xFF;
    }
    uint16_t c = crc16(req, reqLen - 2);
    req[reqLen - 2] = c & 0xFF;
    req[reqLen - 1] = c >> 8;

    uint8_t resp[8];
    return sendRecv(req, reqLen, resp, 8);
}

bool ModbusClient::writeFloat(uint16_t startReg, float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    const uint16_t regs[2] = {
        static_cast<uint16_t>(bits >> 16),
        static_cast<uint16_t>(bits & 0xFFFF),
    };
    return writeRegisters(startReg, regs, 2);
}

bool ModbusClient::readDeviceId(uint16_t& deviceId) {
    uint16_t r = 0;
    if (!readHRRetry(0x0018, 1, &r, 1)) return false;
    deviceId = r;
    return true;
}

bool ModbusClient::readHRRetry(uint16_t start, uint16_t count, uint16_t* out, uint8_t attempts) {
    if (attempts == 0) attempts = rtu_.retries > 0 ? rtu_.retries : kDefaultReadAttempts;
    for (uint8_t i = 0; i < attempts; ++i) {
        if (readHR(start, count, out)) {
            noteCommOk();
            return true;
        }
        if (i + 1 < attempts) vTaskDelay(pdMS_TO_TICKS(40 + i * 40));
    }
    noteCommFail();
    return false;
}

bool ModbusClient::writeRegisterRetry(uint16_t reg, uint16_t val, uint8_t attempts) {
    if (attempts == 0) attempts = rtu_.retries > 0 ? static_cast<uint8_t>(rtu_.retries + 1) : kDefaultWriteAttempts;
    for (uint8_t i = 0; i < attempts; ++i) {
        if (writeRegister(reg, val)) {
            noteCommOk();
            return true;
        }
        if (i + 1 < attempts) vTaskDelay(pdMS_TO_TICKS(50 + i * 50));
    }
    noteCommFail();
    return false;
}

bool ModbusClient::writeRegistersRetry(uint16_t startReg, const uint16_t* values,
                                       uint16_t count, uint8_t attempts) {
    if (attempts == 0) attempts = rtu_.retries > 0 ? static_cast<uint8_t>(rtu_.retries + 1) : kDefaultWriteAttempts;
    for (uint8_t i = 0; i < attempts; ++i) {
        if (writeRegisters(startReg, values, count)) {
            noteCommOk();
            return true;
        }
        if (i + 1 < attempts) vTaskDelay(pdMS_TO_TICKS(50 + i * 50));
    }
    noteCommFail();
    return false;
}

bool ModbusClient::confirmFillStarted(uint32_t waitMs) {
    const uint32_t loops = waitMs / 150;
    for (uint32_t i = 0; i < loops; ++i) {
        uint16_t r[24] = {};
        if (readHRRetry(0x0000, 24, r, 1)) {
            snap_.liveWeightKg    = regsToFloat(r[0x00], r[0x01]);
            snap_.tareWeightKg    = regsToFloat(r[0x02], r[0x03]);
            snap_.netWeightKg     = regsToFloat(r[0x04], r[0x05]);
            snap_.targetWeightKg  = regsToFloat(r[0x06], r[0x07]);
            snap_.ratePerKg       = regsToFloat(r[0x08], r[0x09]);
            snap_.targetAmount    = regsToFloat(r[0x0A], r[0x0B]);
            snap_.currentAmount   = regsToFloat(r[0x0C], r[0x0D]);
            snap_.state           = static_cast<FillState>(r[0x0E]);
            snap_.eStopOk         = r[0x0F] != 0;
            snap_.cylinderPresent = r[0x10] != 0;
            snap_.nozzleEngaged   = r[0x11] != 0;
            snap_.weightStable    = r[0x12] != 0;
            if (snap_.state == FillState::Validating ||
                snap_.state == FillState::Fast ||
                snap_.state == FillState::Slow ||
                snap_.state == FillState::Settling ||
                snap_.state == FillState::Complete) {
                return true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(150));
    }
    return false;
}

bool ModbusClient::readPresetRegisters(float& targetWeightKg, float& ratePerKg, float& targetAmount) {
    uint16_t r[6] = {};
    if (!readHRRetry(0x0006, 6, r, 2)) return false;
    targetWeightKg = regsToFloat(r[0], r[1]);
    ratePerKg = regsToFloat(r[2], r[3]);
    targetAmount = regsToFloat(r[4], r[5]);
    snap_.targetWeightKg = targetWeightKg;
    snap_.ratePerKg = ratePerKg;
    snap_.targetAmount = targetAmount;
    return true;
}

bool ModbusClient::startFill(float targetWeightKg, float ratePerKg) {
    bool locked = false;
    if (busMutex_) {
        if (xSemaphoreTakeRecursive(busMutex_, pdMS_TO_TICKS(rtu_.timeoutMs + 400)) != pdTRUE) {
            ESP_LOGW(TAG, "Start fill failed: RTU bus busy");
            return false;
        }
        locked = true;
    }
    auto unlock = [&]() {
        if (locked && busMutex_) {
            xSemaphoreGiveRecursive(busMutex_);
            locked = false;
        }
    };

    if (targetWeightKg <= 0.0f || targetWeightKg > 500.0f ||
        ratePerKg <= 0.0f || ratePerKg > 100000.0f) {
        ESP_LOGW(TAG, "Start fill rejected locally: target=%.3f rate=%.2f",
                 targetWeightKg, ratePerKg);
        unlock();
        return false;
    }

    const float targetAmount = targetWeightKg * ratePerKg;
    ESP_LOGI(TAG, "Preset sync write target=%.3f rate=%.3f amount=%.3f",
             targetWeightKg, ratePerKg, targetAmount);
    uint32_t targetBits, rateBits, amountBits;
    memcpy(&targetBits, &targetWeightKg, sizeof(targetBits));
    memcpy(&rateBits, &ratePerKg, sizeof(rateBits));
    memcpy(&amountBits, &targetAmount, sizeof(amountBits));

    const uint16_t targetRegs[2] = {
        static_cast<uint16_t>(targetBits >> 16),
        static_cast<uint16_t>(targetBits & 0xFFFF),
    };
    const uint16_t rateRegs[2] = {
        static_cast<uint16_t>(rateBits >> 16),
        static_cast<uint16_t>(rateBits & 0xFFFF),
    };
    const uint16_t amountRegs[2] = {
        static_cast<uint16_t>(amountBits >> 16),
        static_cast<uint16_t>(amountBits & 0xFFFF),
    };

    // Keep these as separate FC16 writes. The controller applies each float when
    // its low word arrives, using the latest persisted values for the other fields.
    if (!writeRegistersRetry(0x0006, targetRegs, 2)) {
        ESP_LOGW(TAG, "Start fill failed: target write not confirmed");
        unlock();
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(80));
    if (!writeRegistersRetry(0x0008, rateRegs, 2)) {
        ESP_LOGW(TAG, "Start fill failed: rate write not confirmed");
        unlock();
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(80));
    if (!writeRegistersRetry(0x000A, amountRegs, 2)) {
        ESP_LOGW(TAG, "Start fill failed: amount write not confirmed");
        unlock();
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(80));

    float readTarget = 0.0f;
    float readRate = 0.0f;
    float readAmount = 0.0f;
    if (!readPresetRegisters(readTarget, readRate, readAmount)) {
        ESP_LOGW(TAG, "Start fill failed: preset readback did not get RTU response");
        unlock();
        return false;
    }
    ESP_LOGI(TAG, "Preset sync readback target=%.3f rate=%.3f amount=%.3f",
             readTarget, readRate, readAmount);
    const bool presetOk = fabsf(readTarget - targetWeightKg) <= 0.01f &&
                          fabsf(readRate - ratePerKg) <= 0.05f &&
                          fabsf(readAmount - targetAmount) <= 0.50f;
    if (!presetOk) {
        ESP_LOGW(TAG,
                 "Preset sync mismatch targetWrite=%.3f targetRead=%.3f rateWrite=%.3f rateRead=%.3f amountWrite=%.3f amountRead=%.3f",
                 targetWeightKg, readTarget, ratePerKg, readRate, targetAmount, readAmount);
        unlock();
        return false;
    }
    ESP_LOGI(TAG, "Preset sync OK");

    if (!writeRegisterRetry(0x0017, 1)) {
        ESP_LOGW(TAG, "Start command response missing; checking controller state");
        if (confirmFillStarted(900)) {
            ESP_LOGW(TAG, "Start command accepted despite missing RTU response");
            unlock();
            return true;
        }
        unlock();
        return false;
    }

    if (!confirmFillStarted(900)) {
        ESP_LOGW(TAG, "Start command written but fill state not confirmed");
        unlock();
        return false;
    }

    ESP_LOGI(TAG, "Start fill confirmed: target=%.3fkg rate=%.2f amount=%.2f",
             targetWeightKg, ratePerKg, targetAmount);
    unlock();
    return true;
}

bool ModbusClient::readHR(uint16_t start, uint16_t count, uint16_t* out) {
    uint8_t req[8];
    req[0] = rtu_.slaveAddress; req[1] = 0x03;
    req[2] = start >> 8; req[3] = start & 0xFF;
    req[4] = count >> 8; req[5] = count & 0xFF;
    uint16_t c = crc16(req, 6);
    req[6] = c & 0xFF; req[7] = c >> 8;

    int expected = 5 + count * 2;
    uint8_t resp[256];
    if (!sendRecv(req, 8, resp, expected)) return false;

    for (int i = 0; i < count; i++)
        out[i] = ((uint16_t)resp[3 + i * 2] << 8) | resp[4 + i * 2];
    return true;
}

bool ModbusClient::sendRecv(const uint8_t* req, int reqLen,
                              uint8_t* resp, int expectLen) {
    const int configuredTimeoutMs = rtu_.timeoutMs > 0 ? rtu_.timeoutMs : kDefaultTimeoutMs;
    const int timeoutMs = configuredTimeoutMs < kDefaultTimeoutMs ? kDefaultTimeoutMs : configuredTimeoutMs;
    if (busMutex_ && xSemaphoreTakeRecursive(busMutex_, pdMS_TO_TICKS(timeoutMs + 100)) != pdTRUE) {
        ESP_LOGW(TAG, "RTU busy req=%02X %02X", reqLen > 0 ? req[0] : 0, reqLen > 1 ? req[1] : 0);
        return false;
    }

    static int64_t lastDiagUs = 0;
    auto diag = [&](const char* reason, int rx) {
        const int64_t now = now_us();
        if (now - lastDiagUs < 2000000) return;
        lastDiagUs = now;
        ESP_LOGW(TAG, "RTU %s rx=%d expect=%d req=%02X %02X %02X %02X %02X %02X %02X %02X",
                 reason, rx, expectLen,
                 reqLen > 0 ? req[0] : 0, reqLen > 1 ? req[1] : 0,
                 reqLen > 2 ? req[2] : 0, reqLen > 3 ? req[3] : 0,
                 reqLen > 4 ? req[4] : 0, reqLen > 5 ? req[5] : 0,
                 reqLen > 6 ? req[6] : 0, reqLen > 7 ? req[7] : 0);
        if (rx > 0) {
            ESP_LOGW(TAG, "RTU bytes %02X %02X %02X %02X %02X %02X %02X %02X",
                     rx > 0 ? resp[0] : 0, rx > 1 ? resp[1] : 0,
                     rx > 2 ? resp[2] : 0, rx > 3 ? resp[3] : 0,
                     rx > 4 ? resp[4] : 0, rx > 5 ? resp[5] : 0,
                     rx > 6 ? resp[6] : 0, rx > 7 ? resp[7] : 0);
        }
    };

    auto validFrame = [&]() {
        uint16_t rxCrc   = (uint16_t)resp[expectLen-2] | ((uint16_t)resp[expectLen-1] << 8);
        uint16_t calcCrc = crc16(resp, expectLen - 2);
        if (rxCrc != calcCrc) return false;
        if (resp[0] != rtu_.slaveAddress) return false;
        if (resp[1] & 0x80) return true;  // valid Modbus exception frame
        return resp[1] == req[1];
    };

    uint8_t buf[320] = {};
    int len = 0;
    const int64_t deadline = now_us() + (int64_t)timeoutMs * 1000;

    uart_flush_input(kRtuUart);
    uart_write_bytes(kRtuUart, req, reqLen);
    uart_wait_tx_done(kRtuUart, pdMS_TO_TICKS(50));

    while (now_us() < deadline) {
        const int room = (int)sizeof(buf) - len;
        if (room <= 0) break;
        int got = uart_read_bytes(kRtuUart, buf + len, room, pdMS_TO_TICKS(10));
        if (got > 0) len += got;

        // Half-duplex RS485 can echo our TX bytes. Drop a complete echoed request
        // only when the expected response shape differs or extra bytes follow.
        if (reqLen > 0 && len >= reqLen &&
            (reqLen != expectLen || len > expectLen) &&
            memcmp(buf, req, reqLen) == 0) {
            memmove(buf, buf + reqLen, len - reqLen);
            len -= reqLen;
        }

        while (len >= expectLen) {
            memcpy(resp, buf, expectLen);
            if (validFrame()) {
                if (resp[1] & 0x80) {
                    diag("exception", expectLen);
                    if (busMutex_) xSemaphoreGiveRecursive(busMutex_);
                    return false;
                }
                if (busMutex_) xSemaphoreGiveRecursive(busMutex_);
                return true;
            }
            memmove(buf, buf + 1, len - 1);
            len--;
        }

        if (len >= 5 && buf[0] == rtu_.slaveAddress && buf[1] == (req[1] | 0x80)) {
            const uint16_t rxCrc = (uint16_t)buf[3] | ((uint16_t)buf[4] << 8);
            const uint16_t calcCrc = crc16(buf, 3);
            if (rxCrc == calcCrc) {
                memcpy(resp, buf, 5);
                diag("exception", 5);
                if (busMutex_) xSemaphoreGiveRecursive(busMutex_);
                return false;
            }
        }
    }

    if (len > 0) {
        const int copy = len < expectLen ? len : expectLen;
        memcpy(resp, buf, copy);
        diag(len < expectLen ? "short-frame" : "crc-error", len);
    } else {
        diag("timeout/no-rx", 0);
    }
    if (busMutex_) xSemaphoreGiveRecursive(busMutex_);
    return false;
}

uint16_t ModbusClient::crc16(const uint8_t* data, int len) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
    }
    return crc;
}

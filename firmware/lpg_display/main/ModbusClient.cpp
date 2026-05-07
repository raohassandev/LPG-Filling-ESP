#include "ModbusClient.h"
#include "DisplayConfig.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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
    uart_config_t cfg = {
        .baud_rate  = kRtuBaud,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(kRtuUart, 512, 0, 0, NULL, 0);
    uart_param_config(kRtuUart, &cfg);
    uart_set_pin(kRtuUart, kRtuTxPin, kRtuRxPin,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    ESP_LOGI(TAG, "Modbus RTU UART%d TX=%d RX=%d baud=%d",
             (int)kRtuUart, (int)kRtuTxPin, (int)kRtuRxPin, kRtuBaud);
}

void ModbusClient::poll() {
    const int64_t now = now_us();

    // Fast — weights, state, flags (regs 0x0000–0x0017, 24 regs)
    if (now - lastFastUs_ >= kFastUs) {
        lastFastUs_ = now;
        uint16_t r[24] = {};
        if (readHR(0x0000, 24, r)) {
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
            snap_.valid           = true;
            snap_.connected       = true;
            snap_.lastOkUs        = now;
        } else {
            if (now - snap_.lastOkUs > 3000000) snap_.connected = false;
        }
    }

    // Slow — RTC (regs 0x0020–0x0025)
    if (now - lastSlowUs_ >= kSlowUs) {
        lastSlowUs_ = now;
        uint16_t r[6] = {};
        if (readHR(0x0020, 6, r)) {
            snap_.rtcHour   = r[3];
            snap_.rtcMinute = r[4];
            snap_.rtcSecond = r[5];
        }
    }

    // Stats — today (regs 0x0030–0x0035)
    if (now - lastStatUs_ >= kStatUs) {
        lastStatUs_ = now;
        uint16_t r[6] = {};
        if (readHR(0x0030, 6, r)) {
            snap_.todayFills  = r[0];
            snap_.todayFails  = r[1];
            snap_.todayKg     = regsToFloat(r[2], r[3]);
            snap_.todayAmount = regsToFloat(r[4], r[5]);
        }
    }
}

bool ModbusClient::writeRegister(uint16_t reg, uint16_t val) {
    uint8_t req[8];
    req[0] = kRtuAddr;
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
    req[0] = kRtuAddr;
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

bool ModbusClient::startFill(float targetWeightKg, float ratePerKg) {
    if (targetWeightKg <= 0.0f || targetWeightKg > 500.0f ||
        ratePerKg <= 0.0f || ratePerKg > 100000.0f) {
        return false;
    }

    const float targetAmount = targetWeightKg * ratePerKg;
    uint32_t targetBits, rateBits, amountBits;
    memcpy(&targetBits, &targetWeightKg, sizeof(targetBits));
    memcpy(&rateBits, &ratePerKg, sizeof(rateBits));
    memcpy(&amountBits, &targetAmount, sizeof(amountBits));

    const uint16_t regs[6] = {
        static_cast<uint16_t>(targetBits >> 16),
        static_cast<uint16_t>(targetBits & 0xFFFF),
        static_cast<uint16_t>(rateBits >> 16),
        static_cast<uint16_t>(rateBits & 0xFFFF),
        static_cast<uint16_t>(amountBits >> 16),
        static_cast<uint16_t>(amountBits & 0xFFFF),
    };
    return writeRegisters(0x0006, regs, 6) && cmdStart();
}

bool ModbusClient::readHR(uint16_t start, uint16_t count, uint16_t* out) {
    uint8_t req[8];
    req[0] = kRtuAddr; req[1] = 0x03;
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
    uart_flush_input(kRtuUart);
    uart_write_bytes(kRtuUart, req, reqLen);
    uart_wait_tx_done(kRtuUart, pdMS_TO_TICKS(50));

    int rx = uart_read_bytes(kRtuUart, resp, expectLen, pdMS_TO_TICKS(kTimeoutMs));
    if (rx < expectLen) return false;

    uint16_t rxCrc   = (uint16_t)resp[rx-2] | ((uint16_t)resp[rx-1] << 8);
    uint16_t calcCrc = crc16(resp, rx - 2);
    if (rxCrc != calcCrc) return false;
    if (resp[0] != kRtuAddr) return false;
    if (resp[1] & 0x80) return false;
    if (resp[1] != req[1]) return false;
    return true;
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

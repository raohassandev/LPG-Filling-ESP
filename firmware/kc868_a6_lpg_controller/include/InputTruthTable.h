#pragma once

#include <Arduino.h>

// Input channel definitions for LPG filling system
// These map to the 6 inputs from the PCF8574 I2C expander (address 0x22)

enum class InputChannel : uint8_t
{
    kNozzleEngaged = 0,   // Input 0: Nozzle in holder (active high)
    kCylinderPresent = 1, // Input 1: Cylinder detected on scale
    kEmergencyStop = 2,   // Input 2: Emergency stop button
    kDoorInterlock = 3,   // Input 3: Cabinet door closed
    kPressureOk = 4,      // Input 4: System pressure OK
    kPowerOk = 5,         // Input 5: Power supply OK
};

// Input truth table configuration
struct InputConfig
{
    InputChannel channel;
    const char *name;
    const char *description;
    bool activeState;    // true = active high, false = active low
    bool normallyOpen;   // true = NO contact, false = NC contact
    uint16_t debounceMs; // Debounce time in milliseconds
    bool safetyCritical; // True if this is a safety input
};

// Global input configuration
static constexpr InputConfig kInputConfigs[] = {
    // Input 0: Nozzle Engaged
    {
        InputChannel::kNozzleEngaged,
        "nozzle_engaged",
        "Nozzle is properly seated in holder",
        true, // Active high
        true, // NO contact (presence = high)
        50,   // 50ms debounce
        false // Not safety critical
    },

    // Input 1: Cylinder Present
    {
        InputChannel::kCylinderPresent,
        "cylinder_present",
        "Cylinder detected on weighing platform",
        true, // Active high
        true, // NO contact
        100,  // 100ms debounce for stability
        false // Not safety critical
    },

    // Input 2: Emergency Stop
    {
        InputChannel::kEmergencyStop,
        "emergency_stop",
        "Emergency stop button not pressed (NC)",
        false, // Active low (NC = closed = normal)
        false, // NC contact
        10,    // 10ms debounce - critical
        true   // SAFETY CRITICAL
    },

    // Input 3: Door Interlock
    {
        InputChannel::kDoorInterlock,
        "door_interlock",
        "Cabinet door is closed (NC)",
        false, // Active low
        false, // NC contact
        50,    // 50ms debounce
        true   // SAFETY CRITICAL
    },

    // Input 4: Pressure OK
    {
        InputChannel::kPressureOk,
        "pressure_ok",
        "LPG system pressure within acceptable range",
        true, // Active high
        true, // NO contact
        200,  // 200ms debounce for pressure stability
        true  // SAFETY CRITICAL
    },

    // Input 5: Power OK
    {
        InputChannel::kPowerOk,
        "power_ok",
        "Power supply voltages OK",
        true, // Active high
        true, // NO contact
        100,  // 100ms debounce
        true  // SAFETY CRITICAL
    }};

// Relay output definitions
enum class RelayChannel : uint8_t
{
    kFastValve = 0, // Relay 0: Fast fill valve
    kSlowValve = 1, // Relay 1: Slow fill valve
    kMainValve = 2, // Relay 2: Main tank valve
    kPump = 3,      // Relay 3: Compressor/pump
    kAlarm = 4,     // Relay 4: Alarm horn
    kIndicator = 5, // Relay 5: Status indicator
};

// Relay truth table configuration
struct RelayConfig
{
    RelayChannel channel;
    const char *name;
    const char *description;
    bool activeState;      // true = energized to open, false = energized to close
    bool failSafePosition; // Position on power loss or fault
    bool bootDefault;      // Default state on boot
    bool safetyCritical;   // True if this is a safety output
};

// Global relay configuration
static constexpr RelayConfig kRelayConfigs[] = {
    // Relay 0: Fast Fill Valve
    {
        RelayChannel::kFastValve,
        "fast_valve",
        "Fast fill solenoid valve",
        true,  // Energized to open
        false, // Fail closed
        false, // Default off
        true   // SAFETY CRITICAL
    },

    // Relay 1: Slow Fill Valve
    {
        RelayChannel::kSlowValve,
        "slow_valve",
        "Slow fill solenoid valve",
        true,  // Energized to open
        false, // Fail closed
        false, // Default off
        true   // SAFETY CRITICAL
    },

    // Relay 2: Main Valve
    {
        RelayChannel::kMainValve,
        "main_valve",
        "Main tank supply valve",
        true,  // Energized to open
        false, // Fail closed
        false, // Default off
        true   // SAFETY CRITICAL
    },

    // Relay 3: Pump/Compressor
    {
        RelayChannel::kPump,
        "pump",
        "Compressor or pump control",
        true,  // Energized to run
        false, // Fail off
        false, // Default off
        true   // SAFETY CRITICAL
    },

    // Relay 4: Alarm
    {
        RelayChannel::kAlarm,
        "alarm",
        "Alarm horn or beacon",
        true,  // Energized to activate
        false, // Fail silent
        false, // Default off
        false  // Not safety critical
    },

    // Relay 5: Indicator
    {
        RelayChannel::kIndicator,
        "indicator",
        "Status indicator light",
        true,  // Energized to light
        false, // Fail off
        false, // Default off
        false  // Not safety critical
    }};

// Helper functions
inline const char *inputName(InputChannel ch)
{
    for (const auto &cfg : kInputConfigs)
    {
        if (cfg.channel == ch)
            return cfg.name;
    }
    return "unknown";
}

inline const char *relayName(RelayChannel ch)
{
    for (const auto &cfg : kRelayConfigs)
    {
        if (cfg.channel == ch)
            return cfg.name;
    }
    return "unknown";
}

inline bool isInputActiveLow(InputChannel ch)
{
    for (const auto &cfg : kInputConfigs)
    {
        if (cfg.channel == ch)
            return !cfg.activeState;
    }
    return false;
}

inline bool isRelayFailSafe(RelayChannel ch)
{
    for (const auto &cfg : kRelayConfigs)
    {
        if (cfg.channel == ch)
            return cfg.failSafePosition;
    }
    return false;
}
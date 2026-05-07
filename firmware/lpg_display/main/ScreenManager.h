#pragma once
#include "ModbusClient.h"
#include "WifiManager.h"

enum class Screen { Dashboard, FillProgress, FillComplete, Fault, Pin, Settings, Wifi };

class ScreenManager {
public:
    void begin(ModbusClient& mbus, WifiManager& wifi);
    void update(const ControllerSnapshot& snap, ModbusClient& mbus, WifiManager& wifi);
    void navigateTo(Screen s);
    Screen current() const { return current_; }

private:
    Screen current_ = Screen::Dashboard;
    Screen pending_ = Screen::Dashboard;

    void loadScreen(Screen s, const ControllerSnapshot& snap,
                    ModbusClient& mbus, WifiManager& wifi);
    Screen stateToScreen(FillState state) const;
};

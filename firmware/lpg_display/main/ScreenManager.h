#pragma once
#include "ModbusClient.h"

enum class Screen { Dashboard, FillProgress, FillComplete, Fault, Pin, Settings };

class ScreenManager {
public:
    void begin(ModbusClient& mbus);
    void update(const ControllerSnapshot& snap, ModbusClient& mbus);
    void navigateTo(Screen s);
    Screen current() const { return current_; }

private:
    Screen current_ = Screen::Dashboard;
    Screen pending_ = Screen::Dashboard;

    void loadScreen(Screen s, const ControllerSnapshot& snap, ModbusClient& mbus);
    Screen stateToScreen(FillState state) const;
};

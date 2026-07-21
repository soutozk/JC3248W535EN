#include "obd2_data.h"

#include <algorithm>

namespace ui {
namespace obd2 {

void SimulatedObdDataProvider::reset()
{
    rpm_ = 0;
    direction_ = 1;
    remainder_ms_ = 0;
}

void SimulatedObdDataProvider::tick(uint32_t elapsed_ms)
{
    static constexpr uint32_t kStepIntervalMs = 40;
    static constexpr int kRpmStep = 240;

    remainder_ms_ += elapsed_ms;
    while(remainder_ms_ >= kStepIntervalMs) {
        remainder_ms_ -= kStepIntervalMs;
        rpm_ += direction_ * kRpmStep;
        if(rpm_ >= 9000) {
            rpm_ = 9000;
            direction_ = -1;
        } else if(rpm_ <= 0) {
            rpm_ = 0;
            direction_ = 1;
        }
    }
}

int SimulatedObdDataProvider::get_rpm() const
{
    return rpm_;
}

int SimulatedObdDataProvider::get_speed() const
{
    return rpm_ / 100;
}

int SimulatedObdDataProvider::get_coolant_temperature() const
{
    return 82 + (rpm_ / 1800);
}

float SimulatedObdDataProvider::get_battery_voltage() const
{
    return 13.8f - (rpm_ >= 7500 ? 0.2f : 0.0f);
}

ObdConnectionState SimulatedObdDataProvider::connection_state() const
{
    return ObdConnectionState::Connected;
}

ObdDataProvider &data_provider()
{
    static SimulatedObdDataProvider provider;
    return provider;
}

} // namespace obd2
} // namespace ui

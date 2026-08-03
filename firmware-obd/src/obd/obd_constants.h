#pragma once

#include <stdint.h>

namespace obd {

constexpr uint16_t kShiftRpmMin = 2000;
constexpr uint16_t kShiftRpmMax = 9000;
constexpr uint16_t kShiftRpmDefault = 7500;
constexpr uint16_t kShiftRpmStep = 100;
constexpr uint16_t kShiftRpmHysteresis = 150;
constexpr int16_t kCoolantColdMaxC = 60;
constexpr int16_t kCoolantNormalMaxC = 100;
constexpr uint32_t kRpmStaleMs = 500;
constexpr uint32_t kSpeedStaleMs = 1000;
constexpr uint32_t kCoolantStaleMs = 5000;
constexpr uint32_t kFastFieldStaleMs = 1500;
constexpr uint32_t kSlowFieldStaleMs = 6000;
constexpr uint32_t kTelemetryUiPeriodMs = 40;

enum Field : uint16_t {
    Rpm = 1u << 0,
    Speed = 1u << 1,
    Coolant = 1u << 2,
    Throttle = 1u << 3,
    EngineLoad = 1u << 4,
    Voltage = 1u << 5,
    Fuel = 1u << 6,
    IntakeAir = 1u << 7,
    Map = 1u << 8,
    Maf = 1u << 9,
    Runtime = 1u << 10,
};

constexpr uint16_t kAllFields = (1u << 11) - 1;

} // namespace obd


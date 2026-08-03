#pragma once

#include <stdint.h>

namespace obd {
namespace settings {

struct ObdDisplaySettings {
    uint16_t shiftRpm;
};

void init();
ObdDisplaySettings get();
bool save(const ObdDisplaySettings &value);
bool restoreDefaults();
uint16_t clampShiftRpm(int value);

} // namespace settings
} // namespace obd


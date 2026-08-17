#pragma once

#include <stddef.h>
#include <stdint.h>

namespace obd {
namespace protocol {

constexpr uint16_t kMagic = 0x424f; // bytes: 'O', 'B'
constexpr uint8_t kVersion = 1;
constexpr uint8_t kTelemetryType = 1;
constexpr uint8_t kStatusType = 2;
constexpr uint8_t kDtcType = 3;

#pragma pack(push, 1)
struct TelemetryFrame {
    uint16_t magic;
    uint8_t version;
    uint8_t type;
    uint16_t size;
    uint32_t sequence;
    uint64_t sourceTimestampMs;
    uint16_t validMask;
    uint16_t supportedMask;
    uint16_t rpm;
    uint16_t speedKmh;
    int16_t coolantDeciC;
    uint16_t throttleCentiPercent;
    uint16_t loadCentiPercent;
    uint16_t voltageMilliVolt;
    uint16_t fuelCentiPercent;
    int16_t intakeDeciC;
    uint16_t mapDeciKpa;
    uint16_t mafCentiGps;
    uint32_t runtimeSeconds;
    uint16_t crc16;
};

struct StatusFrame {
    uint16_t magic;
    uint8_t version;
    uint8_t type;
    uint16_t size;
    uint32_t sequence;
    uint64_t sourceTimestampMs;
    uint8_t state;
    uint8_t flags; // bit 0 ELM, bit 1 ECU, bit 2 ESP32 BLE
    char protocol[8];
    uint16_t latencyMs;
    uint16_t timeouts;
    uint16_t lastError;
    uint16_t crc16;
};

struct DtcFrame {
    uint16_t magic;
    uint8_t version;
    uint8_t type;
    uint16_t size;
    uint32_t sequence;
    uint64_t sourceTimestampMs;
    uint8_t currentCount;
    uint8_t pendingCount;
    uint16_t reserved;
    char current[8][6];
    char pending[8][6];
    uint16_t crc16;
};
#pragma pack(pop)

static_assert(sizeof(TelemetryFrame) == 48, "Telemetry protocol layout changed");
static_assert(sizeof(StatusFrame) == 36, "Status protocol layout changed");
static_assert(sizeof(DtcFrame) == 120, "DTC protocol layout changed");

uint16_t crc16Ccitt(const uint8_t *data, size_t length);
bool validateTelemetry(const uint8_t *data, size_t length, TelemetryFrame &frame);
bool validateStatus(const uint8_t *data, size_t length, StatusFrame &frame);
bool validateDtc(const uint8_t *data, size_t length, DtcFrame &frame);

} // namespace protocol
} // namespace obd

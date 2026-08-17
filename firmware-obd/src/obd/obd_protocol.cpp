#include "obd_protocol.h"

#include <string.h>

namespace obd {
namespace protocol {

uint16_t crc16Ccitt(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xffff;
    for(size_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for(int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                                 : static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

template <typename T>
static bool validate(const uint8_t *data, size_t length, uint8_t type, T &frame)
{
    if(data == nullptr || length != sizeof(T)) return false;
    memcpy(&frame, data, sizeof(T));
    if(frame.magic != kMagic || frame.version != kVersion || frame.type != type ||
       frame.size != sizeof(T)) return false;
    return crc16Ccitt(data, sizeof(T) - sizeof(frame.crc16)) == frame.crc16;
}

bool validateTelemetry(const uint8_t *data, size_t length, TelemetryFrame &frame)
{
    return validate(data, length, kTelemetryType, frame);
}

bool validateStatus(const uint8_t *data, size_t length, StatusFrame &frame)
{
    return validate(data, length, kStatusType, frame);
}

bool validateDtc(const uint8_t *data, size_t length, DtcFrame &frame)
{
    if(!validate(data, length, kDtcType, frame)) return false;
    if(frame.currentCount > 8 || frame.pendingCount > 8) return false;
    for(int group = 0; group < 2; ++group) {
        const uint8_t count = group == 0 ? frame.currentCount : frame.pendingCount;
        const char (*codes)[6] = group == 0 ? frame.current : frame.pending;
        for(uint8_t i = 0; i < count; ++i) {
            if(codes[i][0] == '\0' || codes[i][5] != '\0') return false;
            if(codes[i][0] != 'P' && codes[i][0] != 'C' &&
               codes[i][0] != 'B' && codes[i][0] != 'U') return false;
            if(codes[i][1] < '0' || codes[i][1] > '3') return false;
            for(int j = 2; j < 5; ++j) {
                const char c = codes[i][j];
                const bool digit = c >= '0' && c <= '9';
                const bool hex = c >= 'A' && c <= 'F';
                if(!digit && !hex) return false;
            }
        }
    }
    return true;
}

} // namespace protocol
} // namespace obd

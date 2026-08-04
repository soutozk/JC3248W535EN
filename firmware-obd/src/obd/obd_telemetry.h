#pragma once

#include "obd_protocol.h"
#include <stdint.h>

namespace obd {

enum class ConnectionState : uint8_t {
    Disconnected = 0, ConnectingElm, InitializingElm, DetectingProtocol,
    CheckingSupportedPids, ConnectingEsp32, Ready, Degraded, Reconnecting, Error
};

struct ObdTelemetry {
    uint32_t sequence = 0;
    uint32_t receivedAtMs = 0;
    uint16_t validMask = 0;
    uint16_t supportedMask = 0;
    float rpm = 0;
    float speedKmh = 0;
    float coolantC = 0;
    float throttlePercent = 0;
    float engineLoadPercent = 0;
    float controlModuleVoltage = 0;
    float fuelLevelPercent = 0;
    float intakeAirC = 0;
    float mapKpa = 0;
    float mafGps = 0;
    uint32_t engineRuntimeSeconds = 0;
    uint32_t fieldReceivedAtMs[11] = {};
};

struct ConnectionStatus {
    ConnectionState state = ConnectionState::Disconnected;
    bool elmConnected = false;
    bool ecuConnected = false;
    bool esp32Connected = false;
    char protocol[9] = {};
    uint16_t latencyMs = 0;
    uint16_t timeouts = 0;
    uint16_t lastError = 0;
    uint32_t receivedAtMs = 0;
};

void telemetryInit();
bool acceptTelemetryFrame(const protocol::TelemetryFrame &frame, uint32_t nowMs);
bool acceptStatusFrame(const protocol::StatusFrame &frame, uint32_t nowMs);
ObdTelemetry telemetrySnapshot(uint32_t nowMs);
ConnectionStatus connectionSnapshot();
void telemetrySetBleConnected(bool connected);
void telemetrySimulate(uint32_t nowMs);
bool telemetryPresentationActive();
const char *connectionStateName(ConnectionState state);

} // namespace obd

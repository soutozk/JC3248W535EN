#include "obd_telemetry.h"
#include "obd_constants.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <math.h>
#include <string.h>

namespace obd {

static SemaphoreHandle_t s_mutex = nullptr;
static ObdTelemetry s_data;
static ConnectionStatus s_status;
static uint32_t s_statusSequence = 0;

static bool newer(uint32_t incoming, uint32_t current)
{
    return current == 0 || static_cast<int32_t>(incoming - current) > 0;
}

void telemetryInit()
{
    if(s_mutex == nullptr) s_mutex = xSemaphoreCreateMutex();
}

bool acceptTelemetryFrame(const protocol::TelemetryFrame &f, uint32_t now)
{
    telemetryInit();
    if((f.validMask & ~kAllFields) != 0 || (f.supportedMask & ~kAllFields) != 0) return false;
    if(f.rpm > 20000 || f.speedKmh > 400 || f.coolantDeciC < -400 || f.coolantDeciC > 2500 ||
       f.throttleCentiPercent > 10000 || f.loadCentiPercent > 10000 ||
       f.voltageMilliVolt > 32000 || f.fuelCentiPercent > 10000 ||
       f.intakeDeciC < -400 || f.intakeDeciC > 2500 || f.mapDeciKpa > 30000) return false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if(!newer(f.sequence, s_data.sequence)) {
        xSemaphoreGive(s_mutex);
        return false;
    }
    s_data.sequence = f.sequence;
    s_data.receivedAtMs = now;
    s_data.validMask = f.validMask & f.supportedMask;
    s_data.supportedMask = f.supportedMask;
    s_data.rpm = f.rpm;
    s_data.speedKmh = f.speedKmh;
    s_data.coolantC = f.coolantDeciC / 10.0f;
    s_data.throttlePercent = f.throttleCentiPercent / 100.0f;
    s_data.engineLoadPercent = f.loadCentiPercent / 100.0f;
    s_data.controlModuleVoltage = f.voltageMilliVolt / 1000.0f;
    s_data.fuelLevelPercent = f.fuelCentiPercent / 100.0f;
    s_data.intakeAirC = f.intakeDeciC / 10.0f;
    s_data.mapKpa = f.mapDeciKpa / 10.0f;
    s_data.mafGps = f.mafCentiGps / 100.0f;
    s_data.engineRuntimeSeconds = f.runtimeSeconds;
    for(int i = 0; i < 11; ++i) if(s_data.validMask & (1u << i)) s_data.fieldReceivedAtMs[i] = now;
    xSemaphoreGive(s_mutex);
    return true;
}

bool acceptStatusFrame(const protocol::StatusFrame &f, uint32_t now)
{
    telemetryInit();
    if(f.state > static_cast<uint8_t>(ConnectionState::Error)) return false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if(!newer(f.sequence, s_statusSequence)) {
        xSemaphoreGive(s_mutex);
        return false;
    }
    s_statusSequence = f.sequence;
    s_status.state = static_cast<ConnectionState>(f.state);
    s_status.elmConnected = (f.flags & 1) != 0;
    s_status.ecuConnected = (f.flags & 2) != 0;
    s_status.esp32Connected = (f.flags & 4) != 0;
    memcpy(s_status.protocol, f.protocol, 8);
    s_status.protocol[8] = '\0';
    s_status.latencyMs = f.latencyMs;
    s_status.timeouts = f.timeouts;
    s_status.lastError = f.lastError;
    s_status.receivedAtMs = now;
    xSemaphoreGive(s_mutex);
    return true;
}

ObdTelemetry telemetrySnapshot(uint32_t now)
{
    telemetryInit();
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    ObdTelemetry copy = s_data;
    xSemaphoreGive(s_mutex);
    const uint32_t stale[11] = {kRpmStaleMs, kSpeedStaleMs, kCoolantStaleMs,
        kFastFieldStaleMs, kFastFieldStaleMs, kSlowFieldStaleMs, kSlowFieldStaleMs,
        kSlowFieldStaleMs, kFastFieldStaleMs, kFastFieldStaleMs, kSlowFieldStaleMs};
    for(int i = 0; i < 11; ++i) {
        if((copy.validMask & (1u << i)) && now - copy.fieldReceivedAtMs[i] > stale[i])
            copy.validMask &= static_cast<uint16_t>(~(1u << i));
    }
    return copy;
}

ConnectionStatus connectionSnapshot()
{
    telemetryInit();
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    ConnectionStatus copy = s_status;
    xSemaphoreGive(s_mutex);
    return copy;
}

void telemetrySetBleConnected(bool connected)
{
    telemetryInit();
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_status.esp32Connected = connected;
    if(connected) {
        // O contador do Android reinicia junto com o processo. Uma nova sessao BLE
        // precisa aceitar sua primeira sequencia mesmo que seja menor que a anterior.
        s_data.sequence = 0;
        s_statusSequence = 0;
    } else {
        s_status.state = ConnectionState::Disconnected;
    }
    xSemaphoreGive(s_mutex);
}

void telemetrySimulate(uint32_t now)
{
#if OBD_SIMULATION
    protocol::TelemetryFrame f{};
    f.magic = protocol::kMagic; f.version = 1; f.type = 1; f.size = sizeof(f);
    f.sequence = now / 50 + 1; f.sourceTimestampMs = now;
    f.supportedMask = kAllFields; f.validMask = kAllFields;
    const float phase = (now % 12000) / 12000.0f;
    const float ramp = phase < 0.55f ? phase / 0.55f : (1.0f - phase) / 0.45f;
    f.rpm = static_cast<uint16_t>(850 + 8000 * ramp);
    f.speedKmh = static_cast<uint16_t>(180 * ramp);
    f.coolantDeciC = static_cast<int16_t>((45 + 48 * fminf(now / 180000.0f, 1.0f)) * 10);
    f.throttleCentiPercent = static_cast<uint16_t>((8 + 82 * ramp) * 100);
    f.loadCentiPercent = static_cast<uint16_t>((12 + 75 * ramp) * 100);
    f.voltageMilliVolt = 13800; f.fuelCentiPercent = 5400; f.intakeDeciC = 310;
    f.mapDeciKpa = static_cast<uint16_t>((30 + 70 * ramp) * 10);
    f.mafCentiGps = static_cast<uint16_t>((2 + 80 * ramp) * 100);
    f.runtimeSeconds = now / 1000;
    if((now / 1000) % 17 == 14) f.validMask &= static_cast<uint16_t>(~Rpm);
    acceptTelemetryFrame(f, now);
#else
    (void) now;
#endif
}

const char *connectionStateName(ConnectionState state)
{
    static const char *names[] = {"DESCONECTADO", "CONECTANDO ELM", "INICIALIZANDO ELM",
        "DETECTANDO PROTOCOLO", "VERIFICANDO PIDS", "CONECTANDO ESP32", "PRONTO",
        "DEGRADADO", "RECONECTANDO", "ERRO"};
    const unsigned i = static_cast<unsigned>(state);
    return i < sizeof(names) / sizeof(names[0]) ? names[i] : "ERRO";
}

} // namespace obd

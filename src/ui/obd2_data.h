#pragma once

#include <stdint.h>

namespace ui {
namespace obd2 {

enum class ObdConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Error,
};

class ObdDataProvider {
public:
    virtual ~ObdDataProvider() = default;
    virtual void reset() = 0;
    virtual void tick(uint32_t elapsed_ms) = 0;
    virtual int get_rpm() const = 0;
    virtual int get_speed() const = 0;
    virtual int get_coolant_temperature() const = 0;
    virtual float get_battery_voltage() const = 0;
    virtual ObdConnectionState connection_state() const = 0;
};

class SimulatedObdDataProvider final : public ObdDataProvider {
public:
    void reset() override;
    void tick(uint32_t elapsed_ms) override;
    int get_rpm() const override;
    int get_speed() const override;
    int get_coolant_temperature() const override;
    float get_battery_voltage() const override;
    ObdConnectionState connection_state() const override;

private:
    int rpm_ = 0;
    int direction_ = 1;
    uint32_t remainder_ms_ = 0;
};

ObdDataProvider &data_provider();

} // namespace obd2
} // namespace ui

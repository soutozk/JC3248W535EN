#include "obd_settings.h"
#include "obd_constants.h"

#include "nvs.h"
#include "nvs_flash.h"

namespace obd {
namespace settings {

static ObdDisplaySettings s_value{kShiftRpmDefault};
static bool s_ready = false;

uint16_t clampShiftRpm(int value)
{
    if(value < kShiftRpmMin) value = kShiftRpmMin;
    if(value > kShiftRpmMax) value = kShiftRpmMax;
    return static_cast<uint16_t>(value - (value % kShiftRpmStep));
}

void init()
{
    if(s_ready) return;
    esp_err_t error = nvs_flash_init();
    if(error == ESP_ERR_NVS_NO_FREE_PAGES || error == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    nvs_handle_t handle;
    uint16_t persisted = kShiftRpmDefault;
    if(nvs_open("obd_display", NVS_READONLY, &handle) == ESP_OK) {
        if(nvs_get_u16(handle, "shift_rpm", &persisted) != ESP_OK) persisted = kShiftRpmDefault;
        nvs_close(handle);
    }
    s_value.shiftRpm = clampShiftRpm(persisted);
    s_ready = true;
}

ObdDisplaySettings get()
{
    init();
    return s_value;
}

bool save(const ObdDisplaySettings &value)
{
    init();
    nvs_handle_t handle;
    if(nvs_open("obd_display", NVS_READWRITE, &handle) != ESP_OK) return false;
    s_value.shiftRpm = clampShiftRpm(value.shiftRpm);
    esp_err_t error = nvs_set_u16(handle, "shift_rpm", s_value.shiftRpm);
    if(error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error == ESP_OK;
}

bool restoreDefaults()
{
    return save({kShiftRpmDefault});
}

} // namespace settings
} // namespace obd


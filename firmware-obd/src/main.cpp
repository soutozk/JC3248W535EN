#include "display.h"
#include "esp_bsp.h"
#include "lv_port.h"
#include "obd_ble.h"
#include "obd_screens.h"
#include "obd_settings.h"
#include "obd_telemetry.h"

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "lvgl.h"

namespace {
static const char *TAG = "FIRMWARE_OBD";
constexpr int kRotationDegrees = 90;

void presentationTimer(lv_timer_t *)
{
    obd::telemetrySimulate(static_cast<uint32_t>(esp_timer_get_time() / 1000));
}
}

extern "C" void app_main()
{
    esp_chip_info_t chip{};
    uint32_t flashSize = 0;
    esp_chip_info(&chip);
    esp_flash_get_size(nullptr, &flashSize);
    ESP_LOGI(TAG, "ESP32-S3 cores=%d flash=%luMB PSRAM livre=%lu",
             chip.cores, static_cast<unsigned long>(flashSize / (1024 * 1024)),
             static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));

    obd::settings::init();
    obd::telemetryInit();

    lvgl_port_cfg_t port = ESP_LVGL_PORT_INIT_CONFIG();
    port.task_max_sleep_ms = 16;
    port.task_stack = 6144;

    bsp_display_cfg_t display = {
        .lvgl_port_cfg = port,
        .buffer_size = EXAMPLE_LCD_QSPI_H_RES * EXAMPLE_LCD_QSPI_V_RES,
        .rotate = kRotationDegrees == 90 ? LV_DISP_ROT_90 : LV_DISP_ROT_NONE,
    };
    bsp_display_start_with_config(&display);
    bsp_display_backlight_on();

    obd_ble_init();

    bsp_display_lock(0);
    obd::screens::showMenu();
    lv_timer_create(presentationTimer, 50, nullptr);
    bsp_display_unlock();
    ESP_LOGI(TAG, "Firmware OBD pronto; apresentacao automatica ativa; simulacao forcada=%d",
             OBD_SIMULATION);
}

#include "preferences.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include <stdint.h>
#include <string.h>

namespace ui {
namespace preferences {

static const char *TAG = "ui_preferences";

static constexpr const char *kNamespace = "ui_config";
static constexpr int kDefaultPalette = 0;
static constexpr int kDefaultBrightness = 100;
static constexpr size_t kPathSize = 320;

static bool s_ready = false;
static bool s_loaded = false;
static char s_boot_path[kPathSize] = {};
static char s_logo_path[kPathSize] = {};

static void ensure_nvs()
{
    if(s_ready) {
        return;
    }

    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    s_ready = true;
}

static nvs_handle_t open_read_write()
{
    ensure_nvs();
    nvs_handle_t handle = 0;
    return nvs_open(kNamespace, NVS_READWRITE, &handle) == ESP_OK ? handle : 0;
}

static int get_u8(const char *key, int fallback)
{
    nvs_handle_t handle = open_read_write();
    if(handle == 0) {
        return fallback;
    }

    uint8_t value = 0;
    esp_err_t err = nvs_get_u8(handle, key, &value);
    nvs_close(handle);
    return err == ESP_OK ? static_cast<int>(value) : fallback;
}

static void set_u8(const char *key, int value)
{
    nvs_handle_t handle = open_read_write();
    if(handle == 0) {
        return;
    }

    if(value < 0) {
        value = 0;
    }
    if(value > 255) {
        value = 255;
    }
    nvs_set_u8(handle, key, static_cast<uint8_t>(value));
    nvs_commit(handle);
    nvs_close(handle);
}

static const char *get_path(const char *key, char *buffer)
{
    ensure_nvs();
    nvs_handle_t handle = 0;
    if(nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) {
        buffer[0] = '\0';
        return buffer;
    }

    size_t size = kPathSize;
    esp_err_t err = nvs_get_str(handle, key, buffer, &size);
    nvs_close(handle);
    if(err != ESP_OK) {
        buffer[0] = '\0';
    }
    return buffer;
}

static void set_path(const char *key, const char *path)
{
    nvs_handle_t handle = open_read_write();
    if(handle == 0) {
        ESP_LOGE(TAG, "NVS open failed for key %s", key);
        return;
    }

    esp_err_t err = ESP_OK;
    if(path == nullptr || path[0] == '\0') {
        err = nvs_erase_key(handle, key);
    } else {
        err = nvs_set_str(handle, key, path);
    }
    if(err == ESP_OK) {
        err = nvs_commit(handle);
    }
    ESP_LOGI(TAG, "save %s='%s' result=%s", key, path == nullptr ? "" : path, esp_err_to_name(err));
    nvs_close(handle);
}

void init()
{
    if(s_loaded) {
        return;
    }
    ensure_nvs();
    get_path("boot_path", s_boot_path);
    get_path("logo_path", s_logo_path);
    s_loaded = true;
}

int palette_index()
{
    return get_u8("palette", kDefaultPalette);
}

void set_palette_index(int index)
{
    set_u8("palette", index);
}

int brightness()
{
    int value = get_u8("brightness", kDefaultBrightness);
    return value < 10 ? 10 : (value > 100 ? 100 : value);
}

void set_brightness(int value)
{
    set_u8("brightness", value);
}

const char *boot_image_path()
{
    init();
    return s_boot_path;
}

const char *logo_image_path()
{
    init();
    return s_logo_path;
}

void set_boot_image_path(const char *path)
{
    init();
    set_path("boot_path", path);
    strncpy(s_boot_path, path == nullptr ? "" : path, kPathSize - 1);
    s_boot_path[kPathSize - 1] = '\0';
}

void set_logo_image_path(const char *path)
{
    init();
    set_path("logo_path", path);
    strncpy(s_logo_path, path == nullptr ? "" : path, kPathSize - 1);
    s_logo_path[kPathSize - 1] = '\0';
}

} // namespace preferences
} // namespace ui

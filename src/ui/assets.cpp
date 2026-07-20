#include "assets.h"

#include "preferences.h"
#include "sd_browser.h"

#include "esp_log.h"
#include <stdio.h>

namespace ui {
namespace assets {

static const char *TAG = "ui_assets";

static char s_lvgl_path[320] = {};

static const char *selected_path(const char *candidate, const char *fallback)
{
    if(sd_browser::is_mounted() && candidate != nullptr && candidate[0] != '\0' &&
       sd_browser::file_exists(candidate)) {
        return candidate;
    }
    return fallback;
}

static const char *to_lvgl_path(const char *posix_path)
{
    if(posix_path != nullptr && posix_path[0] != '\0' && posix_path[0] == '/' &&
       posix_path[1] == 's' && posix_path[2] == 'd' && posix_path[3] == '/') {
        snprintf(s_lvgl_path, sizeof(s_lvgl_path), "S:%s", posix_path + 3);
    } else {
        snprintf(s_lvgl_path, sizeof(s_lvgl_path), "S:%s", posix_path == nullptr ? "" : posix_path);
    }
    return s_lvgl_path;
}

const char *project_name()
{
    return "JC3248W535EN";
}

const char *logo_posix_path()
{
    return selected_path(preferences::logo_image_path(), "/sd/logo.png");
}

const char *logo_lvgl_path()
{
    return to_lvgl_path(logo_posix_path());
}

bool logo_available()
{
    return sd_browser::is_mounted() && sd_browser::file_exists(logo_posix_path());
}

const char *intro_gif_posix_path()
{
    return "/sd/intro/boot.gif";
}

const char *intro_gif_lvgl_path()
{
    return "S:/intro/boot.gif";
}

bool intro_gif_available()
{
    // A boot image selected in CONFIGURACOES has priority over the default GIF.
    if(sd_browser::is_mounted() && preferences::boot_image_path()[0] != '\0') {
        return false;
    }
    return sd_browser::is_mounted() && sd_browser::file_exists(intro_gif_posix_path());
}

const char *intro_image_posix_path()
{
    const char *selected = selected_path(preferences::boot_image_path(), nullptr);
    if(selected != nullptr) {
        return selected;
    }
    return selected_path("/sd/intro/boot.png", "/sd/intro/logo.png");
}

const char *intro_image_lvgl_path()
{
    return to_lvgl_path(intro_image_posix_path());
}

bool intro_image_available()
{
    return sd_browser::is_mounted() && sd_browser::file_exists(intro_image_posix_path());
}

const char *home_logo_posix_path()
{
    const char *selected = selected_path(preferences::logo_image_path(), nullptr);
    if(selected != nullptr) {
        return selected;
    }
    ESP_LOGW(TAG, "selected logo is unavailable: %s", preferences::logo_image_path());
    return selected_path("/sd/intro/logo.png", "/sd/logo.png");
}

const char *home_logo_lvgl_path()
{
    return to_lvgl_path(home_logo_posix_path());
}

bool home_logo_available()
{
    return sd_browser::is_mounted() && sd_browser::file_exists(home_logo_posix_path());
}

const char *home_background_posix_path()
{
    if(sd_browser::is_mounted() && sd_browser::file_exists("/sd/backgrounds/home.png")) {
        return "/sd/backgrounds/home.png";
    }
    return "/sd/home_bg.png";
}

const char *home_background_lvgl_path()
{
    if(sd_browser::is_mounted() && sd_browser::file_exists("/sd/backgrounds/home.png")) {
        return "S:/backgrounds/home.png";
    }
    return "S:/home_bg.png";
}

bool home_background_available()
{
    return sd_browser::is_mounted() && sd_browser::file_exists(home_background_posix_path());
}

} // namespace assets
} // namespace ui

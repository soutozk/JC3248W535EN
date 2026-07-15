#include "assets.h"

#include "sd_browser.h"

namespace ui {
namespace assets {

const char *project_name()
{
    return "JC3248W535EN";
}

const char *logo_posix_path()
{
    return "/sd/logo.png";
}

const char *logo_lvgl_path()
{
    return "S:/logo.png";
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
    return sd_browser::is_mounted() && sd_browser::file_exists(intro_gif_posix_path());
}

const char *intro_image_posix_path()
{
    if(sd_browser::is_mounted() && sd_browser::file_exists("/sd/intro/boot.png")) {
        return "/sd/intro/boot.png";
    }
    return "/sd/intro/logo.png";
}

const char *intro_image_lvgl_path()
{
    if(sd_browser::is_mounted() && sd_browser::file_exists("/sd/intro/boot.png")) {
        return "S:/intro/boot.png";
    }
    return "S:/intro/logo.png";
}

bool intro_image_available()
{
    return sd_browser::is_mounted() && sd_browser::file_exists(intro_image_posix_path());
}

const char *home_logo_posix_path()
{
    if(sd_browser::is_mounted() && sd_browser::file_exists("/sd/intro/logo.png")) {
        return "/sd/intro/logo.png";
    }
    return "/sd/logo.png";
}

const char *home_logo_lvgl_path()
{
    if(sd_browser::is_mounted() && sd_browser::file_exists("/sd/intro/logo.png")) {
        return "S:/intro/logo.png";
    }
    return "S:/logo.png";
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

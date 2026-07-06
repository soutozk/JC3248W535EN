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

} // namespace assets
} // namespace ui

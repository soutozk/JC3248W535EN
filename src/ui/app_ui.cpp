#include "app_ui.h"

#include "boot.h"
#include "sd_browser.h"
#include "settings.h"
#include "theme.h"

bool ui_app_mount_storage(void)
{
    return ui::sd_browser::mount();
}

void ui_app_start(void)
{
    ui::settings::init();
    ui::theme::init();
    ui::boot::show();
}

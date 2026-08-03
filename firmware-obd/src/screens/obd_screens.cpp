#include "obd_screens.h"
#include "obd_constants.h"
#include "obd_settings.h"
#include "obd_telemetry.h"
#include "theme.h"

#include "esp_timer.h"
#include "lvgl.h"
#include <math.h>
#include <stdio.h>

namespace obd {
namespace screens {
namespace {

lv_obj_t *label(lv_obj_t *parent, const char *value, const lv_font_t *font,
                lv_color_t color, lv_align_t align, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, value);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, color, 0);
    lv_obj_align(obj, align, x, y);
    return obj;
}

lv_obj_t *newScreen(const char *title)
{
    ui::theme::init();
    lv_obj_t *screen = lv_obj_create(nullptr);
    ui::theme::apply_screen(screen);
    ui::theme::add_frame_ticks(screen);
    ui::theme::add_scanlines(screen, LV_OPA_10);
    label(screen, title, ui::theme::font_title(), ui::theme::colors().text,
          LV_ALIGN_TOP_LEFT, 16, 14);
    return screen;
}

void load(lv_obj_t *screen) { lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 180, 0, true); }

void routeCb(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    const uintptr_t route = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
    switch(route) {
        case 0: showMenu(); break; case 1: showRpm(); break; case 2: showSpeed(); break;
        case 3: showCoolant(); break; case 4: showDashboard(); break;
        case 5: showSettings(); break; case 6: showDiagnostics(); break;
    }
}

lv_obj_t *button(lv_obj_t *parent, const char *text, uintptr_t route,
                 lv_coord_t w, lv_coord_t h, bool enabled = true)
{
    lv_obj_t *obj = lv_btn_create(parent);
    ui::theme::apply_button(obj, enabled);
    lv_obj_set_size(obj, w, h);
    if(route <= 6) {
        lv_obj_add_event_cb(obj, routeCb, LV_EVENT_CLICKED, reinterpret_cast<void *>(route));
    }
    lv_obj_t *caption = label(obj, text, ui::theme::font_body(),
                              enabled ? ui::theme::colors().text : ui::theme::colors().muted,
                              LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(caption, LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

void addBack(lv_obj_t *screen)
{
    lv_obj_t *back = button(screen, "< MENU", 0, 76, 30);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -14, 10);
}

uint32_t nowMs() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

struct GaugeContext { lv_obj_t *value; lv_obj_t *unit; lv_obj_t *status; float shown; uint16_t field; };
static GaugeContext s_gauge{};
static lv_timer_t *s_gaugeTimer = nullptr;

void stopGauge(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_DELETE && s_gaugeTimer != nullptr) {
        lv_timer_del(s_gaugeTimer); s_gaugeTimer = nullptr; s_gauge = {};
    }
}

void gaugeTick(lv_timer_t *)
{
    const ObdTelemetry t = telemetrySnapshot(nowMs());
    bool valid = (t.validMask & s_gauge.field) != 0;
    float target = s_gauge.field == Speed ? t.speedKmh : t.coolantC;
    if(!valid) {
        lv_label_set_text(s_gauge.value, "--");
        lv_label_set_text(s_gauge.status, (t.supportedMask & s_gauge.field) ? "DADO OBSOLETO" : "PID INDISPONIVEL");
        lv_obj_set_style_text_color(s_gauge.status, ui::theme::colors().muted, 0);
        return;
    }
    if(s_gauge.field == Speed) {
        if(target < 0 || target > 400) return;
        s_gauge.shown += (target - s_gauge.shown) * 0.28f;
        char value[12]; snprintf(value, sizeof(value), "%d", static_cast<int>(lroundf(s_gauge.shown)));
        lv_label_set_text(s_gauge.value, value);
        lv_label_set_text(s_gauge.status, "DADO VALIDO / CONECTADO");
        lv_obj_set_style_text_color(s_gauge.status, ui::theme::colors().cyan, 0);
    } else {
        s_gauge.shown += (target - s_gauge.shown) * 0.22f;
        char value[12]; snprintf(value, sizeof(value), "%d", static_cast<int>(lroundf(s_gauge.shown)));
        lv_label_set_text(s_gauge.value, value);
        if(target <= kCoolantColdMaxC) {
            lv_label_set_text(s_gauge.status, "MOTOR FRIO");
            lv_obj_set_style_text_color(s_gauge.status, lv_color_hex(0x24A8FF), 0);
        } else if(target <= kCoolantNormalMaxC) {
            lv_label_set_text(s_gauge.status, "TEMPERATURA NORMAL");
            lv_obj_set_style_text_color(s_gauge.status, lv_color_hex(0x79D616), 0);
        } else {
            lv_label_set_text(s_gauge.status, "TEMPERATURA ELEVADA");
            lv_obj_set_style_text_color(s_gauge.status, lv_color_hex(0xFF3218), 0);
        }
    }
}

struct DashboardContext { lv_obj_t *values[6]; lv_obj_t *footer; };
static DashboardContext s_dash{};
static lv_timer_t *s_dashTimer = nullptr;
void stopDash(lv_event_t *e) { if(lv_event_get_code(e)==LV_EVENT_DELETE && s_dashTimer){lv_timer_del(s_dashTimer);s_dashTimer=nullptr;s_dash={};} }

void dashTick(lv_timer_t *)
{
    const ObdTelemetry t = telemetrySnapshot(nowMs());
    const float values[] = {t.rpm,t.speedKmh,t.coolantC,t.engineLoadPercent,t.throttlePercent,t.controlModuleVoltage};
    const uint16_t masks[] = {Rpm,Speed,Coolant,EngineLoad,Throttle,Voltage};
    const char *formats[] = {"%.0f RPM","%.0f km/h","%.0f C","%.1f %%","%.1f %%","%.2f V"};
    char text[32];
    for(int i=0;i<6;++i){ if(t.validMask&masks[i]) snprintf(text,sizeof(text),formats[i],values[i]); else snprintf(text,sizeof(text),"--"); lv_label_set_text(s_dash.values[i],text); }
    const ConnectionStatus st=connectionSnapshot();
    snprintf(text,sizeof(text),"%s | %u ms",connectionStateName(st.state),st.latencyMs);
    lv_label_set_text(s_dash.footer,text);
}

static uint16_t s_editShift = kShiftRpmDefault;
static lv_obj_t *s_shiftValue = nullptr;
static lv_obj_t *s_saveFeedback = nullptr;
void refreshShift(){char b[24];snprintf(b,sizeof(b),"%u RPM",s_editShift);lv_label_set_text(s_shiftValue,b);}
void shiftCb(lv_event_t *e){if(lv_event_get_code(e)!=LV_EVENT_CLICKED)return;intptr_t op=reinterpret_cast<intptr_t>(lv_event_get_user_data(e));if(op==-1)s_editShift=settings::clampShiftRpm(s_editShift-kShiftRpmStep);else if(op==1)s_editShift=settings::clampShiftRpm(s_editShift+kShiftRpmStep);else if(op==2){bool ok=settings::save({s_editShift});lv_label_set_text(s_saveFeedback,ok?"SALVO":"ERRO AO SALVAR");}else if(op==3){settings::restoreDefaults();s_editShift=kShiftRpmDefault;lv_label_set_text(s_saveFeedback,"PADRAO RESTAURADO");}refreshShift();}

} // namespace

void showMenu()
{
    lv_obj_t *screen = newScreen("OBD-II / MENU");
    const ObdTelemetry t = telemetrySnapshot(nowMs());
    const bool unknown = t.supportedMask == 0;
    struct Tile {const char *name; uintptr_t route; uint16_t mask;};
    const Tile tiles[]={{LV_SYMBOL_REFRESH "  RPM",1,Rpm},{LV_SYMBOL_CHARGE "  VELOCIDADE",2,Speed},{LV_SYMBOL_WARNING "  TEMPERATURA",3,Coolant},{LV_SYMBOL_LIST "  PAINEL GERAL",4,0},{LV_SYMBOL_SETTINGS "  CONFIGURACOES",5,0},{LV_SYMBOL_WIFI "  DIAGNOSTICO",6,0}};
    for(int i=0;i<6;++i){bool enabled=tiles[i].mask==0||unknown||(t.supportedMask&tiles[i].mask);lv_obj_t*b=button(screen,tiles[i].name,tiles[i].route,210,66,enabled);lv_obj_set_pos(b,18+(i%2)*234,58+(i/2)*78);}
    const ConnectionStatus st=connectionSnapshot();
    label(screen,connectionStateName(st.state),ui::theme::font_small(),ui::theme::colors().muted,LV_ALIGN_BOTTOM_MID,0,-10);
    load(screen);
}

void showSpeed()
{
    lv_obj_t *screen=newScreen("VELOCIDADE"); addBack(screen);
    lv_obj_t *panel=ui::theme::create_panel(screen);lv_obj_set_size(panel,430,220);lv_obj_align(panel,LV_ALIGN_CENTER,0,16);
    s_gauge.value=label(panel,"--",&lv_font_montserrat_48,ui::theme::colors().cyan,LV_ALIGN_CENTER,-28,-20);
    s_gauge.unit=label(panel,"km/h",ui::theme::font_body(),ui::theme::colors().text,LV_ALIGN_CENTER,92,2);
    s_gauge.status=label(panel,"AGUARDANDO DADOS",ui::theme::font_small(),ui::theme::colors().muted,LV_ALIGN_BOTTOM_MID,0,-18);
    s_gauge.field=Speed;s_gauge.shown=0;lv_obj_add_event_cb(screen,stopGauge,LV_EVENT_DELETE,nullptr);s_gaugeTimer=lv_timer_create(gaugeTick,40,nullptr);load(screen);
}

void showCoolant()
{
    lv_obj_t *screen=newScreen("LIQUIDO DE ARREFECIMENTO"); addBack(screen);
    lv_obj_t *panel=ui::theme::create_panel(screen);lv_obj_set_size(panel,430,220);lv_obj_align(panel,LV_ALIGN_CENTER,0,16);
    s_gauge.value=label(panel,"--",&lv_font_montserrat_48,ui::theme::colors().cyan,LV_ALIGN_CENTER,-20,-20);
    s_gauge.unit=label(panel,"C",ui::theme::font_body(),ui::theme::colors().text,LV_ALIGN_CENTER,72,2);
    s_gauge.status=label(panel,"AGUARDANDO DADOS",ui::theme::font_small(),ui::theme::colors().muted,LV_ALIGN_BOTTOM_MID,0,-18);
    label(panel,"Faixas visuais de referencia; nao constituem diagnostico.",ui::theme::font_small(),ui::theme::colors().muted,LV_ALIGN_BOTTOM_MID,0,-2);
    s_gauge.field=Coolant;s_gauge.shown=20;lv_obj_add_event_cb(screen,stopGauge,LV_EVENT_DELETE,nullptr);s_gaugeTimer=lv_timer_create(gaugeTick,100,nullptr);load(screen);
}

void showDashboard()
{
    lv_obj_t *screen=newScreen("PAINEL GERAL");addBack(screen);
    const char *names[]={"RPM","VELOCIDADE","TEMP.","CARGA","ACELERADOR","TENSAO"};
    const uintptr_t routes[]={1,2,3,0,0,0};
    for(int i=0;i<6;++i){lv_obj_t*c=ui::theme::create_panel(screen);lv_obj_set_size(c,140,92);lv_obj_set_pos(c,18+(i%3)*151,52+(i/3)*102);label(c,names[i],ui::theme::font_small(),ui::theme::colors().muted,LV_ALIGN_TOP_LEFT,3,1);s_dash.values[i]=label(c,"--",ui::theme::font_body(),ui::theme::colors().cyan,LV_ALIGN_CENTER,0,10);if(routes[i]){lv_obj_add_flag(c,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(c,routeCb,LV_EVENT_CLICKED,reinterpret_cast<void*>(routes[i]));}}
    s_dash.footer=label(screen,"DESCONECTADO",ui::theme::font_small(),ui::theme::colors().muted,LV_ALIGN_BOTTOM_MID,0,-8);lv_obj_add_event_cb(screen,stopDash,LV_EVENT_DELETE,nullptr);s_dashTimer=lv_timer_create(dashTick,100,nullptr);dashTick(nullptr);load(screen);
}

void showSettings()
{
    lv_obj_t *screen=newScreen("CONFIGURACOES OBD");addBack(screen);s_editShift=settings::get().shiftRpm;
    lv_obj_t *panel=ui::theme::create_panel(screen);lv_obj_set_size(panel,430,230);lv_obj_align(panel,LV_ALIGN_CENTER,0,18);
    label(panel,"SHIFT LIGHT",ui::theme::font_title(),ui::theme::colors().text,LV_ALIGN_TOP_MID,0,8);
    s_shiftValue=label(panel,"",&lv_font_montserrat_28,ui::theme::colors().cyan,LV_ALIGN_TOP_MID,0,46);refreshShift();
    lv_obj_t*minus=button(panel,"- 100",99,100,42);lv_obj_align(minus,LV_ALIGN_LEFT_MID,18,5);lv_obj_add_event_cb(minus,shiftCb,LV_EVENT_CLICKED,reinterpret_cast<void*>(-1));
    lv_obj_t*plus=button(panel,"+ 100",99,100,42);lv_obj_align(plus,LV_ALIGN_RIGHT_MID,-18,5);lv_obj_add_event_cb(plus,shiftCb,LV_EVENT_CLICKED,reinterpret_cast<void*>(1));
    lv_obj_t*save=button(panel,"SALVAR",99,120,38);lv_obj_align(save,LV_ALIGN_BOTTOM_LEFT,28,-28);lv_obj_add_event_cb(save,shiftCb,LV_EVENT_CLICKED,reinterpret_cast<void*>(2));
    lv_obj_t*reset=button(panel,"RESTAURAR",99,120,38);lv_obj_align(reset,LV_ALIGN_BOTTOM_RIGHT,-28,-28);lv_obj_add_event_cb(reset,shiftCb,LV_EVENT_CLICKED,reinterpret_cast<void*>(3));
    s_saveFeedback=label(panel,"2000 - 9000 RPM",ui::theme::font_small(),ui::theme::colors().muted,LV_ALIGN_BOTTOM_MID,0,-6);load(screen);
}

void showDiagnostics()
{
    lv_obj_t *screen=newScreen("DIAGNOSTICO");addBack(screen);const ConnectionStatus s=connectionSnapshot();const ObdTelemetry t=telemetrySnapshot(nowMs());
    lv_obj_t*p=ui::theme::create_panel(screen);lv_obj_set_size(p,430,240);lv_obj_align(p,LV_ALIGN_CENTER,0,18);char b[384];
    snprintf(b,sizeof(b),"Estado: %s\nELM conectado: %s\nECU respondendo: %s\nESP32 BLE: %s\nProtocolo: %s\nLatencia: %u ms\nTimeouts: %u\nUltimo erro: %u\nSequencia: %lu\nPIDs validos: 0x%03X\nPIDs suportados: 0x%03X",connectionStateName(s.state),s.elmConnected?"sim":"nao",s.ecuConnected?"sim":"nao",s.esp32Connected?"sim":"nao",s.protocol[0]?s.protocol:"N/D",s.latencyMs,s.timeouts,s.lastError,static_cast<unsigned long>(t.sequence),t.validMask,t.supportedMask);
    lv_obj_t*l=label(p,b,ui::theme::font_small(),ui::theme::colors().text,LV_ALIGN_TOP_LEFT,6,6);lv_obj_set_style_text_line_space(l,5,0);load(screen);
}

} // namespace screens
} // namespace obd

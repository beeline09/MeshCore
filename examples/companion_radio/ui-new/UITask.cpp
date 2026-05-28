#include "UITask.h"
#include <helpers/TxtDataHelpers.h>
#include "../MyMesh.h"
#include "target.h"
#include <time.h>
#if defined(WIFI_SSID) || defined(WITH_WIFI_SWITCHING)
  #include <WiFi.h>
#endif

#ifndef AUTO_OFF_MILLIS
  #define AUTO_OFF_MILLIS     15000   // 15 seconds
#endif
#define BOOT_SCREEN_MILLIS   3000   // 3 seconds

#ifdef PIN_STATUS_LED
#define LED_ON_MILLIS     20
#define LED_ON_MSG_MILLIS 200
#define LED_CYCLE_MILLIS  4000
#endif

#define LONG_PRESS_MILLIS   1200

#ifndef UI_RECENT_LIST_SIZE
  #define UI_RECENT_LIST_SIZE 4
#endif

#if UI_HAS_JOYSTICK
  #define PRESS_LABEL "press Enter"
#else
  #define PRESS_LABEL "long press"
#endif

#include "icons.h"

// nRF52 system-metric helpers — declared at file scope (extern "C" is not allowed
// inside a member function body in C++).
#if defined(ARDUINO_ARCH_NRF52) || defined(NRF52)
extern "C" uint32_t analogReadVDD(void);

// SoftDevice (BLE stack) owns the TEMP peripheral; direct register writes crash.
// BLE builds define BLE_PIN_CODE; USB builds access TEMP registers directly.
#if defined(BLE_PIN_CODE)
extern "C" uint32_t sd_temp_get(int32_t *p_temp);
#endif
#endif

class SplashScreen : public UIScreen {
  UITask* _task;
  unsigned long dismiss_after;
  char _version_info[12];   // e.g. "v1.14.1"
  char _commit_info[12];    // e.g. "#abc1234" or "" if no GIT_COMMIT

public:
  SplashScreen(UITask* task) : _task(task) {
    const char *ver = FIRMWARE_VERSION;
    const char *dash = strchr(ver, '-');

    // version part: "v1.14.1"
    int len = dash ? dash - ver : strlen(ver);
    if (len >= (int)sizeof(_version_info)) len = sizeof(_version_info) - 1;
    memcpy(_version_info, ver, len);
    _version_info[len] = 0;

    // commit part: "#abc1234" (from the part after '-')
    if (dash) {
      _commit_info[0] = '#';
      strncpy(_commit_info + 1, dash + 1, sizeof(_commit_info) - 2);
      _commit_info[sizeof(_commit_info) - 1] = 0;
    } else {
      _commit_info[0] = 0;
    }

    dismiss_after = millis() + BOOT_SCREEN_MILLIS;
  }

  int render(DisplayDriver& display) override {
    // meshcore logo
    display.setColor(DisplayDriver::BLUE);
    int logoWidth = 128;
    display.drawXbm((display.width() - logoWidth) / 2, 3, meshcore_logo, logoWidth, 13);

    // version info
    display.setColor(DisplayDriver::LIGHT);
    display.setTextSize(2);
    display.drawTextCentered(display.width()/2, 22, _version_info);

    // commit hash (small, between version and date)
    display.setTextSize(1);
    if (_commit_info[0]) {
      display.drawTextCentered(display.width()/2, 41, _commit_info);
    }

    display.drawTextCentered(display.width()/2, 51, FIRMWARE_BUILD_DATE);

    return 1000;
  }

  void poll() override {
    if (millis() >= dismiss_after) {
      _task->gotoHomeScreen();
    }
  }
};

class HomeScreen : public UIScreen {
  enum HomePage {
    FIRST,
    CLOCK,     // eInk only: large clock + PM preview inline
    RECENT,
    RADIO,
#ifndef WITH_WIFI_SWITCHING
    BLUETOOTH,
#endif
    ADVERT,
#if ENV_INCLUDE_GPS == 1
    GPS,
#endif
#if UI_SENSORS_PAGE == 1
    SENSORS,
#endif
    SETTINGS,
    SHUTDOWN,
    Count    // keep as last
  };

  UITask* _task;
  mesh::RTCClock* _rtc;
  SensorManager* _sensors;
  NodePrefs* _node_prefs;
  uint8_t _page;
  bool _shutdown_init;
  int _clock_pm_pending;   // PMs queued for inline display on CLOCK page
  AdvertPath recent[UI_RECENT_LIST_SIZE];

  bool          _in_settings    = false;
  int           _settings_sel   = 0;
  int           _settings_scroll = 0;   // first visible item index
  int           _settings_visible = 4;  // updated in render(), used in handleInput()
  unsigned long _pin_show_until = 0;
  int           _pm_clock_mode  = 1;   // 0=all msgs switch screen, 1=PM inline only

#if defined(WITH_COMPANION_CLI) && defined(WITH_WIFI_SWITCHING)
  static const int SETTINGS_N          = 12;
  static const int SETTINGS_COMMS_IDX  = 3;
  static const int SETTINGS_PM_IDX     = 4;
  static const int SETTINGS_DIM_IDX    = 5;
  static const int SETTINGS_C2L_CH_IDX = 6;
  static const int SETTINGS_C2L_DM_IDX = 7;
  static const int SETTINGS_ROT_IDX    = 8;
  static const int SETTINGS_UNREAD_IDX = 9;
  static const int SETTINGS_LOG_IDX    = 10;
  static const int SETTINGS_BACK_IDX   = 11;
#elif defined(WITH_COMPANION_CLI)
  static const int SETTINGS_N          = 11;
  static const int SETTINGS_PM_IDX     = 3;
  static const int SETTINGS_DIM_IDX    = 4;
  static const int SETTINGS_C2L_CH_IDX = 5;
  static const int SETTINGS_C2L_DM_IDX = 6;
  static const int SETTINGS_ROT_IDX    = 7;
  static const int SETTINGS_UNREAD_IDX = 8;
  static const int SETTINGS_LOG_IDX    = 9;
  static const int SETTINGS_BACK_IDX   = 10;
#else
  static const int SETTINGS_N          = 6;
  static const int SETTINGS_PM_IDX     = 0;
  static const int SETTINGS_DIM_IDX    = 1;
  static const int SETTINGS_ROT_IDX    = 2;
  static const int SETTINGS_UNREAD_IDX = 3;
  static const int SETTINGS_LOG_IDX    = 4;
  static const int SETTINGS_BACK_IDX   = 5;
#endif

#ifdef WITH_WIFI_SWITCHING
  bool _in_wifi_select  = false;
  int  _wifi_sel        = 0;
  bool _wifi_wait       = false;
  bool _wifi_scanning   = false;
  int  _wifi_scan_n     = -1;   // number of scan results, -1 = not done
  int  _wifi_scan_scroll = 0;
  bool _wifi_cmd_shown  = false;
  char _wifi_cmd[72];           // full-screen command overlay text
#endif


  void renderBatteryIndicator(DisplayDriver& display, uint16_t batteryMilliVolts) {
    // Convert millivolts to percentage
#ifndef BATT_MIN_MILLIVOLTS
  #define BATT_MIN_MILLIVOLTS 3000
#endif
#ifndef BATT_MAX_MILLIVOLTS
  #define BATT_MAX_MILLIVOLTS 4200
#endif
    const int minMilliVolts = BATT_MIN_MILLIVOLTS;
    const int maxMilliVolts = BATT_MAX_MILLIVOLTS;
    int batteryPercentage = ((batteryMilliVolts - minMilliVolts) * 100) / (maxMilliVolts - minMilliVolts);
    if (batteryPercentage < 0) batteryPercentage = 0; // Clamp to 0%
    if (batteryPercentage > 100) batteryPercentage = 100; // Clamp to 100%

    // battery icon
    int iconWidth = 24;
    int iconHeight = 10;
    int iconX = display.width() - iconWidth - 5; // Position the icon near the top-right corner
    int iconY = 0;
    display.setColor(DisplayDriver::GREEN);

    // battery outline
    display.drawRect(iconX, iconY, iconWidth, iconHeight);

    // battery "cap"
    display.fillRect(iconX + iconWidth, iconY + (iconHeight / 4), 3, iconHeight / 2);

    // fill the battery based on the percentage
    int fillWidth = (batteryPercentage * (iconWidth - 4)) / 100;
    display.fillRect(iconX + 2, iconY + 2, fillWidth, iconHeight - 4);

    // show muted icon if buzzer is muted
#ifdef PIN_BUZZER
    if (_task->isBuzzerQuiet()) {
      display.setColor(DisplayDriver::RED);
      display.drawXbm(iconX - 9, iconY + 1, muted_icon, 8, 8);
    }
#endif
  }

  CayenneLPP sensors_lpp;
  int sensors_nb = 0;
  bool sensors_scroll = false;
  int sensors_scroll_offset = 0;
  int next_sensors_refresh = 0;
  
  // Special CayenneLPP channels for synthetic system metrics (rendered with custom labels)
  static const uint8_t SYS_CH_VDD           = 254;  // VDD voltage (LPP_VOLTAGE)
  static const uint8_t SYS_CH_CPU_TEMP      = 253;  // CPU temperature (LPP_TEMPERATURE)
  static const uint8_t SYS_CH_UPTIME_REBOOT = 252;  // uptime since reboot, encoded as hours (LPP_TEMPERATURE)
  static const uint8_t SYS_CH_UPTIME_CHARGE = 251;  // uptime since charge, encoded as hours (LPP_TEMPERATURE)

  void refresh_sensors() {
    if (millis() > next_sensors_refresh) {
      sensors_lpp.reset();
      sensors_nb = 0;
      sensors_lpp.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);
      sensors.querySensors(0xFF, sensors_lpp);

      // nRF52 system metrics --------------------------------------------------
#if defined(ARDUINO_ARCH_NRF52) || defined(NRF52)
      // VDD: internal 3.6V reference, 12-bit, no external divider needed
      float vdd = analogReadVDD() * 3.6f / 4096.0f;
      sensors_lpp.addVoltage(SYS_CH_VDD, vdd);
#ifdef NRF_TEMP
      // CPU die temperature (0.25°C per unit).
      // SoftDevice (BLE) owns the TEMP peripheral — use sd_temp_get() SVC.
      // USB builds without SoftDevice access the register directly.
      {
        float cpu_t = 0.0f;
#if defined(BLE_PIN_CODE)
        int32_t raw = 0;
        sd_temp_get(&raw);
        cpu_t = raw / 4.0f;
#else
        NRF_TEMP->TASKS_START = 1;
        while (!NRF_TEMP->EVENTS_DATARDY) {}
        NRF_TEMP->EVENTS_DATARDY = 0;
        cpu_t = NRF_TEMP->TEMP / 4.0f;
        NRF_TEMP->TASKS_STOP = 1;
#endif
        sensors_lpp.addTemperature(SYS_CH_CPU_TEMP, cpu_t);
      }
#endif
#endif
      // Uptime: encoded as fractional hours (range ±3276.7 h = ~136 days, sufficient)
      float up_reboot_h = millis() / 3600000.0f;
      float up_charge_h = up_reboot_h;
      if (_node_prefs) up_charge_h += _node_prefs->ui_charge_uptime_base / 3600.0f;
      sensors_lpp.addTemperature(SYS_CH_UPTIME_REBOOT, up_reboot_h);
      sensors_lpp.addTemperature(SYS_CH_UPTIME_CHARGE, up_charge_h);
      // -----------------------------------------------------------------------

      LPPReader reader (sensors_lpp.getBuffer(), sensors_lpp.getSize());
      uint8_t channel, type;
      while(reader.readHeader(channel, type)) {
        reader.skipData(type);
        sensors_nb ++;
      }
      sensors_scroll = sensors_nb > UI_RECENT_LIST_SIZE;
#if AUTO_OFF_MILLIS > 0
      next_sensors_refresh = millis() + 5000; // refresh sensor values every 5 sec
#else
      next_sensors_refresh = millis() + 60000; // refresh sensor values every 1 min
#endif
    }
  }

public:
  HomeScreen(UITask* task, mesh::RTCClock* rtc, SensorManager* sensors, NodePrefs* node_prefs)
     : _task(task), _rtc(rtc), _sensors(sensors), _node_prefs(node_prefs), _page(0),
       _shutdown_init(false), _clock_pm_pending(0), sensors_lpp(200) {
    if (_node_prefs != nullptr) {
      _pm_clock_mode = constrain(_node_prefs->ui_pm_clock_mode, 0, 1);
    }
  }

  bool isOnClockPage() const { return _page == CLOCK; }
  void incrementClockPM() { _clock_pm_pending++; }
  int  getPmClockMode()  const { return _pm_clock_mode; }

  void poll() override {
    if (_shutdown_init && !_task->isButtonPressed()) {  // must wait for USR button to be released
      _task->shutdown();
    }
    if (_pin_show_until && millis() >= _pin_show_until) {
      _pin_show_until = 0;  // render() will show **** on next cycle
    }
#ifdef WITH_WIFI_SWITCHING
    if (_in_wifi_select && _wifi_wait && !the_mesh.isWifiConnecting()) {
      _in_wifi_select = false;
      _wifi_wait      = false;
      WiFi.scanDelete();
      _wifi_scan_n    = -1;
    }
#endif
  }

  int render(DisplayDriver& display) override {
    char tmp[80];
    int hdr_size   = (display.width() >= 200) ? 2 : 1;
    int hdr_line_h = 8 * hdr_size;
    int dot_y      = hdr_line_h + 5;
    int content_y  = hdr_line_h + 14;
    int line_h     = 8 * hdr_size + 3;

    // CLOCK + pending PM: render full-screen like MsgPreviewScreen (no node/battery header)
    if (_page == HomePage::CLOCK && _clock_pm_pending > 0) {
      UITask::ClockPMInfo info;
      if (_task->peekTopMsg(info)) {
        int msg_start_y = hdr_line_h + 3;

        // Translate message body
        char filtered_msg[sizeof(info.msg)];
        display.translateUTF8ToBlocks(filtered_msg, info.msg, sizeof(filtered_msg));
        int msg_len = (int)strlen(filtered_msg);

        // Dynamic body font selection (same algorithm as MsgPreviewScreen)
        int body2_lines = (display.height() - msg_start_y) / 16;
        int body2_cpl   = display.width() / 12;
        int sim_lines   = 0;
        if (body2_lines >= 4) {
          int pos = 0;
          while (pos < msg_len) {
            sim_lines++;
            if (msg_len - pos <= body2_cpl) break;
            int brk = pos + body2_cpl;
            for (int j = pos + body2_cpl; j > pos; j--) {
              if (filtered_msg[j] == ' ') { brk = j; break; }
            }
            pos = brk + (filtered_msg[brk] == ' ' ? 1 : 0);
          }
        }
        int body_size = ((body2_lines >= 4) && (sim_lines <= body2_lines)) ? 2 : 1;

        // Header: "#N (D)name" + time elapsed top-right
        char time_str[8];
        int secs = (int)(_rtc->getCurrentTime() - info.timestamp);
        if (secs < 0) secs = 0;
        if      (secs < 60)   snprintf(time_str, sizeof(time_str), "%ds",  secs);
        else if (secs < 3600) snprintf(time_str, sizeof(time_str), "%dm",  secs / 60);
        else                  snprintf(time_str, sizeof(time_str), "%dh",  secs / 3600);

        int hdr_ch_w    = 6 * hdr_size;
        int hdr_total   = display.width() / hdr_ch_w;
        int name_budget = hdr_total - (int)strlen(time_str) - 4;  // "#N " + gap
        if (name_budget < 0) name_budget = 0;

        char raw_name[36];
        if (info.path_len == 0xFF)
          snprintf(raw_name, sizeof(raw_name), "(D)%s", info.from_name);
        else
          snprintf(raw_name, sizeof(raw_name), "[%d]%s", info.path_len, info.from_name);
        if (name_budget < (int)sizeof(raw_name)) raw_name[name_budget] = '\0';

        char filtered_pm_name[36];
        display.translateUTF8ToBlocks(filtered_pm_name, raw_name, sizeof(filtered_pm_name));

        char hdr_left[64];
        snprintf(hdr_left, sizeof(hdr_left), "#%d %s", _clock_pm_pending, filtered_pm_name);

        display.setTextSize(hdr_size);
        display.setColor(DisplayDriver::GREEN);
        display.setCursor(0, 0);
        display.print(hdr_left);
        int time_x = display.width() - 1 - (int)strlen(time_str) * hdr_ch_w;
        display.setCursor(time_x, 0);
        display.print(time_str);

        // Divider
        display.setColor(DisplayDriver::LIGHT);
        display.drawRect(0, hdr_line_h + 2, display.width(), 1);

        // Message body with dynamic font
        display.setTextSize(body_size);
        display.setCursor(0, msg_start_y);
        display.printWordWrap(filtered_msg, display.width());
      }
      return 60000;
    }

    // node name
    display.setTextSize(hdr_size);
    display.setColor(DisplayDriver::GREEN);
    char filtered_name[sizeof(_node_prefs->node_name)];
    display.translateUTF8ToBlocks(filtered_name, _node_prefs->node_name, sizeof(filtered_name));
    display.setCursor(0, 0);
    display.print(filtered_name);

    // on eink clock page: time sync source right-aligned next to battery
    if (_page == HomePage::CLOCK && _task->isEinkDisplay()) {
      char srcBuf[40];
      snprintf(srcBuf, sizeof(srcBuf), "%s", the_mesh.getTimeSourceLabel());
      if (the_mesh.getTimeSyncCount() > 0 && the_mesh.getTimeSource() == MyMesh::TIME_SOURCE_ADVERT) {
        snprintf(srcBuf, sizeof(srcBuf), "%s %+lds",
                 the_mesh.getTimeSourceLabel(), (long)the_mesh.getTimeLastAdjustment());
      }
      display.setTextSize(1);
      display.drawTextRightAlign(display.width() - 32, 0, srcBuf);
      display.setTextSize(hdr_size);
    }

    // battery voltage
    renderBatteryIndicator(display, _task->getBattMilliVolts());

    // curr page indicator
    int visible_count = HomePage::Count;
    int x = display.width() / 2 - 5 * (visible_count - 1);
    for (uint8_t i = 0; i < HomePage::Count; i++) {
      if (i == _page) {
        display.fillRect(x-1, dot_y-1, 3, 3);
      } else {
        display.fillRect(x, dot_y, 1, 1);
      }
      x += 10;
    }

    if (_page == HomePage::FIRST) {
      display.setColor(DisplayDriver::YELLOW);
      display.setTextSize(2);
      int unrd = _task->getUnreadMsgCount();
      if (unrd > 0)
        sprintf(tmp, "NEW: %d", unrd);
      else
        sprintf(tmp, "MSG: %d", _task->getMsgCount());
      display.drawTextCentered(display.width() / 2, content_y, tmp);

      #ifdef WIFI_SSID
        IPAddress ip = WiFi.localIP();
        snprintf(tmp, sizeof(tmp), "IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        display.setTextSize(1);
        display.drawTextCentered(display.width() / 2, display.height() - 10, tmp);
      #endif
      if (_task->hasConnection()) {
        display.setColor(DisplayDriver::GREEN);
        display.setTextSize(hdr_size);
        display.drawTextCentered(display.width() / 2, content_y + 24, "< Connected >");
#ifdef WITH_WIFI_SWITCHING
      } else if (the_mesh.isWifiConnected()) {
        String wip = the_mesh.getWifiIP();
        snprintf(tmp, sizeof(tmp), "IP:%s", wip.c_str());
        display.setColor(DisplayDriver::GREEN);
        display.setTextSize(1);
        display.drawTextCentered(display.width() / 2, content_y + 24, tmp);
      } else if (the_mesh.isWifiConnecting()) {
        display.setColor(DisplayDriver::YELLOW);
        display.setTextSize(1);
        display.drawTextCentered(display.width() / 2, content_y + 24, "WiFi...");
#endif
      } else if (the_mesh.getBLEPin() != 0) {
#ifdef WITH_WIFI_SWITCHING
        if (the_mesh.getWifiPrefs()->comms_mode != COMMS_MODE_WIFI)
#endif
        {
          display.setColor(DisplayDriver::RED);
          display.setTextSize(2);
          sprintf(tmp, "Pin:%d", the_mesh.getBLEPin());
          display.drawTextCentered(display.width() / 2, content_y + 24, tmp);
        }
      }
    } else if (_page == HomePage::CLOCK) {
      // --- Large clock display ---
      _clock_pm_pending = 0;
      time_t now = (time_t)_rtc->getCurrentTime();
      struct tm timeinfo;
      localtime_r(&now, &timeinfo);
      char timeBuf[6];
      strftime(timeBuf, sizeof(timeBuf), "%H:%M", &timeinfo);
      char sourceBuf[32];
      snprintf(sourceBuf, sizeof(sourceBuf), "%s", the_mesh.getTimeSourceLabel());
      if (the_mesh.getTimeSyncCount() > 0 && the_mesh.getTimeSource() == MyMesh::TIME_SOURCE_ADVERT) {
        snprintf(sourceBuf, sizeof(sourceBuf), "%s %+lds",
                 the_mesh.getTimeSourceLabel(), (long)the_mesh.getTimeLastAdjustment());
      }
      display.setColor(DisplayDriver::GREEN);
      if (_task->isEinkDisplay()) {
        char dateBuf[12];
        strftime(dateBuf, sizeof(dateBuf), "%a %d %b", &timeinfo);
        // source shown in header; date uses hdr_size for readability on wide displays
        int date_sz = hdr_size;
        int clock_sz = min(display.width() / 30, (display.height() - 22) / 8);
        if (clock_sz < 1) clock_sz = 1;
        if (clock_sz > 8) clock_sz = 8;
        int clock_y = (display.height() - 22 - 8 * clock_sz) / 2;
        if (clock_y < content_y) clock_y = content_y;  // don't overlap page dots
        display.setTextSize(clock_sz);
        display.drawTextCentered(display.width() / 2, clock_y, timeBuf);
        display.setTextSize(date_sz);
        display.drawTextCentered(display.width() / 2, display.height() - 20, dateBuf);
      } else if (_task->isColorTFTDisplay()) {
        char dateBuf[12];
        strftime(dateBuf, sizeof(dateBuf), "%a %d %b", &timeinfo);
        // T114 TFT 240×135, virtual canvas 128×64 (SCALE=1.875, Y_OFFSET=7).
        // setTextSize(3) activates glcdfont 6×8 renderer scaled via SCALE_X/Y — same
        // macros as all other primitives, so virtual positions match exactly:
        //   clock  virt 22–46 → phys 48–93 px  (45 px = 37.5% usable, same as OLED V3)
        //   date   virt 46    → phys 93 px      (ArialMT_Plain_24, 24 px tall → 117 px)
        //   source virt 58    → phys 115 px     (ArialMT_Plain_16, 16 px tall → 131 px)
        display.setTextSize(3);
        display.drawTextCentered(display.width() / 2, content_y, timeBuf);
        display.setTextSize(2);
        display.drawTextCentered(display.width() / 2, display.height() - 18, dateBuf);
        display.setTextSize(1);
        display.drawTextCentered(display.width() / 2, display.height() - 6, sourceBuf);
      } else {
        // OLED 128x64: size 3 (18px wide per char, 24px tall) fits "HH:MM" in 90px
        display.setTextSize(3);
        display.drawTextCentered(display.width() / 2, content_y, timeBuf);
        display.setTextSize(1);
        char dateBuf[12];
        strftime(dateBuf, sizeof(dateBuf), "%a %d %b", &timeinfo);
        display.drawTextCentered(display.width() / 2, content_y + 24 + 2, dateBuf);
        display.drawTextCentered(display.width() / 2, display.height() - 8, sourceBuf);
      }
      return 60000;  // refresh once per minute
    } else if (_page == HomePage::RECENT) {
      the_mesh.getRecentlyHeard(recent, UI_RECENT_LIST_SIZE);
      display.setColor(DisplayDriver::GREEN);
      display.setTextSize(hdr_size);
      int y = content_y;
      for (int i = 0; i < UI_RECENT_LIST_SIZE; i++, y += line_h) {
        auto a = &recent[i];
        if (a->name[0] == 0) continue;  // empty slot
        int secs = _rtc->getCurrentTime() - a->recv_timestamp;
        if (secs < 60) {
          sprintf(tmp, "%ds", secs);
        } else if (secs < 60*60) {
          sprintf(tmp, "%dm", secs / 60);
        } else {
          sprintf(tmp, "%dh", secs / (60*60));
        }

        int timestamp_width = display.getTextWidth(tmp);
        int max_name_width = display.width() - timestamp_width - 1;

        char filtered_recent_name[sizeof(a->name)];
        display.translateUTF8ToBlocks(filtered_recent_name, a->name, sizeof(filtered_recent_name));
        display.drawTextEllipsized(0, y, max_name_width, filtered_recent_name);
        display.setCursor(display.width() - timestamp_width - 1, y);
        display.print(tmp);
      }
    } else if (_page == HomePage::RADIO) {
      display.setColor(DisplayDriver::YELLOW);
      display.setTextSize(hdr_size);
      int y = content_y;
      if (hdr_size == 2) {
        snprintf(tmp, sizeof(tmp), "FQ:%.3f SF:%d", _node_prefs->freq, _node_prefs->sf);
        display.setCursor(0, y); display.print(tmp); y += line_h;
        snprintf(tmp, sizeof(tmp), "BW:%.2f  CR:%d", _node_prefs->bw, _node_prefs->cr);
        display.setCursor(0, y); display.print(tmp); y += line_h;
        snprintf(tmp, sizeof(tmp), "TX:%ddBm", _node_prefs->tx_power_dbm);
        display.setCursor(0, y); display.print(tmp); y += line_h;
        snprintf(tmp, sizeof(tmp), "Noise:%d", radio_driver.getNoiseFloor());
        display.setCursor(0, y); display.print(tmp);
      } else {
        snprintf(tmp, sizeof(tmp), "FQ: %06.3f   SF: %d", _node_prefs->freq, _node_prefs->sf);
        display.setCursor(0, y); display.print(tmp); y += line_h;
        snprintf(tmp, sizeof(tmp), "BW: %03.2f     CR: %d", _node_prefs->bw, _node_prefs->cr);
        display.setCursor(0, y); display.print(tmp); y += line_h;
        snprintf(tmp, sizeof(tmp), "TX: %ddBm", _node_prefs->tx_power_dbm);
        display.setCursor(0, y); display.print(tmp); y += line_h;
        snprintf(tmp, sizeof(tmp), "Noise floor: %d", radio_driver.getNoiseFloor());
        display.setCursor(0, y); display.print(tmp);
      }
#ifndef WITH_WIFI_SWITCHING
    } else if (_page == HomePage::BLUETOOTH) {
      display.setColor(DisplayDriver::GREEN);
      display.drawXbm((display.width() - 32) / 2, content_y,
          _task->isSerialEnabled() ? bluetooth_on : bluetooth_off,
          32, 32);
      display.setTextSize(hdr_size);
      display.drawTextCentered(display.width() / 2, display.height() - 8*hdr_size - 2, "toggle: " PRESS_LABEL);
#endif
    } else if (_page == HomePage::ADVERT) {
      display.setColor(DisplayDriver::GREEN);
      display.drawXbm((display.width() - 32) / 2, content_y, advert_icon, 32, 32);
      display.setTextSize(hdr_size);
      display.drawTextCentered(display.width() / 2, display.height() - 8*hdr_size - 2, "advert: " PRESS_LABEL);
#if ENV_INCLUDE_GPS == 1
    } else if (_page == HomePage::GPS) {
      LocationProvider* nmea = sensors.getLocationProvider();
      char buf[50];
      display.setTextSize(hdr_size);
      int y = content_y;
      bool gps_state = _task->getGPSState();
#ifdef PIN_GPS_SWITCH
      bool hw_gps_state = digitalRead(PIN_GPS_SWITCH);
      if (gps_state != hw_gps_state) {
        strcpy(buf, gps_state ? "gps off(hw)" : "gps off(sw)");
      } else {
        strcpy(buf, gps_state ? "gps on" : "gps off");
      }
#else
      strcpy(buf, gps_state ? "gps on" : "gps off");
#endif
      display.drawTextLeftAlign(0, y, buf);
      if (nmea == NULL) {
        y += line_h;
        display.drawTextLeftAlign(0, y, "Can't access GPS");
      } else {
        strcpy(buf, nmea->isValid()?"fix":"no fix");
        display.drawTextRightAlign(display.width()-1, y, buf);
        y += line_h;
        display.drawTextLeftAlign(0, y, "sat");
        sprintf(buf, "%d", nmea->satellitesCount());
        display.drawTextRightAlign(display.width()-1, y, buf);
        y += line_h;
        display.drawTextLeftAlign(0, y, "pos");
        sprintf(buf, "%.4f %.4f",
          nmea->getLatitude()/1000000., nmea->getLongitude()/1000000.);
        display.drawTextRightAlign(display.width()-1, y, buf);
        y += line_h;
        display.drawTextLeftAlign(0, y, "alt");
        sprintf(buf, "%.2f", nmea->getAltitude()/1000.);
        display.drawTextRightAlign(display.width()-1, y, buf);
      }
#endif
#if UI_SENSORS_PAGE == 1
    } else if (_page == HomePage::SENSORS) {
      display.setTextSize(hdr_size);
      int y = content_y;
      refresh_sensors();
      char buf[30];
      char name[30];
      LPPReader r(sensors_lpp.getBuffer(), sensors_lpp.getSize());

      for (int i = 0; i < sensors_scroll_offset; i++) {
        uint8_t channel, type;
        r.readHeader(channel, type);
        r.skipData(type);
      }

      for (int i = 0; i < (sensors_scroll?UI_RECENT_LIST_SIZE:sensors_nb); i++) {
        uint8_t channel, type;
        if (!r.readHeader(channel, type)) { // reached end, reset
          r.reset();
          r.readHeader(channel, type);
        }

        display.setCursor(0, y);
        float v;
        switch (type) {
          case LPP_GPS: { // GPS
            float lat, lon, alt;
            r.readGPS(lat, lon, alt);
            strcpy(name, "gps"); sprintf(buf, "%.4f %.4f", lat, lon);
            break;
          }
          case LPP_VOLTAGE:
            r.readVoltage(v);
            if (channel == SYS_CH_VDD) {
              strcpy(name, "vdd"); sprintf(buf, "%.2fV", v);
            } else {
              strcpy(name, "voltage"); sprintf(buf, "%6.2f", v);
            }
            break;
          case LPP_CURRENT:
            r.readCurrent(v);
            strcpy(name, "current"); sprintf(buf, "%.3f", v);
            break;
          case LPP_TEMPERATURE:
            r.readTemperature(v);
            if (channel == SYS_CH_CPU_TEMP) {
              // CPU die temperature
              strcpy(name, "cpu"); sprintf(buf, "%.1f C", v);
            } else if (channel == SYS_CH_UPTIME_REBOOT || channel == SYS_CH_UPTIME_CHARGE) {
              // Uptime encoded as fractional hours — format as Xh Ym or Ym Zs
              strcpy(name, channel == SYS_CH_UPTIME_REBOOT ? "up(boot)" : "up(chg)");
              uint32_t total_min = (uint32_t)(v * 60.0f + 0.5f);
              if (total_min < 60) {
                sprintf(buf, "%um%us", total_min, (uint32_t)(v * 3600.0f + 0.5f) % 60);
              } else {
                sprintf(buf, "%uh%02um", total_min / 60, total_min % 60);
              }
            } else {
              strcpy(name, "temperature"); sprintf(buf, "%.2f", v);
            }
            break;
          case LPP_RELATIVE_HUMIDITY:
            r.readRelativeHumidity(v);
            strcpy(name, "humidity"); sprintf(buf, "%.2f", v);
            break;
          case LPP_BAROMETRIC_PRESSURE:
            r.readPressure(v);
            strcpy(name, "pressure"); sprintf(buf, "%.2f", v);
            break;
          case LPP_ALTITUDE:
            r.readAltitude(v);
            strcpy(name, "altitude"); sprintf(buf, "%.0f", v);
            break;
          case LPP_POWER:
            r.readPower(v);
            strcpy(name, "power"); sprintf(buf, "%6.2f", v);
            break;
          default:
            r.skipData(type);
            strcpy(name, "unk"); sprintf(buf, "");
        }
        display.setCursor(0, y);
        display.print(name);
        display.setCursor(
          display.width()-display.getTextWidth(buf)-1, y
        );
        display.print(buf);
        y += line_h;
      }
      if (sensors_scroll) sensors_scroll_offset = (sensors_scroll_offset+1)%sensors_nb;
      else sensors_scroll_offset = 0;
#endif
    } else if (_page == HomePage::SETTINGS) {
      display.setTextSize(hdr_size);
#ifdef WITH_WIFI_SWITCHING
      if (_in_wifi_select) {
        const int LINE_H_W  = 8 * hdr_size + 2;
        const int CURSOR_W  = 6 * hdr_size;
        const int visible_w = (display.height() - content_y) / LINE_H_W;

        // Command overlay: fills content area only (header stays visible)
        if (_wifi_cmd_shown) {
          display.setColor(DisplayDriver::YELLOW);
          display.setTextSize(1);
          display.setCursor(0, content_y);
          display.printWordWrap(_wifi_cmd, display.width());
          return 60000;
        }

        // Poll async scan completion
        if (_wifi_scanning) {
          int n = WiFi.scanComplete();
          if (n >= 0) {
            _wifi_scanning    = false;
            _wifi_scan_n      = n;
            _wifi_sel         = 0;
            _wifi_scan_scroll = 0;
          }
        }

        if (the_mesh.isWifiConnecting()) {
          display.setColor(DisplayDriver::YELLOW);
          display.drawTextCentered(display.width() / 2, content_y + 8, "Connecting...");
        } else if (_wifi_scanning) {
          display.setColor(DisplayDriver::YELLOW);
          display.drawTextCentered(display.width() / 2, content_y + 8, "Scanning WiFi...");
        } else {
          int net_n = _wifi_scan_n >= 0 ? _wifi_scan_n : 0;
          WifiPrefs* wp = the_mesh.getWifiPrefs();
          int S = wp->network_count;

          // Gather unsaved scanned SSID indices
          int unsaved[20];
          int N_u = 0;
          for (int si = 0; si < net_n && N_u < 20; si++) {
            String ss = WiFi.SSID(si);
            bool found = false;
            for (int k = 0; k < S; k++) {
              if (ss == wp->networks[k].ssid) { found = true; break; }
            }
            if (!found) unsaved[N_u++] = si;
          }

          // RSSI for each saved network (-200 if not visible)
          int saved_rssi[WIFI_MAX_NETWORKS];
          for (int k = 0; k < S; k++) {
            saved_rssi[k] = -200;
            for (int si = 0; si < net_n; si++) {
              if (WiFi.SSID(si) == wp->networks[k].ssid) { saved_rssi[k] = WiFi.RSSI(si); break; }
            }
          }

          // Items: 0..S-1 = saved SSIDs, S..S+N_u-1 = unsaved scan, S+N_u = BLE, +1 = USB, +2 = Back
          int total_n = S + N_u + 3;
          int max_ssid = display.width() / (6 * hdr_size) - 5;
          if (max_ssid < 4) max_ssid = 4;

          display.setColor(DisplayDriver::LIGHT);
          for (int vi = 0; vi < visible_w; vi++) {
            int i = _wifi_scan_scroll + vi;
            if (i >= total_n) break;
            int y = content_y + vi * LINE_H_W;
            display.setTextSize(hdr_size);
            display.setCursor(0, y);
            display.print(_wifi_sel == i ? ">" : " ");
            display.setCursor(CURSOR_W, y);
            if (i < S) {
              display.print("*");
              display.setCursor(CURSOR_W * 2, y);
              char sbuf[33];
              strncpy(sbuf, wp->networks[i].ssid, max_ssid);
              sbuf[max_ssid] = '\0';
              display.print(sbuf);
              char rbuf[6];
              if (saved_rssi[i] > -200) snprintf(rbuf, sizeof(rbuf), "%4d", saved_rssi[i]);
              else                       strcpy(rbuf, " ---");
              display.drawTextRightAlign(display.width() - 1, y, rbuf);
            } else if (i < S + N_u) {
              display.print(" ");
              display.setCursor(CURSOR_W * 2, y);
              int si = unsaved[i - S];
              String ss = WiFi.SSID(si);
              char sbuf[33];
              strncpy(sbuf, ss.c_str(), max_ssid);
              sbuf[max_ssid] = '\0';
              display.print(sbuf);
              char rbuf[6];
              snprintf(rbuf, sizeof(rbuf), "%4d", WiFi.RSSI(si));
              display.drawTextRightAlign(display.width() - 1, y, rbuf);
            } else if (i == S + N_u)     { display.print("  BLE"); }
            else if (i == S + N_u + 1)   { display.print("  USB"); }
            else                          { display.print("  Back"); }
          }
        }
        return (_wifi_scanning || the_mesh.isWifiConnecting()) ? 400 : 200;
      } else
#endif
      if (!_in_settings) {
        display.setColor(DisplayDriver::GREEN);
        display.drawTextCentered(display.width() / 2, content_y + 8, "Settings");
        display.drawTextCentered(display.width() / 2, content_y + 24, PRESS_LABEL " to enter");
      } else {
        static const char* pm_clok_vals[2] = { "all", "PM" };
        static const char* dim_vals[2]     = { "ON", "OFF" };
#ifdef WITH_COMPANION_CLI
        static const char* chat_vals[4] = { "C+P", "cht", "PM", "OFF" };
        static const char* ts_vals[3]   = { "a+g", "gps", "adv" };
#endif
        const int CURSOR_W = 6 * hdr_size, LINE_H = 8 * hdr_size + 2, Y0 = content_y;
        _settings_visible = (display.height() - Y0) / LINE_H;
        display.setColor(DisplayDriver::LIGHT);
        for (int i = _settings_scroll; i < SETTINGS_N && i < _settings_scroll + _settings_visible; i++) {
          int y = Y0 + (i - _settings_scroll) * LINE_H;
          display.setColor(DisplayDriver::LIGHT);
          display.setCursor(0, y);
          display.print(_settings_sel == i ? ">" : " ");
          display.setCursor(CURSOR_W, y);
          // label
          const char* lbl = "Back";
#ifdef WITH_COMPANION_CLI
          if      (i == 0) lbl = "CLI Chat";
          else if (i == 1) lbl = "CLI PIN";
          else if (i == 2) lbl = "Timesync";
          else
#endif
#ifdef WITH_WIFI_SWITCHING
          if      (i == SETTINGS_COMMS_IDX) lbl = "Comms";
          else
#endif
          if      (i == SETTINGS_PM_IDX)   lbl = "PM CLOCK";
          else if (i == SETTINGS_DIM_IDX)  lbl = "CLOCK DIM";
#ifdef WITH_COMPANION_CLI
          else if (i == SETTINGS_C2L_CH_IDX) lbl = "Cyr2Lat Chan";
          else if (i == SETTINGS_C2L_DM_IDX) lbl = "Cyr2Lat DM";
#endif
          else if (i == SETTINGS_ROT_IDX)    lbl = "Rotation";
          else if (i == SETTINGS_UNREAD_IDX) lbl = "Max Unread";
          else if (i == SETTINGS_LOG_IDX)    lbl = "Max Log";
          display.print(lbl);
          // value (not for Back)
          if (i != SETTINGS_BACK_IDX) {
            char val[12] = "";
#ifdef WITH_COMPANION_CLI
            if (i == 0) {
              snprintf(val, sizeof(val), "%s", chat_vals[the_mesh.getChatMode()]);
            } else if (i == 1) {
              bool show = _pin_show_until && millis() < _pin_show_until;
              snprintf(val, sizeof(val), "%s", show ? the_mesh.getCliPin() : "****");
            } else if (i == 2) {
              snprintf(val, sizeof(val), "%s", ts_vals[the_mesh.getTimesyncMode()]);
            } else
#endif
#ifdef WITH_WIFI_SWITCHING
            if (i == SETTINGS_COMMS_IDX) {
              uint8_t mode = the_mesh.getWifiPrefs()->comms_mode;
              if (mode == COMMS_MODE_WIFI) {
                if (the_mesh.isWifiConnecting())     snprintf(val, sizeof(val), "WiFi ...");
                else if (the_mesh.isWifiConnected()) snprintf(val, sizeof(val), "WiFi OK");
                else                                 snprintf(val, sizeof(val), "WiFi");
              } else if (mode == COMMS_MODE_USB) {
                snprintf(val, sizeof(val), "USB");
              } else {
                snprintf(val, sizeof(val), "BLE");
              }
            } else
#endif
            if (i == SETTINGS_PM_IDX) {
              snprintf(val, sizeof(val), "%s", pm_clok_vals[_pm_clock_mode]);
            } else if (i == SETTINGS_DIM_IDX) {
              snprintf(val, sizeof(val), "%s", dim_vals[_task->getClockDimMode()]);
#ifdef WITH_COMPANION_CLI
            } else if (i == SETTINGS_C2L_CH_IDX) {
              snprintf(val, sizeof(val), "%s", the_mesh.isCyr2LatChannelsEnabled() ? "On" : "Off");
            } else if (i == SETTINGS_C2L_DM_IDX) {
              snprintf(val, sizeof(val), "%s", the_mesh.isCyr2LatContactsEnabled() ? "On" : "Off");
#endif
            } else if (i == SETTINGS_ROT_IDX && _node_prefs) {
              static const char* rot_vals[4] = { "0", "90", "180", "270" };
              snprintf(val, sizeof(val), "%s deg", rot_vals[constrain(_node_prefs->ui_display_rotation, 0, 3)]);
            } else if (i == SETTINGS_UNREAD_IDX && _node_prefs) {
              static const int unread_vals[3] = { 16, 32, 64 };
              snprintf(val, sizeof(val), "%d", unread_vals[constrain(_node_prefs->ui_max_unread_idx, 0, 2)]);
            } else if (i == SETTINGS_LOG_IDX && _node_prefs) {
              static const int log_vals[3] = { 16, 32, 64 };
              snprintf(val, sizeof(val), "%d", log_vals[constrain(_node_prefs->ui_max_log_idx, 0, 2)]);
            }
            display.drawTextRightAlign(display.width() - 2, y, val);
          }
        }
      }
    } else if (_page == HomePage::SHUTDOWN) {
      display.setColor(DisplayDriver::GREEN);
      display.setTextSize(hdr_size);
      if (_shutdown_init) {
        display.drawTextCentered(display.width() / 2, content_y + 16, "hibernating...");
      } else {
        display.drawXbm((display.width() - 32) / 2, content_y, power_icon, 32, 32);
        display.drawTextCentered(display.width() / 2, display.height() - 8*hdr_size - 2, "hibernate:" PRESS_LABEL);
      }
    }
    if (_page == HomePage::SETTINGS && _in_settings && _pin_show_until) {
      unsigned long now = millis();
      if (now < _pin_show_until) {
        int ms = (int)(_pin_show_until - now);
        return ms < 100 ? 100 : ms;  // refresh exactly when PIN expires
      }
    }
    return 5000;   // next render after 5000 ms
  }

  bool handleInput(char c) override {
    // On CLOCK page: intercept navigation while PMs are being read inline
    if (_page == HomePage::CLOCK && _clock_pm_pending > 0) {
      if (c == KEY_NEXT || c == KEY_RIGHT) {
        _task->consumeTopMsg();
        _clock_pm_pending--;
        return true;
      }
      if (c == KEY_LEFT || c == KEY_PREV) {
        return true;  // block page nav while reading PMs
      }
    }

#ifdef WITH_WIFI_SWITCHING
    if (_page == HomePage::SETTINGS && _in_wifi_select) {
      // Any key dismisses the full-screen command overlay
      if (_wifi_cmd_shown) {
        _wifi_cmd_shown = false;
        return true;
      }
      if (_wifi_scanning) return true;  // absorb keys during scan
      if (!the_mesh.isWifiConnecting()) {
        int net_n = _wifi_scan_n >= 0 ? _wifi_scan_n : 0;
        WifiPrefs* wp = the_mesh.getWifiPrefs();
        int S = wp->network_count;
        // Rebuild unsaved indices (same logic as render)
        int unsaved[20];
        int N_u = 0;
        for (int si = 0; si < net_n && N_u < 20; si++) {
          String ss = WiFi.SSID(si);
          bool found = false;
          for (int k = 0; k < S; k++) {
            if (ss == wp->networks[k].ssid) { found = true; break; }
          }
          if (!found) unsaved[N_u++] = si;
        }
        int total_n = S + N_u + 3;

        if (c == KEY_NEXT || c == KEY_RIGHT) {
          _wifi_sel = (_wifi_sel + 1) % total_n;
          if (_wifi_sel < _wifi_scan_scroll) _wifi_scan_scroll = _wifi_sel;
          if (_wifi_sel >= _wifi_scan_scroll + 3) _wifi_scan_scroll = _wifi_sel - 2;
        } else if (c == KEY_LEFT || c == KEY_PREV) {
          WiFi.scanDelete(); _wifi_scan_n = -1;
          _in_wifi_select = false;
        } else if (c == KEY_ENTER) {
          if (_wifi_sel < S) {
            // saved SSID → connect
            WiFi.scanDelete(); _wifi_scan_n = -1;
            the_mesh.switchCommsMode(COMMS_MODE_WIFI, _wifi_sel);
            _wifi_wait = true;
          } else if (_wifi_sel < S + N_u) {
            // unsaved scanned → show full-screen command overlay
            int si = unsaved[_wifi_sel - S];
            String ss = WiFi.SSID(si);
            snprintf(_wifi_cmd, sizeof(_wifi_cmd),
                     "wifi add\n%.32s\n<pass>", ss.c_str());
            _wifi_cmd_shown = true;
          } else if (_wifi_sel == S + N_u) {
            WiFi.scanDelete(); _wifi_scan_n = -1;
            the_mesh.switchCommsMode(COMMS_MODE_BLE);
            _in_wifi_select = false;
          } else if (_wifi_sel == S + N_u + 1) {
            WiFi.scanDelete(); _wifi_scan_n = -1;
            the_mesh.switchCommsMode(COMMS_MODE_USB);
            _in_wifi_select = false;
          } else {
            WiFi.scanDelete(); _wifi_scan_n = -1;
            _in_wifi_select = false;
          }
        }
      }
      return true;
    }
#endif
    // Settings edit mode absorbs all navigation before standard page switching
    if (_page == HomePage::SETTINGS && _in_settings) {
      // ● long (KEY_CANCEL) or KEY_PREV = exit settings
      if (c == KEY_CANCEL || c == KEY_PREV) {
        _in_settings = false;
        return true;
      }
      // ↑ (KEY_UP) = previous item, with wrap-around scroll
      if (c == KEY_UP) {
        _settings_sel = (_settings_sel + SETTINGS_N - 1) % SETTINGS_N;
        if (_settings_sel == SETTINGS_N - 1) {
          _settings_scroll = (SETTINGS_N > _settings_visible) ? (SETTINGS_N - _settings_visible) : 0;
        } else if (_settings_sel < _settings_scroll) {
          _settings_scroll = _settings_sel;
        }
        return true;
      }
      // ↓ (KEY_DOWN) or KEY_NEXT = next item, with wrap-around scroll
      if (c == KEY_DOWN || c == KEY_NEXT) {
        _settings_sel = (_settings_sel + 1) % SETTINGS_N;
        if (_settings_sel == 0) _settings_scroll = 0;
        else if (_settings_sel >= _settings_scroll + _settings_visible) _settings_scroll = _settings_sel - _settings_visible + 1;
        return true;
      }
      // → (KEY_RIGHT) or ● short (KEY_ENTER) = cycle value forward
      // ← (KEY_LEFT) = cycle value backward
      // For binary items direction doesn't matter; for multi-value LEFT reverses.
      if (c == KEY_RIGHT || c == KEY_ENTER || c == KEY_LEFT) {
        bool fwd = (c != KEY_LEFT);
        if (_settings_sel == SETTINGS_BACK_IDX) {
          _in_settings = false;
        } else if (_settings_sel == SETTINGS_PM_IDX) {
          _pm_clock_mode = (_pm_clock_mode + 1) % 2;  // binary — same either way
          _node_prefs->ui_pm_clock_mode = _pm_clock_mode;
          the_mesh.savePrefs();
        } else if (_settings_sel == SETTINGS_DIM_IDX) {
          _task->setClockDimMode((_task->getClockDimMode() + 1) % 2);
          the_mesh.savePrefs();
#ifdef WITH_COMPANION_CLI
        } else if (_settings_sel == SETTINGS_C2L_CH_IDX) {
          the_mesh.setCyr2LatChannelsEnabled(!the_mesh.isCyr2LatChannelsEnabled());
        } else if (_settings_sel == SETTINGS_C2L_DM_IDX) {
          the_mesh.setCyr2LatContactsEnabled(!the_mesh.isCyr2LatContactsEnabled());
        } else if (_settings_sel == 0) {
          int m = the_mesh.getChatMode();
          the_mesh.setChatMode(fwd ? (m + 1) % 4 : (m + 3) % 4);
        } else if (_settings_sel == 1) {
          _pin_show_until = millis() + 3000;
        } else if (_settings_sel == 2) {
          int m = the_mesh.getTimesyncMode();
          the_mesh.setTimesyncMode(fwd ? (m + 1) % 3 : (m + 2) % 3);
#endif
#ifdef WITH_WIFI_SWITCHING
        } else if (_settings_sel == SETTINGS_COMMS_IDX) {
          if (!_wifi_wait && fwd) {
            _in_wifi_select   = true;
            _wifi_sel         = 0;
            _wifi_scan_scroll = 0;
            _wifi_scanning    = true;
            _wifi_scan_n      = -1;
            WiFi.mode(WIFI_STA);
            WiFi.scanNetworks(true);  // async
          }
#endif
        } else if (_settings_sel == SETTINGS_ROT_IDX && _node_prefs) {
          int r = _node_prefs->ui_display_rotation;
          r = fwd ? (r + 1) % 4 : (r + 3) % 4;
          _node_prefs->ui_display_rotation = r;
          _task->setDisplayRotation(r);
          the_mesh.savePrefs();
        } else if (_settings_sel == SETTINGS_UNREAD_IDX && _node_prefs) {
          int idx = _node_prefs->ui_max_unread_idx;
          idx = fwd ? (idx + 1) % 3 : (idx + 2) % 3;
          _node_prefs->ui_max_unread_idx = idx;
          static const int size_table[3] = { 16, 32, 64 };
          _task->updateMsgMaxSizes(size_table[_node_prefs->ui_max_unread_idx],
                                   size_table[_node_prefs->ui_max_log_idx]);
          the_mesh.savePrefs();
        } else if (_settings_sel == SETTINGS_LOG_IDX && _node_prefs) {
          int idx = _node_prefs->ui_max_log_idx;
          idx = fwd ? (idx + 1) % 3 : (idx + 2) % 3;
          _node_prefs->ui_max_log_idx = idx;
          static const int size_table[3] = { 16, 32, 64 };
          _task->updateMsgMaxSizes(size_table[_node_prefs->ui_max_unread_idx],
                                   size_table[_node_prefs->ui_max_log_idx]);
          the_mesh.savePrefs();
        }
        return true;
      }
      return true;  // absorb all other keys in edit mode
    }

    if (c == KEY_LEFT || c == KEY_PREV || c == KEY_UP) {
      _page = (_page + HomePage::Count - 1) % HomePage::Count;
      return true;
    }
    if (c == KEY_NEXT || c == KEY_RIGHT || c == KEY_DOWN) {
      _page = (_page + 1) % HomePage::Count;
      return true;
    }
#ifndef WITH_WIFI_SWITCHING
    if (c == KEY_ENTER && _page == HomePage::BLUETOOTH) {
      if (_task->isSerialEnabled()) {  // toggle Bluetooth on/off
        _task->disableSerial();
      } else {
        _task->enableSerial();
      }
      return true;
    }
#endif
    if (c == KEY_ENTER && _page == HomePage::ADVERT) {
      _task->notify(UIEventType::ack);
      if (the_mesh.advert()) {
        _task->showAlert("Advert sent!", 1000);
      } else {
        _task->showAlert("Advert failed..", 1000);
      }
      return true;
    }
#if ENV_INCLUDE_GPS == 1
    if (c == KEY_ENTER && _page == HomePage::GPS) {
      _task->toggleGPS();
      return true;
    }
#endif
#if UI_SENSORS_PAGE == 1
    if (c == KEY_ENTER && _page == HomePage::SENSORS) {
      _task->toggleGPS();
      next_sensors_refresh=0;
      return true;
    }
#endif
    if (c == KEY_ENTER && _page == HomePage::SETTINGS) {
      _in_settings = true;
      _settings_sel = 0;
      _settings_scroll = 0;
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::SHUTDOWN) {
      _shutdown_init = true;  // need to wait for button to be released
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::FIRST) {
      if (_task->getUnreadMsgCount() > 0) {
        _task->gotoMsgPreview();
        return true;
      }
      if (_task->getLogCount() > 0) {
        _task->gotoMsgHistory();
        return true;
      }
      return false;
    }
    return false;
  }
};

class MsgPreviewScreen : public UIScreen {
  UITask* _task;
  mesh::RTCClock* _rtc;

  #define MAX_UNREAD_MSGS   64   // compile-time array capacity; runtime cap via _max_unread
  int num_unread;
  int head = MAX_UNREAD_MSGS - 1; // index of latest unread message

public:
  struct MsgEntry {
    uint32_t timestamp;
    uint8_t  path_len;       // 0xFF = direct, otherwise hop count (group)
    bool     is_pm;
    char     from_name[32];  // sender name (direct) or channel name (group)
    char     msg[MAX_TEXT_LEN];
  };

  MsgPreviewScreen(UITask* task, mesh::RTCClock* rtc) : _task(task), _rtc(rtc) { num_unread = 0; }

  #define MAX_LOG_MSGS  64   // compile-time array capacity; runtime cap via _max_log
  bool _in_history  = false;
  int  _hist_cursor = 0;

  int _max_unread = 32;  // runtime cap, changed via setMaxSizes()
  int _max_log    = 32;

  void setMaxSizes(int max_unread, int max_log) {
    _max_unread = constrain(max_unread, 1, MAX_UNREAD_MSGS);
    _max_log    = constrain(max_log,    1, MAX_LOG_MSGS);
  }

private:
  MsgEntry unread[MAX_UNREAD_MSGS];
  MsgEntry _log[MAX_LOG_MSGS];
  int _log_head  = 0;
  int _log_count = 0;

  int indexFromNewest(int offset) const {
    return (head - offset + MAX_UNREAD_MSGS) % MAX_UNREAD_MSGS;
  }

  void pushEntry(const MsgEntry& entry) {
    head = (head + 1) % MAX_UNREAD_MSGS;
    if (num_unread < _max_unread) num_unread++;
    unread[head] = entry;
  }

public:
  void addPreview(uint8_t path_len, const char* from_name, const char* msg, bool is_pm) {
    MsgEntry entry = {};
    auto p = &entry;
    p->timestamp = _rtc->getCurrentTime();
    p->path_len  = path_len;
    p->is_pm     = is_pm;
    strncpy(p->from_name, from_name, sizeof(p->from_name) - 1);
    p->from_name[sizeof(p->from_name) - 1] = '\0';
    StrHelper::strncpy(p->msg, msg, sizeof(p->msg));
    pushEntry(entry);
    _log[_log_head] = entry;
    _log_head = (_log_head + 1) % MAX_LOG_MSGS;
    if (_log_count < _max_log) _log_count++;
    _in_history = false;  // new message interrupts history browse
  }

  int  logCount()       const { return _log_count; }
  void enterHistoryMode()     { _in_history = true; _hist_cursor = 0; }

  // Peek at the current (latest) unread message without consuming it
  const MsgEntry* peekCurrent() const {
    if (num_unread == 0) return nullptr;
    return &unread[head];
  }

  const MsgEntry* peekLatestPM() const {
    for (int i = 0; i < num_unread; i++) {
      const MsgEntry* p = &unread[indexFromNewest(i)];
      if (p->is_pm) return p;
    }
    return nullptr;
  }

  // Advance past the current message (mark as read) without switching screens
  void consumeOne() {
    if (num_unread == 0) return;
    head = (head + MAX_UNREAD_MSGS - 1) % MAX_UNREAD_MSGS;
    num_unread--;
  }

  bool consumeLatestPM() {
    int target = -1;
    for (int i = 0; i < num_unread; i++) {
      if (unread[indexFromNewest(i)].is_pm) {
        target = i;
        break;
      }
    }
    if (target < 0) return false;

    static MsgEntry kept[MAX_UNREAD_MSGS];  // static: avoids 6 KB stack allocation
    int kept_count = 0;
    for (int i = 0; i < num_unread; i++) {
      if (i == target) continue;
      kept[kept_count++] = unread[indexFromNewest(i)];
    }

    num_unread = 0;
    head = MAX_UNREAD_MSGS - 1;
    for (int i = kept_count - 1; i >= 0; i--) {
      pushEntry(kept[i]);
    }
    return true;
  }

  int unreadCount() const { return num_unread; }

  int render(DisplayDriver& display) override {
    if (_in_history) {
      if (_log_count == 0) { _in_history = false; return 1000; }
      int idx = (_log_head - _log_count + _hist_cursor + MAX_LOG_MSGS * 2) % MAX_LOG_MSGS;
      const MsgEntry* p = &_log[idx];

      int hdr_size   = (display.width() >= 200) ? 2 : 1;
      int hdr_line_h = 8 * hdr_size;
      int msg_start_y = hdr_line_h + 3;

      char filtered_msg[MAX_TEXT_LEN];
      display.translateUTF8ToBlocks(filtered_msg, p->msg, sizeof(filtered_msg));
      int msg_len = (int)strlen(filtered_msg);

      int body2_lines = (display.height() - msg_start_y) / 16;
      int body2_cpl   = display.width() / 12;
      int sim_lines = 0;
      if (body2_lines >= 4) {
        int pos = 0;
        while (pos < msg_len) {
          sim_lines++;
          if (msg_len - pos <= body2_cpl) break;
          int brk = pos + body2_cpl;
          for (int i = pos + body2_cpl; i > pos; i--) {
            if (filtered_msg[i] == ' ') { brk = i; break; }
          }
          pos = brk + (filtered_msg[brk] == ' ' ? 1 : 0);
        }
      }
      int body_size = ((body2_lines >= 4) && (sim_lines <= body2_lines)) ? 2 : 1;

      char time_str[8];
      int secs = (int)(_rtc->getCurrentTime() - p->timestamp);
      if (secs < 0) secs = 0;
      if      (secs < 60)   snprintf(time_str, sizeof(time_str), "%ds",  secs);
      else if (secs < 3600) snprintf(time_str, sizeof(time_str), "%dm",  secs / 60);
      else                  snprintf(time_str, sizeof(time_str), "%dh",  secs / 3600);

      int hdr_ch_w    = 6 * hdr_size;
      int hdr_total   = display.width() / hdr_ch_w;
      int name_budget = hdr_total - (int)strlen(time_str) - 6; // "[N/M] " prefix
      if (name_budget < 0) name_budget = 0;

      char raw_name[36];
      if (p->path_len == 0xFF)
        snprintf(raw_name, sizeof(raw_name), "(D)%s", p->from_name);
      else
        snprintf(raw_name, sizeof(raw_name), "[%d]%s", p->path_len, p->from_name);
      if (name_budget < (int)sizeof(raw_name)) raw_name[name_budget] = '\0';

      char filtered_name[36];
      display.translateUTF8ToBlocks(filtered_name, raw_name, sizeof(filtered_name));

      char hdr_left[64];
      snprintf(hdr_left, sizeof(hdr_left), "#%d/%d %s",
               _hist_cursor + 1, _log_count, filtered_name);

      display.setTextSize(hdr_size);
      display.setColor(DisplayDriver::YELLOW);
      display.setCursor(0, 0);
      display.print(hdr_left);
      int time_x = display.width() - 1 - (int)strlen(time_str) * hdr_ch_w;
      display.setCursor(time_x, 0);
      display.print(time_str);

      display.setColor(DisplayDriver::LIGHT);
      display.drawRect(0, hdr_line_h + 2, display.width(), 1);

      display.setTextSize(body_size);
      display.setCursor(0, msg_start_y);
      display.printWordWrap(filtered_msg, display.width());

      return (AUTO_OFF_MILLIS == 0) ? 10000 : 1000;
    }
    if (num_unread == 0) return 1000;
    auto* p = &unread[head];

    // 1. Metrics
    int hdr_size    = (display.width() >= 200) ? 2 : 1;
    int hdr_line_h  = 8 * hdr_size;
    int msg_start_y = hdr_line_h + 3;

    char filtered_msg[MAX_TEXT_LEN];
    display.translateUTF8ToBlocks(filtered_msg, p->msg, sizeof(filtered_msg));
    int msg_len = (int)strlen(filtered_msg);

    int body2_lines = (display.height() - msg_start_y) / 16;
    int body2_cpl   = display.width() / 12;
    // Simulate word wrap at size 2 to get actual line count
    int sim_lines = 0;
    if (body2_lines >= 4) {
      int pos = 0;
      while (pos < msg_len) {
        sim_lines++;
        if (msg_len - pos <= body2_cpl) break;
        int brk = pos + body2_cpl;
        for (int i = pos + body2_cpl; i > pos; i--) {
          if (filtered_msg[i] == ' ') { brk = i; break; }
        }
        pos = brk + (filtered_msg[brk] == ' ' ? 1 : 0);
      }
    }
    bool use2     = (body2_lines >= 4) && (sim_lines <= body2_lines);
    int body_size = use2 ? 2 : 1;

    // 2. Time string
    char time_str[8];
    int secs = (int)(_rtc->getCurrentTime() - p->timestamp);
    if (secs < 0) secs = 0;
    if      (secs < 60)   snprintf(time_str, sizeof(time_str), "%ds",  secs);
    else if (secs < 3600) snprintf(time_str, sizeof(time_str), "%dm",  secs / 60);
    else                  snprintf(time_str, sizeof(time_str), "%dh",  secs / 3600);

    // 3. Header left: "#N (D)name" for direct, "#N channel" for group
    int hdr_ch_w    = 6 * hdr_size;
    int hdr_total   = display.width() / hdr_ch_w;
    int name_budget = hdr_total - (int)strlen(time_str) - 4;  // 4 = "#N " + gap
    if (name_budget < 0) name_budget = 0;

    char raw_name[36];
    if (p->path_len == 0xFF)
      snprintf(raw_name, sizeof(raw_name), "(D)%s", p->from_name);
    else
      snprintf(raw_name, sizeof(raw_name), "[%d]%s", p->path_len, p->from_name);
    if (name_budget < (int)sizeof(raw_name)) raw_name[name_budget] = '\0';

    char filtered_name[36];
    display.translateUTF8ToBlocks(filtered_name, raw_name, sizeof(filtered_name));

    char hdr_left[64];
    snprintf(hdr_left, sizeof(hdr_left), "#%d %s", num_unread, filtered_name);

    // 4. Render header
    display.setTextSize(hdr_size);
    display.setColor(DisplayDriver::GREEN);
    display.setCursor(0, 0);
    display.print(hdr_left);
    int time_x = display.width() - 1 - (int)strlen(time_str) * hdr_ch_w;
    display.setCursor(time_x, 0);
    display.print(time_str);

    // 5. Divider
    display.setColor(DisplayDriver::LIGHT);
    display.drawRect(0, hdr_line_h + 2, display.width(), 1);

    // 6. Message body — word wrap
    display.setTextSize(body_size);
    display.setCursor(0, msg_start_y);
    display.printWordWrap(filtered_msg, display.width());

    return (AUTO_OFF_MILLIS == 0) ? 10000 : 1000;
  }

  bool handleInput(char c) override {
    if (_in_history) {
      // → / ↓ = newer message (or exit at the newest end)
      if (c == KEY_NEXT || c == KEY_RIGHT || c == KEY_DOWN) {
        _hist_cursor++;
        if (_hist_cursor >= _log_count) {
          _in_history = false;
          _task->gotoHomeScreen();
        }
        return true;
      }
      // ← / ↑ = previous (older) message
      if (c == KEY_PREV || c == KEY_LEFT || c == KEY_UP) {
        if (_hist_cursor > 0) _hist_cursor--;
        return true;
      }
      // ● short / ● long = exit history
      if (c == KEY_ENTER || c == KEY_CANCEL) {
        _in_history = false;
        _task->gotoHomeScreen();
        return true;
      }
      return false;
    }
    // → = next unread (dismiss current)
    if (c == KEY_NEXT || c == KEY_RIGHT) {
      head = (head + MAX_UNREAD_MSGS - 1) % MAX_UNREAD_MSGS;
      num_unread--;
      if (num_unread == 0) {
        // All unread dismissed — slide into history at the newest message
        // so the user can confirm they've read it and navigate back if needed.
        if (_log_count > 0) {
          _in_history    = true;
          _hist_cursor   = _log_count - 1;  // newest entry
        } else {
          _task->gotoHomeScreen();
        }
      }
      return true;
    }
    // ← = выход (back to home)
    if (c == KEY_LEFT) {
      _task->gotoHomeScreen();
      return true;
    }
    // ↓ = clear all unread messages
    if (c == KEY_DOWN) {
      num_unread = 0;
      head = MAX_UNREAD_MSGS - 1;
      _task->gotoHomeScreen();
      return true;
    }
    // KEY_UP reserved for quick reply (TODO: implement)
    if (c == KEY_ENTER || c == KEY_CANCEL) {
      _task->gotoHomeScreen();
      return true;
    }
    return false;
  }
};

void UITask::begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs) {
  _display = display;
  _sensors = sensors;
  _auto_off = millis() + AUTO_OFF_MILLIS;

#ifdef DISPLAY_TZ
  setenv("TZ", DISPLAY_TZ, 1);
  tzset();
#endif

#if defined(PIN_USER_BTN)
  user_btn.begin();
#endif
#if UI_HAS_JOYSTICK
  joystick_left.begin();
  joystick_right.begin();
  back_btn.begin();
  joystick_down.begin();
#endif
#if defined(PIN_USER_BTN_ANA)
  analog_btn.begin();
#endif

  _node_prefs = node_prefs;
  if (_node_prefs != NULL) {
    setClockDimMode(_node_prefs->ui_clock_dim_mode);
  }

  if (_display != NULL) {
    // Pre-set rotation BEFORE turnOn() so GxEPDDisplay::begin() initialises the panel
    // with the correct scan direction from the very first display() call.
    if (_node_prefs != NULL) {
      _display->setRotation(_node_prefs->ui_display_rotation);
    }
    _display->turnOn();
  }

#ifdef PIN_BUZZER
  buzzer.begin();
  buzzer.quiet(_node_prefs->buzzer_quiet);
#endif

#ifdef PIN_VIBRATION
  vibration.begin();
#endif

  ui_started_at = millis();
  _alert_expiry = 0;

  splash = new SplashScreen(this);
  home = new HomeScreen(this, &rtc_clock, sensors, node_prefs);
  msg_preview = new MsgPreviewScreen(this, &rtc_clock);
  // Apply runtime buffer sizes from prefs
  if (_node_prefs != NULL) {
    static const int size_table[3] = { 16, 32, 64 };
    int mu = size_table[constrain(_node_prefs->ui_max_unread_idx, 0, 2)];
    int ml = size_table[constrain(_node_prefs->ui_max_log_idx,    0, 2)];
    ((MsgPreviewScreen*)msg_preview)->setMaxSizes(mu, ml);
  }
  setCurrScreen(splash);
}

void UITask::setClockDimMode(int m) {
  _clock_dim_mode = constrain(m, 0, 1);
  if (_node_prefs != NULL) {
    _node_prefs->ui_clock_dim_mode = _clock_dim_mode;
  }
}

void UITask::showAlert(const char* text, int duration_millis) {
  strcpy(_alert, text);
  _alert_expiry = millis() + duration_millis;
}

void UITask::notify(UIEventType t) {
#if defined(PIN_BUZZER)
switch(t){
  case UIEventType::contactMessage:
    // gemini's pick
    buzzer.play("MsgRcv3:d=4,o=6,b=200:32e,32g,32b,16c7");
    break;
  case UIEventType::channelMessage:
    buzzer.play("kerplop:d=16,o=6,b=120:32g#,32c#");
    break;
  case UIEventType::ack:
    buzzer.play("ack:d=32,o=8,b=120:c");
    break;
  case UIEventType::roomMessage:
  case UIEventType::newContactMessage:
  case UIEventType::none:
  default:
    break;
}
#endif

#ifdef PIN_VIBRATION
  // Trigger vibration for all UI events except none
  if (t != UIEventType::none) {
    vibration.trigger();
  }
#endif
  _next_refresh = 0;  // always trigger a redraw on any notification
}


void UITask::msgRead(int msgcount) {
  _msgcount = msgcount;
  if (msgcount == 0) {
    gotoHomeScreen();
  }
}

void UITask::newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount, bool is_pm) {
  _msgcount = msgcount;

  bool on_clock = (curr == home) && ((HomeScreen*)home)->isOnClockPage();

  ((MsgPreviewScreen*)msg_preview)->addPreview(path_len, from_name, text, is_pm);

  if (on_clock) {
    int pm_mode = ((HomeScreen*)home)->getPmClockMode();
    if (pm_mode == 1 && is_pm) {
      // PM inline: stay on clock page, show PM overlay
      ((HomeScreen*)home)->incrementClockPM();
      _next_refresh = 100;
    } else if (pm_mode == 0) {
      // All messages switch to msg_preview
      setCurrScreen(msg_preview);
    }
    // pm_mode==1 && !is_pm: silently queued, no screen switch
  } else {
    setCurrScreen(msg_preview);
  }

  if (_display != NULL) {
    if (!_display->isOn() && !hasConnection()) {
      _display->turnOn();
    }
    if (_display->isOn()) {
      _auto_off = millis() + AUTO_OFF_MILLIS;  // extend the auto-off timer
      _next_refresh = 100;  // trigger refresh
    }
  }
}

bool UITask::peekTopMsg(ClockPMInfo& out) const {
  if (!msg_preview) return false;
  const MsgPreviewScreen::MsgEntry* p = ((MsgPreviewScreen*)msg_preview)->peekLatestPM();
  if (!p) return false;
  out.path_len  = p->path_len;
  out.timestamp = p->timestamp;
  strncpy(out.from_name, p->from_name, sizeof(out.from_name) - 1);
  out.from_name[sizeof(out.from_name) - 1] = '\0';
  strncpy(out.msg, p->msg, sizeof(out.msg) - 1);
  out.msg[sizeof(out.msg) - 1] = '\0';
  return true;
}

void UITask::consumeTopMsg() {
  if (msg_preview) ((MsgPreviewScreen*)msg_preview)->consumeLatestPM();
}

int UITask::getUnreadMsgCount() const {
  if (!msg_preview) return 0;
  return ((MsgPreviewScreen*)msg_preview)->unreadCount();
}

int UITask::getLogCount() const {
  if (!msg_preview) return 0;
  return ((MsgPreviewScreen*)msg_preview)->logCount();
}

void UITask::gotoMsgHistory() {
  if (!msg_preview) return;
  ((MsgPreviewScreen*)msg_preview)->enterHistoryMode();
  setCurrScreen(msg_preview);
}

void UITask::userLedHandler() {
#ifdef PIN_STATUS_LED
  int cur_time = millis();
  if (cur_time > next_led_change) {
    if (led_state == 0) {
      led_state = 1;
      if (_msgcount > 0) {
        last_led_increment = LED_ON_MSG_MILLIS;
      } else {
        last_led_increment = LED_ON_MILLIS;
      }
      next_led_change = cur_time + last_led_increment;
    } else {
      led_state = 0;
      next_led_change = cur_time + LED_CYCLE_MILLIS - last_led_increment;
    }
    digitalWrite(PIN_STATUS_LED, led_state == LED_STATE_ON);
  }
#endif
}

void UITask::setCurrScreen(UIScreen* c) {
  curr = c;
  _next_refresh = 100;
}

/*
  hardware-agnostic pre-shutdown activity should be done here
*/
void UITask::shutdown(bool restart){

  #ifdef PIN_BUZZER
  /* note: we have a choice here -
     we can do a blocking buzzer.loop() with non-deterministic consequences
     or we can set a flag and delay the shutdown for a couple of seconds
     while a non-blocking buzzer.loop() plays out in UITask::loop()
  */
  buzzer.shutdown();
  uint32_t buzzer_timer = millis(); // fail-safe shutdown
  while (buzzer.isPlaying() && (millis() - 2500) < buzzer_timer)
    buzzer.loop();

  #endif // PIN_BUZZER

  if (restart) {
    // Accumulate uptime into the charge-cycle counter before rebooting
    if (_node_prefs != NULL) {
      _node_prefs->ui_charge_uptime_base += millis() / 1000;
      the_mesh.savePrefs();
    }
    _board->reboot();
  } else {
    _display->turnOff();
    radio_driver.powerOff();
    _board->powerOff();
  }
}

void UITask::setDisplayRotation(uint8_t r) {
  if (_display != NULL) {
    _display->setRotation(r);
  }
}

void UITask::updateMsgMaxSizes(int max_unread, int max_log) {
  if (msg_preview) {
    ((MsgPreviewScreen*)msg_preview)->setMaxSizes(max_unread, max_log);
  }
}

bool UITask::isButtonPressed() const {
#ifdef PIN_USER_BTN
  return user_btn.isPressed();
#else
  return false;
#endif
}

#if UI_HAS_JOYSTICK
// Remap directional keys to match display rotation.
// Default rotation=3 (landscape) needs no remap; each 90° step rotates keys oppositely.
// Formula: key_steps_CW = (7 - rotation) % 4
static char remapKeyForRotation(char c, uint8_t rotation) {
  static const char cw[4] = { KEY_UP, KEY_RIGHT, KEY_DOWN, KEY_LEFT };
  int dir = -1;
  for (int i = 0; i < 4; i++) {
    if (c == cw[i]) { dir = i; break; }
  }
  if (dir < 0) return c;
  int steps = (7 - rotation) % 4;
  return cw[(dir + steps) % 4];
}
#endif

void UITask::loop() {
  char c = 0;
#if UI_HAS_JOYSTICK
  // SELECT (D6): short=enter/confirm, long=back/cancel
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_CANCEL);
  }
  // LEFT (D3): page left
  ev = joystick_left.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_LEFT);
  }
  // RIGHT (D4): page right
  ev = joystick_right.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_RIGHT);
  }
  // UP (D5): navigate up / previous item
  ev = back_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_UP);
  }
  // DOWN (D7 = P0.11): navigate down / next item
  ev = joystick_down.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_DOWN);
  }
#elif defined(PIN_USER_BTN)
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_NEXT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
    c = handleDoubleClick(KEY_PREV);
  } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }
#endif
#if defined(PIN_USER_BTN_ANA)
  if (abs(millis() - _analogue_pin_read_millis) > 10) {
    ev = analog_btn.check();
    if (ev == BUTTON_EVENT_CLICK) {
      c = checkDisplayOn(KEY_NEXT);
    } else if (ev == BUTTON_EVENT_LONG_PRESS) {
      c = handleLongPress(KEY_ENTER);
    } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
      c = handleDoubleClick(KEY_PREV);
    } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
      c = handleTripleClick(KEY_SELECT);
    }
    _analogue_pin_read_millis = millis();
  }
#endif
#if defined(BACKLIGHT_BTN)
  if (millis() > next_backlight_btn_check) {
    bool touch_state = digitalRead(PIN_BUTTON2);
#if defined(DISP_BACKLIGHT)
    digitalWrite(DISP_BACKLIGHT, !touch_state);
#elif defined(EXP_PIN_BACKLIGHT)
    expander.digitalWrite(EXP_PIN_BACKLIGHT, !touch_state);
#endif
    next_backlight_btn_check = millis() + 300;
  }
#endif

#if UI_HAS_JOYSTICK
  if (c != 0 && _node_prefs)
    c = remapKeyForRotation(c, _node_prefs->ui_display_rotation);
#endif

  if (c != 0 && curr) {
    curr->handleInput(c);
    _auto_off = millis() + AUTO_OFF_MILLIS;   // extend auto-off timer
    _next_refresh = 100;  // trigger refresh
  }

  userLedHandler();

#ifdef PIN_BUZZER
  if (buzzer.isPlaying())  buzzer.loop();
#endif

  if (curr) curr->poll();

  if (_display != NULL && _display->isOn()) {
    if (millis() >= _next_refresh && curr) {
      _display->startFrame();
      int delay_millis = curr->render(*_display);
      if (millis() < _alert_expiry) {  // render alert popup
        _display->setTextSize(1);
        int y = _display->height() / 3;
        int p = _display->height() / 32;
        _display->setColor(DisplayDriver::DARK);
        _display->fillRect(p, y, _display->width() - p*2, y);
        _display->setColor(DisplayDriver::LIGHT);  // draw box border
        _display->drawRect(p, y, _display->width() - p*2, y);
        _display->drawTextCentered(_display->width() / 2, y + p*3, _alert);
        _next_refresh = _alert_expiry;   // will need refresh when alert is dismissed
      } else {
        _next_refresh = millis() + delay_millis;
      }
      if (_display->isEink() && home)
        _display->setFullRefreshSuppressed(((HomeScreen*)home)->isOnClockPage());
      _display->endFrame();
    }
#if AUTO_OFF_MILLIS > 0
    if (_clock_dim_mode == 1) {
      // OFF: no dim while on CLOCK page
      if (home && ((HomeScreen*)home)->isOnClockPage())
        _auto_off = millis() + AUTO_OFF_MILLIS;
      if (millis() > _auto_off) _display->turnOff();
    } else {
      // ON: normal auto-off
      if (millis() > _auto_off) _display->turnOff();
    }
#endif
  }

#ifdef PIN_VIBRATION
  vibration.loop();
#endif

#ifdef AUTO_SHUTDOWN_MILLIVOLTS
  if (millis() > next_batt_chck) {
    uint16_t milliVolts = getBattMilliVolts();
    if (milliVolts > 0 && milliVolts < AUTO_SHUTDOWN_MILLIVOLTS) {

      // show low battery shutdown alert
      // we should only do this for eink displays, which will persist after power loss
      #if defined(THINKNODE_M1) || defined(LILYGO_TECHO)
      if (_display != NULL) {
        _display->startFrame();
        _display->setTextSize(2);
        _display->setColor(DisplayDriver::RED);
        _display->drawTextCentered(_display->width() / 2, 20, "Low Battery.");
        _display->drawTextCentered(_display->width() / 2, 40, "Shutting Down!");
        _display->endFrame();
      }
      #endif

      shutdown();

    }
    next_batt_chck = millis() + 8000;
  }
#endif
}

char UITask::checkDisplayOn(char c) {
  if (_display != NULL) {
    if (!_display->isOn()) {
      _display->turnOn();   // turn display on and consume event
      c = 0;
    }
    _auto_off = millis() + AUTO_OFF_MILLIS;   // extend auto-off timer
    _next_refresh = 0;  // trigger refresh
  }
  return c;
}

char UITask::handleLongPress(char c) {
  if (millis() - ui_started_at < 8000) {   // long press in first 8 seconds since startup -> CLI/rescue
    the_mesh.enterCLIRescue();
    c = 0;   // consume event
  }
  return c;
}

char UITask::handleDoubleClick(char c) {
  MESH_DEBUG_PRINTLN("UITask: double click triggered");
  checkDisplayOn(c);
  return c;
}

char UITask::handleTripleClick(char c) {
  MESH_DEBUG_PRINTLN("UITask: triple click triggered");
  checkDisplayOn(c);
  toggleBuzzer();
  c = 0;
  return c;
}

bool UITask::getGPSState() {
  if (_sensors != NULL) {
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
        return !strcmp(_sensors->getSettingValue(i), "1");
      }
    }
  } 
  return false;
}

void UITask::toggleGPS() {
    if (_sensors != NULL) {
    // toggle GPS on/off
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
        if (strcmp(_sensors->getSettingValue(i), "1") == 0) {
          _sensors->setSettingValue("gps", "0");
          _node_prefs->gps_enabled = 0;
          notify(UIEventType::ack);
        } else {
          _sensors->setSettingValue("gps", "1");
          _node_prefs->gps_enabled = 1;
          notify(UIEventType::ack);
        }
        the_mesh.savePrefs();
        showAlert(_node_prefs->gps_enabled ? "GPS: Enabled" : "GPS: Disabled", 800);
        _next_refresh = 0;
        break;
      }
    }
  }
}

void UITask::toggleBuzzer() {
    // Toggle buzzer quiet mode
  #ifdef PIN_BUZZER
    if (buzzer.isQuiet()) {
      buzzer.quiet(false);
      notify(UIEventType::ack);
    } else {
      buzzer.quiet(true);
    }
    _node_prefs->buzzer_quiet = buzzer.isQuiet();
    the_mesh.savePrefs();
    showAlert(buzzer.isQuiet() ? "Buzzer: OFF" : "Buzzer: ON", 800);
    _next_refresh = 0;  // trigger refresh
  #endif
}

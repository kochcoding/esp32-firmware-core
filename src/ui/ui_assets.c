#include "ui/ui_assets.h"

const char UI_INDEX_HTML[] =
    "<!doctype html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>ESP32 Setup</title></head><body>"
    "<h2>WiFi Setup</h2>"
    "<form method='POST' action='/save'>"
    "<label>SSID</label><br><input name='ssid' maxlength='31' style='width:100%'><br><br>"
    "<label>Password</label><br><input name='pass' maxlength='63' type='password' style='width:100%'><br><br>"
    "<button type='submit'>Save</button>"
    "</form>"
    "</body></html>";

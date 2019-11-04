void setup_OTA() {
    ArduinoOTA.setHostname(cfg.p.hostname);
  
    ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else { // U_SPIFFS
        type = "filesystem";
        filesystem.end();
        // TODO: SPIFFS.end()
      }
      logger << "Start updating " << type << endl;
    });
  
    ArduinoOTA.onEnd([]() {
      logger << "OTA update finished" << endl;
    });
  
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
     ; // TODO
    });
  
    ArduinoOTA.onError([](ota_error_t error) {
      logger << "OTA update failed" << endl;
    });
  
    ArduinoOTA.begin();
    logger << "OTA ready (";
    if (p.AP_mode == WIFI_APMODE) {
      logger << WiFi.softAPIP();
    } else {
      logger << WiFi.localIP();
    }
    logger << ")" << endl;
}

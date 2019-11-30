void setup_OTA() {
    Logger << "Starting OTA service... ";
    
    ArduinoOTA.setHostname(cfg.p.hostname.c_str());
  
    ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else { // U_SPIFFS
        type = "filesystem";
        filesystem.end();
        // TODO: SPIFFS.end()
      }
      Logger << "Start updating " << type << endl;
    });
  
    ArduinoOTA.onEnd([]() {
      Logger << "OTA update finished" << endl;
    });
  
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
     ; // TODO
    });
  
    ArduinoOTA.onError([](ota_error_t error) {
      Logger << "OTA update failed" << endl;
    });
  
    ArduinoOTA.begin();
    Logger << "done (";
    if (p.AP_mode == WIFI_APMODE) {
      Logger << WiFi.softAPIP();
    } else {
      Logger << WiFi.localIP();
    }
    Logger << ")" << endl;
}

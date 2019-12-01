#ifndef config_h
#define config_h

#include <Arduino.h>
#include <SPIFFSIniFile.h>
#include "Process.h"

#define CONFIG_MAX_FILENAME_LEN 26


#define section_controller "controller"
#define controller_kp "Kp"
#define controller_tn "Tn"
#define controller_tv "Tv"
#define controller_emax "Emax"
#define controller_time "time"
#define controller_temp "temp"

#define section_wifi "wifi"
#define wifi_hostname "hostname"
#define wifi_ssid "ssid"
#define wifi_pw "pw"

#define section_system "system"
#define system_tz "tz"

class Config {
  public:

  enum WiFiEnum {WIFI_OFFLINE, WIFI_CONN, WIFI_APMODE};
  struct param {
    WiFiEnum AP_mode;
    double emax;
    double kp, tn, tv;            // controller parameter
    String ssid, pw;
    String hostname;
    int tzoffset;
  } p;

  // constructor
  Config(Process p, const char* filename);
  Config(Process p);
  
  bool read();
  bool save();

  private:
  Process _process;
  char _filename[CONFIG_MAX_FILENAME_LEN];
  mutable File _file;
  SPIFFSIniFile *_ini;

  void write(const char* section);
  void write(const char* key, const char* value);
  void write(const char* key, int value);
  void write(const char* key, double value);
  bool readDouble(const char* section, const char* key, double &value, double deflt);
  bool readInt   (const char* section, const char* key, int    &value, int    deflt);
  bool readString(const char* section, const char* key, String &value, String deflt);
  
};


#endif

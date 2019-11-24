#ifndef config_h
#define config_h

#include <Arduino.h>
#include <Streaming.h>
#include <FS.h>
#include <SPIFFSIniFile.h>

#define CONFIG_MAX_FILENAME_LEN 26


#define section_controller "controller"
#define controller_kp "Kp"
#define controller_tn "Tn"
#define controller_tv "Tv"
#define controller_emax "Emax"


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
    int set_time, act_time;       // remaining time (s)
//    double set, act, out;         // controller i/o
    double emax;
    double kp, tn, tv;            // controller parameter
    double t1;                    // filter time for act temperature
    String ssid, pw;
    String hostname;
    int tzoffset;
  } p;

  // constructor
  Config(const char* filename, Print *logger);
  Config();
  
  bool read();
  bool save();

  private:
  char _filename[CONFIG_MAX_FILENAME_LEN];
  mutable File _file;
  SPIFFSIniFile *_ini;
  Print* _logger;

  void write(const char* section);
  void write(const char* key, const char* value);
  void write(const char* key, int value);
  void write(const char* key, double value);
  bool readDouble(const char* section, const char* key, double &value, double deflt);
  bool readInt   (const char* section, const char* key, int    &value, int    deflt);
  bool readString(const char* section, const char* key, String &value, String deflt);
  
};


#endif

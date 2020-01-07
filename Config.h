#ifndef config_h
#define config_h

#include <Arduino.h>
#include <SPIFFSIniFile.h>

#define CONFIG_MAX_FILENAME_LEN 26

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
  Config();
  
  void setFilename(const char *fn);
  bool read();
  bool save();
  void readRecipes();

  private:
  void writeRecipes();
  
//  Process _process;
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

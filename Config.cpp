#include "Config.h"

//const uint8_t Config::maxFilenameLen = CONFIG_MAX_FILENAME_LEN;


// constructor
Config::Config(const char* filename, Print *logger) {
  _ini = new SPIFFSIniFile(filename);
  if (strlen(filename) <= CONFIG_MAX_FILENAME_LEN)
    strcpy(_filename, filename);
  else
    _filename[0] = '\0';
  _logger = logger;
  *_logger << "Config initialized (" << filename << ")" << endl;
}


bool Config::read() {  
  String s;
  if (!_ini->open()) {
    *_logger << "Ini-File does not exist" << endl;
    return false;
  }

  readDouble(section_controller, controller_kp, p.kp, 30.0);
  readDouble(section_controller, controller_tn, p.tn, 20.0);
  readDouble(section_controller, controller_tv, p.tv, 0.0);

  readString(section_wifi, wifi_hostname, s, String("onsenPID"));       strncpy(p.hostname, s.c_str(), sizeof(p.hostname));
  readString(section_wifi, wifi_ssid,     s, String(""));               strncpy(p.ssid, s.c_str(), sizeof(p.ssid));
  readString(section_wifi, wifi_pw,       s, String(""));               strncpy(p.pw, s.c_str(), sizeof(p.pw));

  readInt(section_system, system_tz, p.tzoffset, 0);
  
  *_logger << "Ini-File read" << endl;
  return true;
}


bool Config::readString(const char* section, const char* key, String &value, String deflt) {
  const size_t bufferLen = 80;
  char buffer[bufferLen];
  *_logger << section << ", " << key <<": ";
  if (_ini->getValue(section, key, buffer, bufferLen)) {
    value = "" + String(buffer);
    *_logger << value << endl;
    return true;
  } else {
    *_logger << "not found, using default (" << deflt << ")" << endl;
    value = deflt;
    return false;
  }
}

bool Config::readInt(const char* section, const char* key, int &value, int deflt) {
  const size_t bufferLen = 80;
  char buffer[bufferLen];
  *_logger << section << ", " << key <<": ";
  if (_ini->getValue(section, key, buffer, bufferLen)) {
    value = atoi(buffer);
    *_logger << value << endl;
    return true;
  } else {
    *_logger << "not found, using default (" << deflt << ")" << endl;
    value = deflt;
    return false;
  }
}

bool Config::readDouble(const char* section, const char* key, double &value, double deflt) {
  const size_t bufferLen = 80;
  char buffer[bufferLen];
  *_logger << section << ", " << key <<": ";
  if (_ini->getValue(section, key, buffer, bufferLen)) {
    value = atof(buffer);
    *_logger << value << endl;
    return true;
  } else {
    *_logger << "not found, using default (" << deflt << ")" << endl;
    value = deflt;
    return false;
  }
}

bool Config::save(){
  if (_file) {
      _file.close();
  }
  
  _file = SPIFFS.open(_filename, "w");
  if (!_file) {
      return false;
  }
  
  write(section_controller);
  write(controller_kp, p.kp);
  write(controller_tn, p.tn);
  write(controller_tv, p.tv);

  write(section_wifi);
  write(wifi_hostname, p.hostname);
  write(wifi_ssid, p.ssid);
  write(wifi_pw, p.pw);

  write(section_system);
  write(system_tz, p.tzoffset);
  
  _file.close();
  *_logger << "Ini-File written" << endl;
  return true;
}


void Config::write(const char* section) {
  _file.write('[');
  _file.write(section);
  _file.write(']');
  _file.write('\n');
}



void Config::write(const char* key, const char* value) {
  _file.write(key);
  _file.write(" = ");
  _file.write(value);
  _file.write('\n');
}


void Config::write(const char* key, double value) {
  write(key, String(value, 6).c_str());
}

void Config::write(const char* key, int value) {
  write(key, String(value).c_str());
}

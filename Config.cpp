#include "Config.h"
#include "Logfile.h"
#include <Streaming.h>
#include <FS.h>
#include "recipe.h"

//const uint8_t Config::maxFilenameLen = CONFIG_MAX_FILENAME_LEN;


// constructor
Config::Config(Process p, const char* filename) {
	_process = p;
	_ini = new SPIFFSIniFile(filename);
	if (strlen(filename) <= CONFIG_MAX_FILENAME_LEN)
		strcpy(_filename, filename);
	else
		_filename[0] = '\0';
	Logger << "Config initialized (" << filename << ")" << endl;
}
Config::Config(Process p) {
	_process = p;
}

bool Config::read() {  
	String s;
	if (!_ini->open()) {
		Logger << "Ini-File does not exist" << endl;
	}
	double d;
	int i;
	
	readDouble(section_controller, controller_kp, p.kp, 30.0);
	_process.setParam(Parameter::KP, p.kp);
	
	readDouble(section_controller, controller_tn, p.tn, 20.0);
	_process.setParam(Parameter::TN, p.tn);
	
	readDouble(section_controller, controller_emax, p.emax, 10.0);
	_process.setParam(Parameter::EMAX, p.emax);

	//readDouble(section_controller, controller_tv, d, 0.0);

	readDouble(section_controller, controller_temp, d, 65.0);
	_process.setParam(Parameter::TEMP, d);

	readInt(section_controller, controller_time, i, 3600);
	_process.setParam(Parameter::TIME, i);

	readString(section_wifi, wifi_hostname, p.hostname, String("onsenPID"));
	readString(section_wifi, wifi_ssid,     p.ssid, String("default_ssid"));
	readString(section_wifi, wifi_pw,       p.pw, String("default_pw"));

	readInt(section_system, system_tz, p.tzoffset, 0);
	
	Logger << "End of config file" << endl;
	return true;
}


bool Config::readString(const char* section, const char* key, String &value, String deflt) {
	const size_t bufferLen = 80;
	char buffer[bufferLen];
	Logger << "- " << section << ", " << key <<": ";
	if (_ini && _ini->getValue(section, key, buffer, bufferLen)) {
		value = String(buffer);
		Logger << "'" << value << "'" << endl;
		return true;
	} else {
		Logger << "not found, using default (" << deflt << ")" << endl;
		value = deflt;
		return false;
	}
}

bool Config::readInt(const char* section, const char* key, int &value, int deflt) {
	const size_t bufferLen = 80;
	char buffer[bufferLen];
	Logger << "- " << section << ", " << key <<": ";
	if (_ini && _ini->getValue(section, key, buffer, bufferLen)) {
		value = atoi(buffer);
		Logger << value << endl;
		return true;
	} else {
		Logger << "not found, using default (" << deflt << ")" << endl;
		value = deflt;
		return false;
	}
}

bool Config::readDouble(const char* section, const char* key, double &value, double deflt) {
	const size_t bufferLen = 80;
	char buffer[bufferLen];
	Logger << "- " << section << ", " << key <<": ";
	if (_ini && _ini->getValue(section, key, buffer, bufferLen)) {
		value = atof(buffer);
		Logger << value << endl;
		return true;
	} else {
		Logger << "not found, using default (" << deflt << ")" << endl;
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
	write(controller_emax, p.emax);

	write(section_wifi);
	write(wifi_hostname, p.hostname.c_str());
	write(wifi_ssid, p.ssid.c_str());
	write(wifi_pw, p.pw.c_str());

	write(section_system);
	write(system_tz, p.tzoffset);
	
	_file.close();
	Logger << "Ini-File written" << endl;
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

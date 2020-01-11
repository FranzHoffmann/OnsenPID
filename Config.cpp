#include "Config.h"
#include "Logfile.h"
#include <Streaming.h>
#include <FS.h>
#include "recipe.h"


// constructor
Config::Config() {
}


void Config::setFilename(const char *fn) {
	if (strlen(fn) <= CONFIG_MAX_FILENAME_LEN)
		strncpy(_filename, fn, sizeof(_filename));
	else
		strncpy(_filename, "config.ini", sizeof(_filename));
	_ini = new SPIFFSIniFile(_filename);
	Logger << "Config initialized (" << _filename << ")" << endl;
}


// Note: config file can not be updated.
// This will delete the file and write a new one.
bool Config::save(){
	if (_file) _file.close();
	_file = SPIFFS.open(_filename, "w");
	if (!_file) return false;

	writeRecipes();

	write("wifi");
	write("hostname", p.hostname.c_str());
	write("wifi", p.ssid.c_str());
	write("pw", p.pw.c_str());

	write("system");
	write("tz", p.tzoffset);
	write("btn_up", p.btn_up);
	write("btn_dn", p.btn_dn);
	write("btn_le", p.btn_le);
	write("btn_ri", p.btn_ri);
	write("btn_sel", p.btn_sel);
	write("btn_tol", p.btn_tol);
	write("pwm_port", p.pwm_port);
	
	_file.close();
	Logger << "Ini-File written" << endl;
	return true;
}


bool Config::read() {  
	if (!_ini->open()) {
		Logger << "Ini-File does not exist" << endl;
	}

	readRecipes();

	readString("wifi", "hostname", p.hostname, String("onsenPID"));
	readString("wifi", "wifi",     p.ssid, String("default_ssid"));
	readString("wifi", "pw",       p.pw, String("default_pw"));

	readInt("system", "tz", p.tzoffset, 0);
	readInt("system", "btn_up", p.btn_up, 200);
	readInt("system", "btn_dn", p.btn_dn, 460);
	readInt("system", "btn_le", p.btn_le, 690);
	readInt("system", "btn_ri", p.btn_ri, 10);
	readInt("system", "btn_sel", p.btn_sel, 1010);
	readInt("system", "btn_tol", p.btn_tol, 10);
	readInt("system", "pwm_port", p.pwm_port, 4);

	Logger << "End of config file" << endl;
	return true;
}


void Config::readRecipes() {
	String section, key;
	String strValue;
	for (int i=0; i<REC_COUNT; i++) {
		section = "Recipe" + String(i);
		readString(section.c_str(), "name", strValue, section.c_str());
		strncpy(recipe[i].name, strValue.c_str(), sizeof(recipe[i].name));
		// debug Serial << strValue << "(" << recipe[i].name << ")" << endl;
		// read times and temps arrays
		for (int j=0; j<REC_STEPS; j++) {
			key = "time" + String(j);
			readInt(section.c_str(), key.c_str(), recipe[i].times[j], 0);
			key = "temp" + String(j);
			readDouble(section.c_str(), key.c_str(), recipe[i].temps[j], 0.0);
			yield();
		}
		// read params
		for (int j=0; j<REC_PARAM_COUNT; j++) {
			key = pararray[j].id;
			readDouble(section.c_str(), key.c_str(), recipe[i].param[j], 0.0);
			yield();
		}
	}
}


// Note: only the whole file can be written at once.
// Don't call this, except from save()
void Config::writeRecipes() {
	String section, key;
	String strValue;
	for (int i=0; i<REC_COUNT; i++) {
		section = "Recipe" + String(i);
		write(section.c_str());
		write("name", recipe[i].name);
		// write times and temps arrays
		for (int j=0; j<REC_STEPS; j++) {
			key = "time" + String(j);
			write(key.c_str(), recipe[i].times[j]);
			key = "temp" + String(j);
			write(key.c_str(), recipe[i].temps[j]);
			yield();
		}
		// write params
		for (int j=0; j<REC_PARAM_COUNT; j++) {
			key = pararray[j].id;
			write(key.c_str(), recipe[i].param[j]);
			yield();
		}
	}
}


bool Config::readString(const char* section, const char* key, String &value, String deflt) {
	const size_t bufferLen = 80;
	char buffer[bufferLen];
	Logger << "- " << section << ", " << key <<": ";
	if (_ini && _ini->getValue(section, key, buffer, bufferLen)) {
		value = String(buffer);
		value.trim();
		if (value.length() == 0) value = deflt;
		// debug Logger << "'" << value << "'" << endl;
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
		// debug Logger << value << endl;
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
		// debug Logger << value << endl;
		return true;
	} else {
		Logger << "not found, using default (" << deflt << ")" << endl;
		value = deflt;
		return false;
	}
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

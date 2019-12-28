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
	if (!_ini->open()) {
		Logger << "Ini-File does not exist" << endl;
	}

	readRecipes();

	readString(section_wifi, wifi_hostname, p.hostname, String("onsenPID"));
	readString(section_wifi, wifi_ssid,     p.ssid, String("default_ssid"));
	readString(section_wifi, wifi_pw,       p.pw, String("default_pw"));

	readInt(section_system, system_tz, p.tzoffset, 0);
	
	Logger << "End of config file" << endl;
	return true;
}

void Config::readRecipes() {
	String section, key;
	String strValue;
	for (int i=0; i<REC_COUNT; i++) {
		section = "Recipe" + i;
		readString(section.c_str(), "name", strValue, section.c_str());
		strncpy(recipe[i].name, strValue.c_str(), sizeof(recipe[i].name));
		// read times and temps arrays
		for (int j=0; j<REC_STEPS; j++) {
			key = "time" + j;
			readInt(section.c_str(), key.c_str(), recipe[i].times[j], 0);
			key = "temp" + j;
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
		section = "Recipe" + i;
		write(section.c_str());
		write("name", recipe[i].name);
		// write times and temps arrays
		for (int j=0; j<REC_STEPS; j++) {
			key = "time" + j;
			write(key.c_str(), recipe[i].times[j]);
			key = "temp" + j;
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

	writeRecipes();

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

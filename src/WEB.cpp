#include <WEB.h>
#include <Stream.h>
#include <PString.h>
#include <recipe.h>
#include <Process.h>
#include <LittleFS.h>
#include <Clock.h>
#include <DataLogger.h>
#include <util.h>
#include <Global.h>

ESP8266WebServer server(80);

// ----------------------------------------------------------------------------- web server
String getContentType(String filename) {
	if (server.hasArg("download"))    return "application/octet-stream";
	if (filename.endsWith(".htm"))    return "text/html";
	if (filename.endsWith(".html"))   return "text/html";
	if (filename.endsWith(".css"))    return "text/css";
	if (filename.endsWith(".js"))     return "application/javascript";
	if (filename.endsWith(".png"))    return "image/png";
	if (filename.endsWith(".gif"))    return "image/gif";
	if (filename.endsWith(".jpg"))    return "image/jpeg";
	if (filename.endsWith(".ico"))    return "image/x-icon";
	if (filename.endsWith(".xml"))    return "text/xml";
	if (filename.endsWith(".pdf"))    return "application/x-pdf";
	if (filename.endsWith(".zip"))    return "application/x-zip";
	if (filename.endsWith(".gz"))     return "application/x-gzip";
	return "text/plain";
}


void fail(String msg) {
	server.send(500, "text/plain", msg + "\r\n");
}
void redirect(String url) {
	server.sendHeader("Location", url, true);
	server.send(302, "text/plain", "");
}


/**
 * split a string, e.g.:
 * spliString("foo:bar:baz", ':', 0) --> "foo"
 **/
String splitString(String s, char seperator, int column) {
	int actCol = 0;
	String result = "";
	for (unsigned int i=0; i<s.length(); i++) {
		char c = s.charAt(i);
		if (c != seperator) result += c;
		if ((c == seperator) || i == s.length()-1) {
			if (actCol == column) {
				return result;
			} else {
				result = "";
				actCol++;
			}
		}
	}
	return String("");
}

String directory() {
	Dir dir = LittleFS.openDir("/");
	String s = "<ul>";
	while (dir.next()) {
		String fileName = dir.fileName();
		size_t fileSize = dir.fileSize();
		s += "<li><a href=\"" + fileName + "\">" + fileName + "</a> (" + String(fileSize) + " Bytes)";
		s += "(<a href=\"/delete?fn=" + fileName + "\">delete</a>)</li>";
	}
	s += "</ul>";
	return s;
}

/**
 * replace some keywords by values.
 * keywords can contain up to two parameters, like this: "FOO:1:3"
 */
String subst(String var, int par) {
	String code = splitString(var, ':', 0);
	int j = splitString(var, ':', 1).toInt();
	// unused? int k = splitString(var, ':', 2).toInt();

	if (code == "#")	return String(par);

	if (code == "ACT") return String(p.act);
	if (code == "SET") return String(sm.out.set);
	if (code == "OUT") return String(p.out);

	if (code == "NAME#") 	return String(recipe[j].name);
	if (code == "NAME") 	return String(recipe[par].name);
	if (code == "TEMP")		return String(recipe[par].temps[j]);
	if (code == "TIME")		return String(recipe[par].times[j] / 60);
	if (code == "KP") 		return String(recipe[par].param[(int)Parameter::KP]);
	if (code == "TN") 		return String(recipe[par].param[(int)Parameter::TN]);
	if (code == "TV") 		return String(recipe[par].param[(int)Parameter::TV]);
	if (code == "EMAX") 	return String(recipe[par].param[(int)Parameter::EMAX]);
	if (code == "PMAX") 	return String(recipe[par].param[(int)Parameter::PMAX]);

	if (code == "TIME_LEFT") return String(sm.getRemainingTime());	// seconds
	if (code == "STARTTIME") return secToTime(0);// TODO FIXME starttime);

	if (code == "STATE")	return sm.stateAsString(sm.getState());
	if (code == "HOSTNAME")	return String(cfg.p.hostname);
	if (code == "VERSION")	return String("OnsenPID ") + VERSION;
	
	if (code == "SSID") return cfg.p.ssid;
	if (code == "PW") return cfg.p.pw;
	if (code == "TZ") return String(cfg.p.tzoffset);
	if (code == "DIR") return directory();
	if (code == "LAST_REC") return ""; // TODO FIXME get_selected_recipe_no() == j ? "selected" : "";
	return var; // ignore unknown strings
}


/*
 *  read a file, optionally substitute variable like {{ACT}} and {{SET}}, and send to web server
 *  everything enclosed in {{ }} is treated as a variable and sent to substitute()
 *  use escape character '\' in file to output {{: \{{
 */
String readAndSubstitute(File f, int par) {
	String var, content;
	enum {TEXT, ESCAPED, VAR_START, VAR, VAR_END} state;
	state = TEXT;
	while (f.available()) {
		char c = f.read();
		switch(state) {
			case TEXT:
				if (c ==  '\\')    state = ESCAPED;
				else if (c == '{') state = VAR_START;
				else content += c;
				break;

			case ESCAPED:
				content += c;
				state = TEXT;
				break;

			case VAR_START:
				if (c == '{') {           // found {{
					state = VAR;
					var = "";
				} else {                  // found {x
					content += '{';
					content += c;
					state = TEXT;
				}
				break;

			case VAR:
				if (c == '}') {         // found }
					state = VAR_END;
				} else {
					var += c;
				}
				break;

			case VAR_END:
				if (c == '}') {            // found }}
					content += subst(var, par);
					state = TEXT;
				} else {                  // found }x inside variable
					var += '}';
					var += c;
				}
				break;
		}
	}
	return content;
}
String readAndSubstitute(File f) { return readAndSubstitute(f, 0); }

void changeParam(String arg_name, String param_name, double *param) {
	String str_param = "Parameter";
	String str_from  = "geändert von";
	String str_to    = "auf";
	if (server.hasArg(arg_name)) {
			double val = server.arg(arg_name).toDouble();
			Logger << str_param << ' ' << param_name << ' ' << str_from << ' ' << *param << ' ' << str_to << ' ' << val << endl;
			*param = val;
	}
}


void changeParam(String arg_name, String param_name, String &param) {
	String str_param = "Parameter";
	String str_from  = "geändert von";
	String str_to    = "auf";
	if (server.hasArg(arg_name)) {
			String val = server.arg(arg_name);
			Logger << str_param << ' ' << param_name << ' ' << str_from << ' ' << param << ' ' << str_to << ' ' << val << endl;
			param = val;
	}
}


void changeParam(String arg_name, String param_name, int *param) {
	String str_param = "Parameter";
	String str_from  = "geändert von";
	String str_to    = "auf";
	if (server.hasArg(arg_name)) {
			int val = server.arg(arg_name).toInt();
			Logger << str_param << ' ' << param_name << ' ' << str_from << ' ' << *param << ' ' << str_to << ' ' << val << endl;
			*param = val;
	}
}

// ---------------------------------------------------------------------------------------- send file
/**
 * send file with substitution of {{variables}}
 * only works with small files.
 * TODO: make this streaming...
 */
void send_file(String filename, int par) {
	File f = LittleFS.open(filename, "r");
	if (!f) {
		Logger << "WEB: send_file(" << filename << "): Datei nicht gefunden." << endl;
		fail("file not found: " + filename);
		return;
	}
	String reply = readAndSubstitute(f, par);
	server.send(200, getContentType(filename), reply);
	f.close();
}
void send_file(String filename) { send_file(filename, 0); }

/**
 * send file (as-is, no substitutions)
 * this also works with huge files.
 */
bool stream_file(String filename) {
	String content_type = getContentType(filename);
	String filename_gz = filename + ".gz";
	String fn;
	if (LittleFS.exists(filename_gz)) {
		fn = filename_gz;
	} else if (LittleFS.exists(filename)) {
		fn = filename;
	} else {
			Logger << "WEB: stream_file(" << filename << "): Datei nicht gefunden" << endl;
			return false;
	}
  Logger << "WEB: stream_file(" << fn << ')' << endl;
	File f = LittleFS.open(fn, "r");
	if (!f) {
		return false;
	}
	server.streamFile(f, content_type);
	f.close();
	return true;
}

void logAccess() {
	Logger << "WEB: request '" << server.uri() << "'";
	for(int i=0; i<server.args(); i++) {
		Logger << ", " << server.argName(i) << "=" << server.arg(i);
	}
	Logger << endl;
}

// --------------------------------------------------------------------------------------------- not found
// this is called when the URL is not one of the special URLs defined above.
// 1) if the URL refers to a file, stream it
// 2) 404
void handleNotFound() {
	logAccess();
	String filename = server.uri();
	if (stream_file(filename)) {
		return;
	}
	Logger << "WEB: URL nicht gefunden: " << server.uri() << endl;
	String message = "File Not Found\n\n";
	message += "URI: " + server.uri();
	message += "\nMethod: " + (server.method() == HTTP_GET) ? "GET" : "POST";
	message += "\nArguments: " + server.args();
	message += "\n";
	for (uint8_t i = 0; i < server.args(); i++) {
		message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
	}
	server.send(404, "text/plain", message);
}


// --------------------------------------------------------------------------------------------- root
void handleRoot() {
	logAccess();
	if (server.hasArg("rst")) {
		ESP.restart();
	}
	send_file("/index.html");
}

// --------------------------------------------------------------------------------------------- Ajax (and commands)
void handleAjax() {
	//logAccess();
	send_file("/ajax.json");
}


// --------------------------------------------------------------------------------------------- WiFi
/*
{"result":7, "list":[
{"ssid":"Netz 1", "Ch":6, "encryption":"foo", "RSSI":32}
,{"ssid":"Netz 2", "Ch":13, "encryption":"offen", "RSSI":74}
]}
*/
void handleWifi()   {
	logAccess();
	if (server.hasArg("scan")) {
		// TODO: make this async
		Logger << "WiFi-Scan gestartet" << endl;
		int n = WiFi.scanNetworks();
		Logger << "WiFi-Scan abgeschlossen (" << n << " Netze gefunden)" << endl;
		server.setContentLength(CONTENT_LENGTH_UNKNOWN);
		server.send(200, "text/json", String(""));
		server.sendContent("{\"result\":" + String(n) + ", \"list\":[\n");
		for (int i=0; i<n; i++) {
			String reply;
			if (i>0) {
				reply = ",\n";
			} else {
				reply = "";
			}
			reply += "{\"ssid\":\"" + WiFi.SSID(i) + "\""
				+ ", \"Ch\":" + String(WiFi.channel(i))
				+ ", \"encryption\":" + (WiFi.encryptionType(i) == ENC_TYPE_NONE ? "\"open\"" : "\"encrypted\"")
				+ ", \"RSSI\":" + String(WiFi.RSSI(i))
				+ "}";
			if (i == n-1) reply += "]}";
			server.sendContent(reply);
		}
		WiFi.scanDelete();

	} else {
		if (server.hasArg("save")) {
			changeParam("ssid", "SSID", cfg.p.ssid);
			changeParam("pwd", "Password", cfg.p.pw);
			changeParam("hn", "Hostname", cfg.p.hostname);
			cfg.save();
		}
		send_file("/wifi.html");
	}
}


// --------------------------------------------------------------------------------------------- recipe
void handleStart() {
	logAccess();
	if (!server.hasArg("start")) {
		send_file("/start.html");
		return;
	}

	int rec = 0;
	if (server.hasArg("rec")) {
		String s = server.arg("rec");
		rec = s.toInt();
		set_selected_recipe_no(rec);
	} else {
		send_file("/start.html");
		return;
	}

	// "time" is String in format "08:09".
	// Convert to 0-86400 seconds
	String t = server.arg("time");
	long time = t.substring(0, 2).toInt() * 3600 + t.substring(3).toInt() * 60;
	Logger << "Startbefehl über Web (Rezept: " << rec;

	String m = server.arg("mode");
	if (m == "st") {
		Logger << ", Startzeit: " << t << ")" << endl;
		sm.startByStartTime(rec, time);
	} else 
	if (m == "et") {
		Logger << ", Endzeit: " << t << ")" << endl;
		sm.startByEndTime(rec, time);
	} else {
		Logger << ")" << endl;
		sm.startCooking(rec);
	}
	redirect("/");
}


// --------------------------------------------------------------------------------------------- recipe
void handleRecipe() {
	logAccess();
	int rec = 0;
	if (server.hasArg("rec")) {
		rec = server.arg("rec").toInt();
		rec = limit(rec, 0, REC_COUNT-1);
	}
	
	if (server.hasArg("save")) {
		String arg, val;
		for (int i=0; i<server.args(); i++) {
			arg = server.argName(i);
			val = server.arg(i);
			Serial << arg << " = " << val << endl;

			if (arg == "name") {
				strncpy(recipe[rec].name, val.c_str(), sizeof(recipe[rec].name));
				continue;
			}
			
			// temperatures
			for (int j=0; j<REC_STEPS; j++) {
				String keyword = String("temp") + String(j);
				if (arg == keyword) {
					recipe[rec].temps[j] = val.toDouble();
					break;
				}
			}
			
			// times
			for (int j=0; j<REC_STEPS; j++) {
				String keyword = String("time") + String(j);
				if (arg == keyword) {
					recipe[rec].times[j] = val.toInt()*60;
					break;
				}
			}
			
			// parameters
			for (int j=0; j<REC_PARAM_COUNT; j++) {
				String keyword = String("par") + String(j);
				if (arg == keyword) {
					recipe[rec].param[j] = val.toDouble();
					break;
				}
			}
		}
		cfg.save();
	}
	send_file("/rec.html", rec);
}


// --------------------------------------------------------------------------------------------- data

void sendJsonData(unsigned long t) {
	dl_rewind("TODO", t);
	if (!dl_hasMore()) {
		server.send(200, "text/json", "{\"data\":[]}");
		return;
	}
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "text/json", String(""));
	server.sendContent("{\"data\":[");

	uint32_t count = 0;
	char buf[100];
	PString pstr(buf, sizeof(buf));
	char Q = '"';
	while (dl_hasMore() && count < 1000) {
		if (count > 0) server.sendContent(",");
		dl_data_t data = dl_getNext();
		pstr = "{";
		pstr << Q << "t"   << Q << ":" << data.ts  << ", ";
		pstr << Q << "act" << Q << ":" << data.act << ", ";
		pstr << Q << "set" << Q << ":" << data.set << ", ";
		pstr << Q << "out" << Q << ":" << data.out;
		pstr << "}" << endl;
		server.sendContent(buf);
		count++;
		yield();
	}
	server.sendContent("]}");
}


void sendCSVData() {
	Logger << "WEB request: CSV data" << endl;

	uint32_t count = 0;
	dl_rewind("TODO", 0);
	if (!dl_hasMore()) {
		server.send(200, "text/csv", "");
		return;
	}
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.sendHeader("Content-Disposition", "inline; filename=\"data.csv\"", true);
	server.send(200, "text/csv", "time,setpoint,temperature,power\n");

	char buf[100];
	PString pstr(buf, sizeof(buf));
	while (dl_hasMore()) {
		dl_data_t data = dl_getNext();
		pstr = "";
		pstr << data.ts;
		pstr << "," << data.set;
		pstr << "," << data.act;
		pstr << "," << data.out << "\n";
		server.sendContent(buf);
		count++;
		yield();
	}
	server.sendContent("]}");
	Logger << "answer: " << count << " data points" << endl;
}


void handleData()   {
	logAccess();
	if (server.hasArg("t")) {
		unsigned long t = strtoul(server.arg("t").c_str(), NULL, 10);
		sendJsonData(t);
		return;
	}
	if (server.hasArg("csv")) {
		sendCSVData();
		return;
	}
	send_file("/data.html");
}


int indexof(char c, char *s) {
		for (int i=0; s[i]!='\0'; i++) {
			if (s[i] == ';') {
				return i;
			}
		}
	return -1;
}


// --------------------------------------------------------------------------------------------- log
void handleLog() {
	logAccess();
	Serial << " Free Heap: " << ESP.getFreeHeap() << endl;
	if (!server.hasArg("t")) {
		// send html page
		send_file("/log.html");
		return;
	}

	// else send data
	String s;
	bool first = true;
	unsigned long t = strtoul(server.arg("t").c_str(), NULL, 10);

	Serial << millis() << " - checkpoint 1" << endl;
	Logger.rewind(t);
	if (!Logger.hasMore()) {
		server.send(200, "text/json", "{\"log\":[]}");
		Serial << millis() << " - checkpoint 2" << endl;
		return;
	}

	Serial << millis() << " - checkpoint 3" << endl;

	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "text/json", String(""));
	server.sendContent("{\"log\":[");


	// TODO: this seems to be an endless loop sometimes?
	// workaround: timeout
	unsigned long timeout = millis() + 1000;

	while (Logger.hasMore() && millis() < timeout) {
		Serial << millis() << " - checkpoint 4" << endl;

		if (!first) server.sendContent(",");
		first = false;

		String entry_str = Logger.getNext();
		if (entry_str.length() > 0) {
			LogfileT::LogEntryStruct entry = Logger.strToEntry(entry_str);
			String s = "{\"ts\":" + String(entry.timestamp) + ", \"msg\":\"" + String(entry.message) + "\"}\r";
			server.sendContent(s);
			delay(1);
		}
	}
	Serial << "checkpoint 5" << endl;

	server.sendContent("]}");
	Serial << " Free Heap: " << ESP.getFreeHeap() << endl;
}

// --------------------------------------------------------------------------------------------- config
void handleConfig() {
	logAccess();
	if (server.hasArg("save")) {
		// Zeitzone fest Berlin
		//changeParam("tz", "UTC Offset", &(cfg.p.tzoffset));
		//Clock.setTimeOffset(cfg.p.tzoffset * 3600);
		//cfg.save();
	}
	send_file("/cfg.html");
}

// --------------------------------------------------------------------------------------------- File upload
File uploadFile;
void handleFileUpload() {
	logAccess();
	if (server.uri() != "/edit") {
		return;
	}
	HTTPUpload& upload = server.upload();
	String fn = "/" + upload.filename;

	if (upload.status == UPLOAD_FILE_START) {
		if (uploadFile) {
			uploadFile.close();
		}
		if (LittleFS.exists(fn)) {
			LittleFS.remove(fn);
		}
		uploadFile = LittleFS.open(fn, "w");
		Logger << "WEB upload gestartet, Datei: " << fn << endl;
		if (!uploadFile) {
			Logger << "Fehler: konnte Datei nicht öffnen" << endl;
		}
	}

	else if (upload.status == UPLOAD_FILE_WRITE) {
		if (uploadFile) {
			uploadFile.write(upload.buf, upload.currentSize);
			Logger << "WEB upload: " << upload.currentSize << " Bytes empfangen" << endl;
		} else {
			Logger << "WEB upload: konnte nicht schreiben" << endl;
		}
	}

	else if (upload.status == UPLOAD_FILE_END) {
		if (uploadFile) {
			uploadFile.close();
		}
		Logger << "WEN upload beednet, " << upload.totalSize << " Bytes" << endl;
	}
}

void handleFileDelete() {
	logAccess();
	if (server.hasArg("fn")) {
		String fn = server.arg("fn");
		Logger << "WEB: Datei '" << fn << "' gelöscht" << endl;
		if (!LittleFS.remove(fn)) {
			Logger << "Datei löschen ist fehlgeschlagen" << endl;
		}
	}
	redirect("/cfg");
}

void setup_webserver(ESP8266WebServer *s) {
	s->on("/",     handleRoot);
	s->on("/start",handleStart);
	s->on("/rec",  handleRecipe);
	s->on("/data", handleData);
	s->on("/cfg",  handleConfig);
	s->on("/wifi", handleWifi);
	s->on("/log",  handleLog);
	s->on("/s",    handleAjax);
	s->on("/edit", HTTP_POST, []() {redirect("/cfg");}, handleFileUpload);
	s->on("/delete",  handleFileDelete);

	server.onNotFound(handleNotFound);
	s->begin();
}
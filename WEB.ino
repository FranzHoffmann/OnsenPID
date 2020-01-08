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
	for (int i=0; i<s.length(); i++) {
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


/**
 * replace some keywords by values.
 * keywords can contain up to two parameters, like this: "FOO:1:3"
 */
String subst(String var, int par) {
	String code = splitString(var, ':', 0);
	int j = splitString(var, ':', 1).toInt();
	int k = splitString(var, ':', 2).toInt();

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
	
	if (code == "CLOCK") return String(Clock.getEpochTime());//timeClient.getFormattedTime());
	if (code == "TIME_LEFT") return String(sm.getRemainingTime() / 60);

	if (code == "STATE")	return sm.stateAsString(sm.getState());
	if (code == "HOSTNAME")	return String(cfg.p.hostname);
	if (code == "VERSION")	return String("OnsenPID ") + VERSION;
	
	if (code == "SSID") return cfg.p.ssid;
	if (code == "PW") return cfg.p.pw;
	if (code == "TZ") return String(cfg.p.tzoffset);
	if (code == "DIR") return directory();
	return var; // ignore unknown strings
}


String directory() {
	Dir dir = filesystem.openDir("/");
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

/*
 *  read a file, optionally substitute variable like {{ACT}} and {{SET}}, and send to web server
 *  everything enclosed in {{ }} is treated as a variable and sent to substitute()
 *  use escape character '\' in file to output {{: \{{
 */
String readAndSubstitute(File f) { return readAndSubstitute(f, 0); }
String readAndSubstitute(File f, int par) {
	String var, content;
	enum {TEXT, ESCAPED, VAR1, VAR_START, VAR, VAR_END} state;
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
					// debug Serial << "{{" << var << "}}" << endl;
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
void send_file(String filename) { send_file(filename, 0); }
void send_file(String filename, int par) {
	// debug Logger << "send_file(): " << filename << endl;
	File f = filesystem.open(filename, "r");
	if (!f) {
		Logger << "send_file(): file not found: " << filename << endl;
		fail("file not found: " + filename);
		return;
	}
	String reply = readAndSubstitute(f, par);
	server.send(200, getContentType(filename), reply);
	f.close();
}


/**
 * send file (as-is, no substitutions)
 * this also works with huge files.
 */
bool stream_file(String filename) {
	String content_type = getContentType(filename);
	String filename_gz = filename + ".gz";
	String fn;
	if (filesystem.exists(filename_gz)) {
		fn = filename_gz;
	} else if (filesystem.exists(filename)) {
		fn = filename;
	} else {
			Logger << "stream_file(): could not open file" << filename << endl;
			return false;
	}
	File f = filesystem.open(fn, "r");
	if (!f) {
		return false;
	}
	server.streamFile(f, content_type);
	f.close();
	return true;
}

// --------------------------------------------------------------------------------------------- not found
// this is called when the URL is not one of the special URLs defined above.
// 1) if the URL refers to a file, stream it
// 2) 404
void handleNotFound() {
	String filename = server.uri();
	if (stream_file(filename)) {
		return;
	}
	Logger << "URL nicht gefunden: " << server.uri() << endl;
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
	if (server.hasArg("rst")) {
		ESP.restart();
	}
	send_file("/index.html");
}

// --------------------------------------------------------------------------------------------- Ajax (and commands)
void handleAjax() {
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
	if (!server.hasArg("start")) {
		send_file("/start.html");
		return;
	}

	String t = server.arg("time");
	unsigned long epoch = Clock.getEpochTime();
	unsigned long midnight = epoch - (epoch % 86400L) - 3600 * cfg.p.tzoffset;
	int hrs = t.substring(0, 2).toInt();
	int mins = t.substring(3).toInt();
	unsigned long timearg = midnight + 60 * mins + 3600 * hrs;
	while (timearg < epoch) timearg += 86400;
	Logger << "time: " << hrs << ":" << mins << ", timearg: " << timearg << endl;

	int rec = 0;
	if (server.hasArg("rec")) {
		String s = server.arg("rec");
		rec = s.toInt();
	} else {
		send_file("/start.html");
		return;
	}

	String m = server.arg("mode");
	if (m == "nw") {
		Logger << "Web: Kochen sofort" << endl;
		sm.startCooking(rec);
	} else
	if (m == "st") {
		sm.startByStartTime(rec, timearg);
		Logger << "Web: Kochen später (Start um " << hrs << ":" << mins << ", " << timearg << ")" << endl;
	} else
	if (m == "et") {
		Logger << "Web: Kochen später (Fertig um " << hrs << ":" << mins << ", " << timearg << ")" << endl;
		sm.startByEndTime(rec, timearg);
	} else {
		Logger << "Web: unverständlichen Startbefehl ignoriert" << endl;
	}
	redirect("/");
}


// --------------------------------------------------------------------------------------------- recipe
void handleRecipe() {
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
void handleData()   {
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

void sendJsonData(unsigned long t) {
	bool first = true;

	//Logger << "request: json data since " << t << endl;
	dl_rewind("TODO", t);
	if (!dl_hasMore()) {
		Logger << "answer: nothing" << endl;
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
	while (dl_hasMore() && count < 30000) {
		if (!first) server.sendContent(",");
		first = false;
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
	if (count < 300) {
		server.sendContent("], \"more\":0}");
	} else {
		server.sendContent("], \"more\":1}");
	}
	//Logger << "answer: " << count << " data points" << endl;
}


void sendCSVData() {
	Logger << "request: CSV data" << endl;

	uint32_t count = 0;
	dl_rewind("TODO", 0);
	if (!dl_hasMore()) {
		Logger << "answer: nothing" << endl;
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
	if (!server.hasArg("t")) {
		// send html page
		send_file("/log.html");
		return;
	}

	// else send data
	String s;
	bool first = true;
	unsigned long t = strtoul(server.arg("t").c_str(), NULL, 10);

	Logger.rewind(t);
	if (!Logger.hasMore()) {
		server.send(200, "text/json", "{\"log\":[]}");
		return;
	}

	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "text/json", String(""));
	server.sendContent("{\"log\":[");

	while (Logger.hasMore()) {
		if (!first) server.sendContent(",");
		first = false;

		String entry_str = Logger.getNext();
		if (entry_str.length() > 0) {
			LogfileT::LogEntryStruct entry = Logger.strToEntry(entry_str);
			String s = "{\"ts\":" + String(entry.timestamp) + ", \"msg\":\"" + String(entry.message) + "\"}\r";
			server.sendContent(s);
		}
	}
	server.sendContent("]}");
}

// --------------------------------------------------------------------------------------------- config
void handleConfig() {
	if (server.hasArg("save")) {
		changeParam("kp", "Verstärkung", &(cfg.p.kp));
		changeParam("tn", "Nachstellzeit", &(cfg.p.tn));
		changeParam("tv", "Vorhaltezeit", &(cfg.p.tv));
		changeParam("emax", "Max. Regelabweichung", &(cfg.p.emax));
		changeParam("tz", "UTC Offset", &(cfg.p.tzoffset));
		cfg.save();
	}
	send_file("/cfg.html");
}

// --------------------------------------------------------------------------------------------- File upload
File uploadFile;
void handleFileUpload() {
	if (server.uri() != "/edit") {
		return;
	}
	HTTPUpload& upload = server.upload();
	String fn = "/" + upload.filename;

	if (upload.status == UPLOAD_FILE_START) {
		if (uploadFile) {
			uploadFile.close();
		}
		if (filesystem.exists(fn)) {
			filesystem.remove(fn);
		}
		uploadFile = filesystem.open(fn, "w");
		Logger << "Upload: START, filename: " << fn << endl;
		if (!uploadFile) {
			Logger << "Warning: could not open file" << endl;
		}
	}

	else if (upload.status == UPLOAD_FILE_WRITE) {
		if (uploadFile) {
			uploadFile.write(upload.buf, upload.currentSize);
			Logger << "Upload: WRITE, Bytes: " << upload.currentSize << endl;
		} else {
			Logger << "Upload: WRITE (failed)" << endl;
		}
	}

	else if (upload.status == UPLOAD_FILE_END) {
		if (uploadFile) {
			uploadFile.close();
		}
		Logger << "Upload: END, Size: " << upload.totalSize << endl;
	}
}

void handleFileDelete() {
	if (server.hasArg("fn")) {
		String fn = server.arg("fn");
		Logger << "deleting file '" << fn << "'" << endl;
		if (!filesystem.remove(fn)) {
			Logger << "file delete failed" << endl;
		}
	}
	redirect("/cfg");
}

void setup_webserver(ESP8266WebServer *s) {
  s->on("/",     handleRoot);
  s->on("/rec",  handleRecipe);
  s->on("/data", handleData);
  s->on("/cfg",  handleConfig);
  s->on("/wifi", handleWifi);
  s->on("/log",  handleLog);
  s->on("/s",    handleAjax);
  s->on("/f",    handleFile);
  s->on("/favicon.ico",    handleFavicon);

  s->on("/edit", HTTP_POST, []() {returnOK();}, handleFileUpload);

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
void returnOK() {
  server.send(200, "text/plain", "");
}


String subst(String var) {
  String s;
  if (var == "ACT") return String(p.act);
  if (var == "SET") return String(p.set);
  if (var == "OUT") return String(p.out);
  if (var == "KP") return String(p.kp);
  if (var == "TN") return String(p.tn);
  if (var == "TV") return String(p.tv);
  if (var == "TIME_LEFT") return String(p.set_time - p.act_time);
  if (var == "TIME_SET") return String(p.set_time);
  if (var == "HOSTNAME") return HOSTNAME;
  if (var == "SSID") return p.ssid;
  
  if (var == "CSS")  {
    String s; s.reserve(1200);
    readFile("/stylesheet.css", s, false);
    return s;
  }
  
  return var; // ignore unknown strings
}

/* 
 *  read a file, optionally substitute variable like {{ACT}} and {{SET}}, and send to web server
 *  everything enclosed in {{ }} is treated as a variable and sent to substitute()
 *  use escape character '\' in file to output {{: \{\{
 */
bool readFile(const String &filename, String &content, bool substitute) {
  if (!filesystem.exists(filename)) {
    logger << "readFile(): file not found: " << filename << endl;
    return false;
  }
  File f = filesystem.open(filename, "r");
  if (!f) {
    logger << "readFile(): could not open file" << filename << endl;
    return false;
  }  
  String var;
  enum {TEXT, ESCAPED, VAR1, VAR_START, VAR, VAR_END} state;
  state = TEXT;
  while (f.available()) {
    char c = f.read();
    if (!substitute) {
      content += c;
      
    } else {
      switch(state) {
  
        case TEXT:
          switch (c) {
            case '\\':
              state = ESCAPED;
              break;
            case '{':
              state = VAR_START;
              break;
            default:
              content += c;
          }
          break;
  
        case ESCAPED:
          content += c;
          state = TEXT;
          break;
  
        case VAR_START:                      // first { found
          if (c == '{') {
            state = VAR;
            var = "";
          } else {
            content += '{';
            content += c;
            state = TEXT;
          }
          break;
  
        case VAR:                    // both {{ found
          if (c == '}') {
            state = VAR_END;
          } else {
            var += c;
          }
          break;
  
        case VAR_END:
          if (c == '}') {            // both }} found
            content += subst(var);
            state = TEXT;
          } else {
            // should not happen
            var += '}';
            var += c;
          }
          break;
      }
    }
  } 
  f.close();
  return true;
}

// ---------------------------------------------------------------------------------------- send file
/**
 * send file with substitution of {{variables}}
 * only works with small files.
 * TODO: make thsi streaming...
 */
void send_file(String filename) {
  // logger << "send_file(): " << filename << endl;
  String reply; reply.reserve(8192);
  if (!readFile(filename, reply, true)) {
    return fail("file not found: " + filename);
  }
  server.send(200, getContentType(filename), reply);
}


/**
 * send file (as-is, no substitutions)
 * this also works with huge files.
 */
void stream_file(String filename) {
    File f = filesystem.open(filename, "r");
    if (!f) {
      logger << "stream_file(): could not open file" << filename << endl;
      return;
    }  
    server.streamFile(f, getContentType(filename));
    f.close();  
}


/**
 * send any file in a query like /f&f=filename.txt
 * very unsafe, but very convenient
 */
void handleFile()   {
  if (server.hasArg("f")) {
    String filename = server.arg("f");
    stream_file(filename);
  }
}


// --------------------------------------------------------------------------------------------- not found
void handleNotFound() {
  logger << "URL not found: " << server.uri() << endl;
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}


// --------------------------------------------------------------------------------------------- Root etc.
void handleRoot()   {send_file("/index.html");}
void handleFavicon() {send_file("/favicon.ico");}


// --------------------------------------------------------------------------------------------- Ajax (and commands)
void handleAjax() {
  if (server.hasArg("cmd")) {
    String cmd = server.arg("cmd");
    if (cmd == "1") {
      // start cooking
      // TODO: do this properly. state shoud be encapsulated and changed by functions
      p.state = STATE_COOKING;
    }
  }
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
    if (p.AP_mode != WIFI_APMODE) {
      // TODO: scanning not possible
    } else {
      // TODO: make this async
      logger << "WiFi scan started" << endl;
      int n = WiFi.scanNetworks();
      logger << "WiFi scan finished (" << n << " networks found)" << endl;
      bool first = true;
      server.setContentLength(CONTENT_LENGTH_UNKNOWN);
      server.send(200, "text/json", String(""));
      server.sendContent("{\"result\":" + String(n) + ", \"list\":[\n");
      for (int i=0; i<n; i++) {
        if (!first) {
          server.sendContent(",\n");
        }
        first = false;
        server.sendContent("{\"ssid\":\"" + WiFi.SSID(i) + "\"" 
          + ", \"Ch\":" + String(WiFi.channel(i)) 
          + ", \"encryption\":" + (WiFi.encryptionType(i) == ENC_TYPE_NONE ? "\"open\"" : "\"encrypted\"")
          + ", \"RSSI\":" + String(WiFi.RSSI(i))
          + "}");
      }
      server.sendContent("]}");
    }
  } else {
    send_file("/wifi.html");
  }
}


// --------------------------------------------------------------------------------------------- recipe
void handleRecipe() {
  send_file("/rec.html");
  //TODO
}

// --------------------------------------------------------------------------------------------- data
void handleData()   {
  if (!server.hasArg("t")) {
    // send html page
    send_file("/data.html");
    return;
  }
  
  //else send data
  bool first = true;
  unsigned long t = strtoul(server.arg("t").c_str(), NULL, 10);
  logger << "handleData(): dl_rewind(" << t << ")" << endl;
  dl_rewind("TODO", t);
  if (!dl_hasMore()) {
    logger << "handleData(): nothing (!dl_hasMore())" << endl;
    server.send(200, "text/json", "{\"data\":[]}");
    return;    
  }
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", String(""));
  server.sendContent("{\"data\":[");

  while (dl_hasMore()) {
    if (!first) server.sendContent(",");
    first = false;
    server.sendContent(dl_getNext());
  }
  server.sendContent("]}");
  logger << "handleData(): done" << endl;
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

  logger.rewind(t);
  if (!logger.hasMore()) {
    server.send(200, "text/json", "{\"log\":[]}");
    return;
  }
  
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", String(""));
  server.sendContent("{\"log\":[");

  while (logger.hasMore()) {
    if (!first) server.sendContent(",");
    first = false;

    String entry_str = logger.getNext();
    if (entry_str.length() > 0) {
      Logfile::LogEntryStruct entry = logger.strToEntry(entry_str);
      String s = "{\"ts\":" + String(entry.timestamp) + ", \"msg\":\"" + String(entry.message) + "\"}\r";
      server.sendContent(s);
    }
  }
  server.sendContent("]}");
}


void changeParam(String arg_name, String param_name, double *param) {
  String str_param = "Parameter";
  String str_from  = "geändert von";
  String str_to    = "auf";
  if (server.hasArg(arg_name)) {
      double val = server.arg(arg_name).toDouble();
      logger << str_param << ' ' << param_name << ' ' << str_from << ' ' << *param << ' ' << str_to << ' ' << val << endl;
      *param = val;
  }  
}


// --------------------------------------------------------------------------------------------- config
void changeParam(String arg_name, String param_name, int *param) {
  String str_param = "Parameter";
  String str_from  = "geändert von";
  String str_to    = "auf";
  if (server.hasArg(arg_name)) {
      int val = server.arg(arg_name).toInt();
      logger << str_param << ' ' << param_name << ' ' << str_from << ' ' << *param << ' ' << str_to << ' ' << val << endl;
      *param = val;
  }  
}


void handleConfig() {
  if (server.hasArg("save")) {
    changeParam("kp", "Verstärkung", &(p.kp));
    changeParam("tn", "Nachstellzeit", &(p.tn));
    changeParam("tv", "Vorhaltezeit", &(p.tv));
    changeParam("tz", "UTC Offset", &(p.tzoffset));
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
    if (filesystem.exists(fn)) {
      filesystem.remove(fn);
    }
    uploadFile = filesystem.open(fn, "w");
    logger << "Upload: START, filename: " << fn << endl;
  }
    
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
      logger << "Upload: WRITE, Bytes: " << upload.currentSize << endl;
    } else {
      logger << "Upload: WRITE (failed)" << endl;
    }
  }
  
  else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
    }
    logger << "Upload: END, Size: " << upload.totalSize << endl;
  }
}

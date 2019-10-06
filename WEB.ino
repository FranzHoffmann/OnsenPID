void setup_webserver(ESP8266WebServer *s) {
  s->on("/",     handleRoot);
  s->on("/rec",  handleRecipe);
  s->on("/data", handleData);
  s->on("/cfg",  handleConfig);
  s->on("/wifi", handleWifi);
  s->on("/log",  handleLog);
  s->on("/logd",  handleLogData);
  s->on("/s",    handleAjax);
  s->on("/favicon.ico",    handleFile);

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


bool readFile(const String &filename, String &content, bool substitute) {
  if (!filesystem->exists(filename)) {
    logger << "readFile(): file not found: " << filename << endl;
    return false;
  }
  File f = filesystem->open(filename, "r");
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
}
/* 
 *  read a file, substitute variable like {{ACT}} and {{SET}}, and send to web server
 *  everything enclosed in {{ }} is treated as a variable and sent to substitute()
 *  use escape character '\' in file to output {{: \{\{
 */
void send_file(String filename) {
  String reply; reply.reserve(8192);
  if (!readFile(filename, reply, true)) {
    return fail("file not found: " + filename);
  }
  server.send(200, getContentType(filename), reply);
}

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

void handleRoot()   {
  send_file("/index.html");
}

void handleWifi()   {
  send_file("/wifi.html");
  //TODO
}

void handleAjax()   {
  send_file("/ajax.json");
  //TODO
}

void handleFile()   {
  send_file(server.uri());
}

void handleLog()    {
  send_file("/log.html");
  //TODO: merge log and logd
}

void handleRecipe() {
  send_file("/rec.html");
  //TODO
}

void handleData()   {
  send_file("/data.html");
  //TODO
}


void handleLogData() {
  String reply; reply.reserve(8192);
  // TODO: Schrittweise übergeben, Speicher beachten
  logger.rewind();
  while (logger.hasMore()) {
    char line[80];
    logger.readNextMsg(line, sizeof(line));
    reply += line;
    reply += "\r\n";
  }
  server.send(200, "text/plain", reply);
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

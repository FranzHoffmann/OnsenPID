void setup_webserver(ESP8266WebServer *s) {
  s->on("/",    handleRoot);
  s->on("/cfg", handleConfig);
  s->on("/sta", handleStatus);
  s->on("/log", handleLog);
  s->on("/s",   handleAjax);
  s->begin();
}

// ----------------------------------------------------------------------------- web server
String getContentType(String filename) {
  if (server.hasArg("download")) {
    return "application/octet-stream";
  } else if (filename.endsWith(".htm")) {
    return "text/html";
  } else if (filename.endsWith(".html")) {
    return "text/html";
  } else if (filename.endsWith(".css")) {
    return "text/css";
  } else if (filename.endsWith(".js")) {
    return "application/javascript";
  } else if (filename.endsWith(".png")) {
    return "image/png";
  } else if (filename.endsWith(".gif")) {
    return "image/gif";
  } else if (filename.endsWith(".jpg")) {
    return "image/jpeg";
  } else if (filename.endsWith(".ico")) {
    return "image/x-icon";
  } else if (filename.endsWith(".xml")) {
    return "text/xml";
  } else if (filename.endsWith(".pdf")) {
    return "application/x-pdf";
  } else if (filename.endsWith(".zip")) {
    return "application/x-zip";
  } else if (filename.endsWith(".gz")) {
    return "application/x-gzip";
  }
  return "text/plain";
}


void returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
}


String substitute(String var) {
  String s;
  if (var == "ACT") return String(p.act);
  if (var == "SET") return String(p.set);
  if (var == "OUT") return String(p.out);
  if (var == "KP") return String(p.kp);
  if (var == "TN") return String(p.tn);
  if (var == "TIME_LEFT") return String(p.set_time - p.act_time);
  
  return var;
}

/* 
 *  read a file, substitute variable like {{ACT}} and {{SET}}, and send to web server
 *  everything enclosed in {{ }} is treated as a variable and sent to substitute()
 *  use escape character '\' in file to output {{: \{\{
 */
void send_file(String filename) {
  if (!filesystem->exists(filename)) {
    return returnFail("file not found: " + filename);
  }
  File f = filesystem->open(filename, "r");
  if (!f) {
    return returnFail("could not open file");
  }
  
  String reply; reply.reserve(8192);
  String var;
  enum {TEXT, ESCAPED, VAR1, VAR_START, VAR, VAR_END} state;
  state = TEXT;
  while (f.available()) {
    char c = f.read();
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
            reply += c;
        }
        break;

      case ESCAPED:
        reply += c;
        state = TEXT;
        break;

      case VAR_START:                      // first { found
        if (c == '{') {
          state = VAR;
          var = "";
        } else {
          reply += '{';
          reply += c;
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
          reply += substitute(var);
          state = TEXT;
        } else {
          // should not happen
          var += '}';
          var += c;
        }
        break;
    }
  }
  f.close();  
  server.send(200, getContentType(filename), reply);
}


void handleRoot()   {send_file("/index.html");}
void handleConfig() {send_file("/cfg.html");}
void handleStatus() {send_file("/status.html");}
void handleLog()    {send_file("/log.html");}
void handleAjax()   {send_file("/ajax.json");}

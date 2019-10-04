void setup_webserver(ESP8266WebServer *s) {
  s->on("/",     handleRoot);
  s->on("/cfg",  handleConfig);
  s->on("/wifi", handleWifi);
  s->on("/log",  handleLog);
  s->on("/s",    handleAjax);
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


String substitute(String var) {
  String s;
  if (var == "ACT") return String(p.act);
  if (var == "SET") return String(p.set);
  if (var == "OUT") return String(p.out);
  if (var == "KP") return String(p.kp);
  if (var == "TN") return String(p.tn);
  if (var == "TIME_LEFT") return String(p.set_time - p.act_time);
  if (var == "HOSTNAME") return HOSTNAME;
  if (var == "SSID") return p.ssid;
  
  if (var == "CSS")  {
    File f = filesystem->open("stylesheet.css", "r");
    if (!f) return "";
    String s = f.readString();
    f.close();
    return s;
  }
  
  return var; // ignore unknown strings
}

/* 
 *  read a file, substitute variable like {{ACT}} and {{SET}}, and send to web server
 *  everything enclosed in {{ }} is treated as a variable and sent to substitute()
 *  use escape character '\' in file to output {{: \{\{
 */
void send_file(String filename) {
  if (!filesystem->exists(filename)) {
    return fail("file not found: " + filename);
  }
  File f = filesystem->open(filename, "r");
  if (!f) {
    return fail("could not open file");
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
void handleWifi()   {send_file("/wifi.html");}
void handleLog()    {send_file("/log.html");}
void handleAjax()   {send_file("/ajax.json");}

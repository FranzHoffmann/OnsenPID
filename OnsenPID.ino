/*
  TODO
  ====
  - data Logger / chart display webpage
  - log data access with "batch no"

  - D-Part
  - repalce "Einschalten" by "Start dialog"
  
  more ideas
  ==========
  - autotuning
  - improve anti-windup
  - complex recipes, more recipes, selection via LCD/web
  - better statistics: runtime and ms/s for each task
*/

// TODO: remove
//#include <NTPClient.h>      // note: git pull request 22 (asynchronus update) required
//#include <WiFiUdp.h>
//WiFiUDP ntpUDP;
//NTPClient timeClient(ntpUDP);


#include <Streaming.h>
#include <ESP8266WiFi.h>
#include <PString.h>
#include "Logfile.h"
#include <FS.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include "src/MemInfo/MemoryInfo.h"
#include "Config.h"
#include <DS18B20.h>
#include "Process.h"
#include "src/Clock/Clock.h"
#include "LCDMenu.h"


#define PWM_PORT D4
#define TMP_PORT D7


ESP8266WebServer server(80);
FS filesystem = SPIFFS;
OneWire oneWire(TMP_PORT);
DS18B20 thermometer(&oneWire);

/* simple scheduler */
#define MAX_TASKS 20
typedef int (*task)();    // a task is a function "int f()". returns deltaT for next call.
struct task_t {
  task t;
  unsigned long nextrun;
  const char *taskname;
  unsigned long tsum, tmin, tmax, tcount;
};
task_t tasklist[MAX_TASKS];

enum WiFiEnum {WIFI_OFFLINE, WIFI_CONN, WIFI_APMODE};

struct param {
  WiFiEnum AP_mode;
  double set;			// temperature setpoint, from recipe
  double act;			// actual temperature, from thermometer
  double out;			// actual power, from controller
  boolean released;				// controller is released
  int screen;                   // active screen
  boolean sensorOK;
} p;

Process sm;
Config cfg(sm);
LCDMenu lcd(sm);


// ------------------------------------------------------------------------ useful functions

void init_params() {
  cfg.read();
}

void init_stats(task_t *t) {
      t->tsum = 0;
      t->tmin = 999999;
      t->tmax = 0;
      t->tcount = 0;
}

boolean start_task(task t, const char *name) {
  for (int i=0; i<MAX_TASKS; i++) {
    if (tasklist[i].t == NULL) {
      tasklist[i].t = t;
      tasklist[i].nextrun = millis();
      tasklist[i].taskname = name;
      init_stats(&tasklist[i]);
      return true;
    }
  }
  Logger << "Unable to start Task" << endl;
  return false;
}




double scale(int in, double min, double max) {
  double in_r = in;
  return (in_r / 1024) * (max - min) + min;
}

double pt1(double x, double tau, double T) {
  static double y_old;
  double a = T / tau;
  double y =  a * x + (1.0 - a) * y_old;
  y_old = y;
  return y;
}

double xscale(double in, double in_min, double in_max, double out_min, double out_max) {
  return (in - in_min)/(in_max - in_min) * (out_max - out_min) + out_min;
}

double linearize(double x, double *xval, double *yval, int len) {
  for (int idx = 1; idx<len; idx++) {
    if (xval[idx-1] > x && xval[idx] <= x) {
      return xscale(x, xval[idx], xval[idx-1], yval[idx], yval[idx-1]);
    }
  }
  return yval[len-1];
}

double limit(double x, double xmin, double xmax) {
  if (x < xmin) return xmin;
  if (x > xmax) return xmax;
  return x; 
}



// -------------------------------------------------------------------------- NTP Task
/* task: ntp client */
int task_ntp() {
  Clock.update();
  //timeClient.update();
  return 10;
}

// -------------------------------------------------------------------------- Statistik Task
/* task: calculate debug statistics */
int task_statistics() {
  //Logger << endl << "Statistics: " << endl;
  for (int i=0; i<MAX_TASKS; i++) {
    if (tasklist[i].t != NULL) {
      double avg = tasklist[i].tcount > 0 
                 ? (double)tasklist[i].tsum / tasklist[i].tcount
                 : 0;
      //Logger << tasklist[i].taskname;
      //for (int i=strlen(tasklist[i].taskname); i<20; i++) { Serial << '.'; }  // crashes?
      //Logger << ',' << tasklist[i].tmin
     //      << ','<< avg
     //      << ','<< tasklist[i].tmax
     //      << "ms" << endl;
      init_stats(&tasklist[i]);
    }
  }   
  //Logger << "Free RAM: " << getTotalAvailableMemory() << ", largest: " << getLargestAvailableBlock() << endl;
  return 600000; // good thing int seems to be 32 bit
}

// -------------------------------------------------------------------------- Recipe Task
int task_recipe() {
  sm.update();
  p.set = sm.getCookingTemp();
  p.released = (sm.getState() == State::COOKING);
  return 1000;
}

// -------------------------------------------------------------------------- Temperature Task
/* task: read temperature */
int task_read_temp() {
  static uint32_t counter = 0;
  if (thermometer.isConversionComplete()) {
    p.act = thermometer.getTempC();
    counter = 0;
    if (!p.sensorOK) {
      p.sensorOK = true;
      Logger << "thermometer ok" << endl;
    }
    thermometer.requestTemperatures();
  } else {
    counter++;
    if (counter > 5) {
      if (p.sensorOK) {
        p.sensorOK = false;
        Logger << "thermometer failure, resetting" << endl;
      }
      thermometer.requestTemperatures();    
    }
  }

  // simulation:
  //p.act = pt1((p.out_b ? 150.0 : 20.0), 200, cycle/1000.0);
  return 1000;  
}

// -------------------------------------------------------------------------- LCD Task
/* task: update LCD */
int task_lcd() {
	lcd.update();
	
  // TODO
  /* 
  if ((line1 == line1_old) && (line2 == line2_old)) return 50;
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd << line1;
  lcd.setCursor(0,1);
  lcd << line2;
  line1_old.begin(); line1_old << line1;
  line2_old.begin(); line2_old << line2;
  */
  return 50;  
}

// -------------------------------------------------------------------------- Webserver Task
int task_webserver() {
  server.handleClient();
  MDNS.update();
  ArduinoOTA.handle();
  return 20;
}


// ----------------------------------------------------------------------------- loop
void loop() {
  unsigned long t = millis();
  unsigned long t0, dt;
  
  for (int i=0; i<MAX_TASKS; i++) {
    if (tasklist[i].t != NULL && t > tasklist[i].nextrun) {
      t0 = millis();
      tasklist[i].nextrun += (tasklist[i].t)();
      dt = millis()- t0;
      tasklist[i].tcount++;
      tasklist[i].tsum += dt;
      tasklist[i].tmin = min(tasklist[i].tmin, dt);
      tasklist[i].tmax = max(tasklist[i].tmax, dt);
    }
  }
}

    
// ----------------------------------------------------------------------------- start_WiFi
void start_WiFi() {
  Logger << "Versuche verbindung mit '" << cfg.p.ssid << "'" << endl;
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.p.ssid, cfg.p.pw);
  for (int i=0; i<100; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      Logger << "WiFi verbunden (IP: " << WiFi.localIP() << ")" << endl;
      p.AP_mode = WIFI_CONN;
      return;
    }
    delay(100);
  }
  Logger << "WiFi-Verbindung fehlgeschlagen" << endl;

  // open AP
  uint8_t macAddr[6];
  String ssid = "Onsenei-";
  WiFi.softAPmacAddress(macAddr);
  for (int i = 4; i < 6; i++) {
    ssid += String(macAddr[i], HEX);
  }
  boolean success = WiFi.softAP(ssid);
  if (success) {
    Logger << "Access Point erstellt: '" << ssid << "'" << endl;
    p.AP_mode = WIFI_APMODE;
  } else {
    Logger << "Acces Point erstellen fehlgeschlagen" << endl;
    p.AP_mode = WIFI_OFFLINE;
  }
  return;
}

void setup_ds18b20() {
  Logger  << ("Initializing thermometer DS18B20 ") << endl;
  Logger << "DS18B20 Library version: " << DS18B20_LIB_VERSION << endl;
  thermometer.begin();
  thermometer.setResolution(12);
  thermometer.requestTemperatures();
}

// ----------------------------------------------------------------------------- setup
void setup() {
  pinMode(PWM_PORT, OUTPUT);
  Serial.begin(115200);

  filesystem.begin();
  Logger << "------ REBOOT" << endl;
  Logger << "File system mounted" << endl;
  Dir dir = filesystem.openDir("/");
  while (dir.next()) {
    String fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    Serial << "- '" << fileName << "', " << fileSize << " bytes" << endl;
  } 
    
  cfg = Config(sm, "/config.ini");
  init_params();

  lcd.setup();
  Logger << "LCD initialized" << endl;
    
  start_WiFi();
  Logger << "WiFi initilaized" << endl;

  setup_webserver(&server);
  Logger << "Webserver initialized" << endl;
  
  MDNS.begin(cfg.p.hostname);
  MDNS.addService("http", "tcp", 80);
  Logger << "MDNS initialized" << endl;
  
  setup_OTA();
  Logger << "OTA initialized " << endl;

  //timeClient.setUpdateCallback(onNtpUpdate);
  //timeClient.begin();
  Logger << "NTPClient initialized" << endl;
  
  setup_dl(); // data Logger
  Logger << "DataLogger initialized" << endl;

  setup_ds18b20();
  
  start_task(task_lcd, "lcd");
  start_task(task_ntp, "ntp");
  start_task(task_recipe, "recipe");
  start_task(task_read_temp, "r_temp");
  start_task(task_statistics, "stats");
  start_task(task_webserver, "apache");

  setup_controller();
}

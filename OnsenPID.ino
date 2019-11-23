/*
  TODO
  ====
  - data logger / chart display webpage
  - log data access with "batch no"
  - use real temperature sensor
  - WiFi setup from web interface
  - D-Part
  - start in x minutes / finish at x:xx 
  - repalce "Einschalten" by "Start dialog"
  - move PWM (and PID?) to interrupt

  DONE
  ====
  - hostname
  - persistant storage of parameters

  
  more ideas
  ==========
  - autotuning
  - improve anti-windup
  - complex recipes, more recipes, selection via LCD/web
  - better statistics: runtime and ms/s for each task
*/

#include <Streaming.h>
#include <NTPClient.h>      // note: git pull request 22 (asynchronus update) required
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <LiquidCrystal.h>
#include <PString.h>
#include "Logfile.h"
#include <FS.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include "MemoryInfo.h"
#include "Config.h"
#include <DS18B20.h>

#define LCD_RS D5
#define LCD_EN D6
#define LCD_D4 D0
#define LCD_D5 D1
#define LCD_D6 D2
#define LCD_D7 D3

#define PWM_PORT D4
#define TMP_PORT D7

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
ESP8266WebServer server(80);
FS filesystem = SPIFFS;
Logfile logger(filesystem, Serial, timeClient);
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
DS18B20 ds(TMP_PORT);
uint8_t selected;  // aktiver ds18b20

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

/* Buttons and menu p.screens */
enum StateEnum {STATE_IDLE, STATE_COOKING, STATE_FINISHED, STATE_ERROR};
enum ButtonEnum {BTN_SEL, BTN_UP, BTN_DN, BTN_LE, BTN_RI, BTN_NONE};
#define DISP_MAIN 0
#define DISP_SET_SET 1
#define DISP_SET_TIME 2
#define DISP_OUTPUT 3
#define DISP_SET_KP 4
#define DISP_SET_TN 5
#define DISP_IP 6
#define SCREEN_COUNT 7

#define DEGC '\1' << 'C'   // custom caracter for degree
#define ARROW '\2'         // custom character for up/down arrow

enum WiFiEnum {WIFI_OFFLINE, WIFI_CONN, WIFI_APMODE};
struct param {
  StateEnum state;              // global state
  WiFiEnum AP_mode;
  int set_time, act_time;       // remaining time (s)
  double set, act, out;         // controller i/o
  double t1;                    // filter time for act temperature
  boolean out_b;                // heater is on
  int screen;                   // active screen
} p;

Config cfg;

char buf[4][17];
PString line1(buf[0], sizeof(buf[0]));
PString line2(buf[1], sizeof(buf[1]));
PString line1_old(buf[2], sizeof(buf[2]));
PString line2_old(buf[3], sizeof(buf[3]));




// ------------------------------------------------------------------------ useful functions

void init_params() {
  p.t1 = 0.3;
  p.set_time = 600;
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
  logger << "Unable to start Task" << endl;
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
  timeClient.update();
  return 10;  
}

// -------------------------------------------------------------------------- Statistik Task
/* task: calculate debug statistics */
int task_statistics() {
  Serial << endl << "Statistics: " << endl;
  for (int i=0; i<MAX_TASKS; i++) {
    if (tasklist[i].t != NULL) {
      double avg = tasklist[i].tcount > 0 
                 ? (double)tasklist[i].tsum / tasklist[i].tcount
                 : 0;
      Serial << tasklist[i].taskname;
      //for (int i=strlen(tasklist[i].taskname); i<20; i++) { Serial << '.'; }  // crashes?
      Serial << '\t' << tasklist[i].tmin
             << '\t'<< avg
             << '\t'<< tasklist[i].tmax
             << "ms" << endl;
      init_stats(&tasklist[i]);
    }
  }   
  logger << "Free RAM: " << getTotalAvailableMemory() << ", largest: " << getLargestAvailableBlock() << endl;
  return 60000; // good thing int seems to be 32 bit
}

// -------------------------------------------------------------------------- Recipe Task
int task_recipe() {
  static StateEnum oldstate;
  if (oldstate != p.state) {
    oldstate = p.state;
    switch (p.state) {
      case STATE_IDLE:
        logger << "Grundzustand" << endl;
        break;
      case STATE_COOKING:
        logger << "Kochen gestartet" << endl;
        dl_startBatch();
        break;
      case STATE_FINISHED:
        logger << "Kochen abgeschlossen" << endl;
        dl_endBatch();
        break;
      case STATE_ERROR:
        logger << "Systemfehler erkannt" << endl;
        break;
    }
  }
  if (p.state == STATE_COOKING) {
    p.act_time += 1;
    if (p.act_time >= p.set_time) {
      p.act_time = p.set_time;
      p.state = STATE_FINISHED;
    }
  }
  return 1000;
}

// -------------------------------------------------------------------------- Temperature Task
/* task: read temperature */
int task_read_temp() {
  p.act = selected ? ds.getTempC() : 0.0;
  return 2000;
  /*
  int cycle = 1;
  /*
  int reading = analogRead(A0);
  double act = scale(reading, 0.0, 100.0);
  p.act = pt1(act, p.t1, cycle/1000.0);
  
  // simulation:
  p.act = pt1((p.out_b ? 150.0 : 20.0), 200, cycle/1000.0);
  return cycle;*/
}

// -------------------------------------------------------------------------- PID Task
/* task: PID controller */
int task_PID() {
  static double ipart;
  int ta = 50;

  if (p.state == STATE_COOKING) {
    double e = p.set - p.act;
    double ppart = cfg.p.kp * e;
    ipart = limit(ipart + ta/1000.0/cfg.p.tn * e, 0, 100.0);
    p.out = limit(ppart + ipart, 0.0, 100.0);

  } else {
    ipart = 0.0;
    p.out = 0.0;
  }
  return ta;
}

// -------------------------------------------------------------------------- PWM Task
/* task: PWM */
int task_PWM() {
  static unsigned long T;

  int dt   = 10;
  int Tmax = 1000;
  unsigned long Thi = (p.out / 100.0 * Tmax);

  if (T>=Tmax)  T = 0;
  T += dt;
  p.out_b = T <= Thi;
  digitalWrite(PWM_PORT, p.out_b);
  
  return dt;
}

// -------------------------------------------------------------------------- LCD Task
/* task: update LCD */
int task_lcd() {
  if ((line1 == line1_old) && (line2 == line2_old)) return 50;
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd << line1;
  lcd.setCursor(0,1);
  lcd << line2;
  line1_old.begin(); line1_old << line1;
  line2_old.begin(); line2_old << line2;
  return 500;  
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
  logger << "Versuche verbindung mit '" << cfg.p.ssid << "'" << endl;
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.p.ssid, cfg.p.pw);
  for (int i=0; i<100; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      logger << "WiFi verbunden (IP: " << WiFi.localIP() << ")" << endl;
      p.AP_mode = WIFI_CONN;
      return;
    }
    delay(100);
  }
  logger << "WiFi-Verbindung fehlgeschlagen" << endl;

  // open AP
  uint8_t macAddr[6];
  String ssid = "Onsenei-";
  WiFi.softAPmacAddress(macAddr);
  for (int i = 4; i < 6; i++) {
    ssid += String(macAddr[i], HEX);
  }
  boolean success = WiFi.softAP(ssid);
  if (success) {
    cfg.p.ssid = ssid;
    logger << "Access Point erstellt: '" << ssid << "'" << endl;
    p.AP_mode = WIFI_APMODE;
  } else {
    logger << "Acces Point erstellen fehlgeschlagen" << endl;
    p.AP_mode = WIFI_OFFLINE;
  }
  return;
}

void setup_ds18b20() {
    logger  << ("DS18B20: ");
  logger << ds.getNumberOfDevices() << " devices found" << endl;
  while (selected = ds.selectNext()) {
    switch (ds.getFamilyCode()) {
      case MODEL_DS18S20:
        logger << "Model: DS18S20/DS1820" << endl;
        break;
      case MODEL_DS1822:
        logger << "Model: DS1822" << endl;
        break;
      case MODEL_DS18B20:
        logger << "Model: DS18B20" << endl;
        break;
      default:
        logger << "Unrecognized Device" << endl;
        break;
    }
      uint8_t address[8];
    ds.getAddress(address);

    logger << "Address:";
    for (uint8_t i = 0; i < 8; i++) {
      logger << address[i];
    }
    logger << endl;
    
    logger << "Resolution: " << ds.getResolution() << endl;
    logger << "Power Mode: ";
    if (ds.getPowerMode()) {
      logger << "External" << endl;
    } else {
      logger << "Parasite" << endl;
    }
    break;
  }
}

// ----------------------------------------------------------------------------- setup
void setup() {
  pinMode(PWM_PORT, OUTPUT);
  Serial.begin(115200);

  filesystem.begin();
  logger << "File system mounted. contents:" << endl;
  Dir dir = filesystem.openDir("/");
  while (dir.next()) {
    String fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    Serial << "- '" << fileName << "', " << fileSize << " bytes" << endl;
  } 
    
  cfg = Config("/config.ini", &logger);
  init_params();

  lcd.begin(16, 2);
  byte char_deg[8] = {6,9,9,6,0,0,0,0};
  byte char_updn[8] = {4, 14, 31, 4, 4, 31, 14, 4}; 
  lcd.createChar(1, char_deg);
  lcd.createChar(2, char_updn);
  lcd.clear();
  logger << "LCD initialized" << endl;
    
  start_WiFi();
  logger << "WiFi initilaized" << endl;

  setup_webserver(&server);
  logger << "Webserver initialized" << endl;
  
  MDNS.begin(cfg.p.hostname);
  MDNS.addService("http", "tcp", 80);
  logger << "MDNS initialized" << endl;
  
  setup_OTA();
  logger << "OTA initialized " << endl;
  
  timeClient.begin();
  logger << "NTPClient initialized" << endl;
  
  setup_dl(); // data logger
  logger << "Datalogger initialized" << endl;

  setup_ds18b20();
  
  start_task(task_keyboard, "keybd");
  start_task(task_lcd, "lcd");
  start_task(task_ntp, "ntp");
  start_task(task_recipe, "recipe");
//  start_task(task_read_temp, "r_temp");
  start_task(task_PID, "PID");
  start_task(task_PWM, "PWM");
  start_task(task_statistics, "stats");
  start_task(task_webserver, "apache");
}

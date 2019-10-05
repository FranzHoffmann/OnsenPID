/*

*/

#include <Streaming.h>
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <LiquidCrystal.h>
#include <PString.h>
#include "Log2.h"
#include <FS.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include "MemoryInfo.h"

#define HOSTNAME "onsen"

Log2 logger;
ESP8266WebServer server(80);

FS* filesystem = &SPIFFS;

/* simple scheduler */
#define MAX_TASKS 10
typedef int (*task)();    // a task is a function "int f()". returns deltaT for next call.
struct task_t {
  task t;
  unsigned long nextrun;
  const char *taskname;
  unsigned long tsum, tmin, tmax, tcount;
};
task_t tasklist[MAX_TASKS];

/* Buttons and menu p.screens */
typedef enum WiFiEnum {WIFI_OFFLINE, WIFI_CONN, WIFI_APMODE} WiFiEnum;
typedef enum StateEnum {STATE_IDLE, STATE_COOKING, STATE_FINISHED, STATE_ERROR} State;
typedef enum BtnEnum {BTN_SEL, BTN_UP, BTN_DN, BTN_LE, BTN_RI, BTN_NONE} Button;
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

#define PWM_PORT D4

struct param {
  State state;                  // global state
  int set_time, act_time;       // remaining time (s)
  double set, act, out;         // controller i/o
  double kp, tn, tv;            // controller parameter
  double t1;                    // filter time for act temperature
  boolean out_b;                // heater is on
  int screen;                   // active screen
  char ssid[33], pw[33];
  WiFiEnum AP_mode;
} p;

char buf[4][17];
PString line1(buf[0], sizeof(buf[0]));
PString line2(buf[1], sizeof(buf[1]));
PString line1_old(buf[2], sizeof(buf[2]));
PString line2_old(buf[3], sizeof(buf[3]));


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, 7200);
LiquidCrystal lcd(D5, D6, D0, D1, D2, D3);      // lcd(rs, en, d4, d5, d6, d7);


// ------------------------------------------------------------------------ useful functions

void init_params() {
  p.set = 45.0;
  p.kp = 30.0;
  p.tn = 20.0;
  p.tv = 0.0;
  p.t1 = 0.3;
  p.set_time = 600;

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
             << endl;
      init_stats(&tasklist[i]);
    }
  }
  logger << "Free RAM: " << getTotalAvailableMemory() << ", largest: " << getLargestAvailableBlock() << endl;
  return 60000;
}

// -------------------------------------------------------------------------- Recipe Task
int task_recipe() {
  static State oldstate;
  if (oldstate != p.state) {
    oldstate = p.state;
    switch (p.state) {
      case STATE_IDLE:
        logger << "Grundzustand" << endl;
        break;
      case STATE_COOKING:
        logger << "Kochen gestartet" << endl;
        break;
      case STATE_FINISHED:
        logger << "Kochen abgeschlossen" << endl;
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
  int cycle = 1;
  /*
  int reading = analogRead(A0);
  double act = scale(reading, 0.0, 100.0);
  p.act = pt1(act, p.t1, cycle/1000.0);
  */
  // simulation:
  p.act = pt1((p.out_b ? 150.0 : 20.0), 200, cycle/1000.0);
  return cycle;
}

// -------------------------------------------------------------------------- PID Task
/* task: PID controller */
int task_PID() {
  static double ipart;
  int ta = 50;

  if (p.state == STATE_COOKING) {
    double e = p.set - p.act;
    double ppart = p.kp * e;
    ipart = limit(ipart + ta/1000.0/p.tn * e, 0, 100.0);
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
  WiFi.mode(WIFI_STA);
  WiFi.begin();

  // wait for connection
  for (int i=0; i<100; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      logger << "WiFi connected (IP: " << WiFi.localIP() << ")" << endl;
      p.AP_mode = WIFI_CONN;
      return;
    }
    delay(100);
  }
  logger << "WiFi connection failed" << endl;

  // open AP
  uint8_t macAddr[6];
  char buffer[32];
  PString ssid(buffer, sizeof(buffer));

  WiFi.softAPmacAddress(macAddr);
  ssid << "Onsenei-";
  for (int i = 4; i < 6; i++) ssid.printf("%02x", macAddr[i]);
  boolean success = WiFi.softAP("Onsen-1234");
  if (success) {
    strncpy(p.ssid, buffer, sizeof(p.ssid));
    logger << "Opened Access Point '" << ssid << "'" << endl;
    p.AP_mode = WIFI_APMODE;
  } else {
    logger << "Failed to open AP" << endl;
    p.AP_mode = WIFI_OFFLINE;
  }
  return;
}


// ----------------------------------------------------------------------------- setup
void setup() {
  init_params();

  pinMode(PWM_PORT, OUTPUT);
  Serial.begin(115200);

  filesystem->begin();
  {
    Dir dir = filesystem->openDir("/");
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      logger << "FS File: " << fileName << " size: " << fileSize << endl;
    }
  }

  lcd.begin(16, 2);
  byte char_deg[8] = {6,9,9,6,0,0,0,0};
  byte char_updn[8] = {4, 14, 31, 4, 4, 31, 14, 4}; 
  lcd.createChar(1, char_deg);
  lcd.createChar(2, char_updn);
  lcd.clear();
  
  start_WiFi();

  MDNS.begin(HOSTNAME);
  setup_webserver(&server);
  MDNS.addService("http", "tcp", 80);

  setup_OTA();
  
  timeClient.begin();

  start_task(task_keyboard, "keybd");
  start_task(task_lcd, "lcd");
  start_task(task_ntp, "ntp");
  start_task(task_recipe, "recipe");
  start_task(task_read_temp, "r_temp");
  start_task(task_PID, "PID");
  start_task(task_PWM, "PWM");
  start_task(task_statistics, "stats");
  start_task(task_webserver, "apache");
}

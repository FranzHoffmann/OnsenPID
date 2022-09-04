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

#include <Streaming.h>
#include <ESP8266WiFi.h>
#include <PString.h>
#include <LittleFS.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <DS18B20.h>
#include <MemoryInfo.h>

#include <Global.h>
#include <Clock.h>
#include <util.h>
#include <LCDMenu.h>
#include <tasks.h>
#include <Config.h>
#include <Process.h>
#include <Logfile.h>
#include <WEB.h>
#include <DataLogger.h>
#include <Controller.h>

#define TMP_PORT D7

// remove? FS filesystem = LittleFS;
OneWire oneWire(TMP_PORT);
DS18B20 thermometer(&oneWire);
Config cfg;
Process sm(&cfg);
extern ESP8266WebServer server;

// ---------------------------------------------------------------- WIFI
WiFiEventHandler OnConnected, OnGotIp, OnDisconnected;
WiFiEventHandler OnStationConnected, OnStationDisconnected;

String macToString(const unsigned char* mac) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

// ---------------------------------------------------------- setup WiFi
void start_WiFi() {
  Logger << "WiFi-Configuration:" << endl;
  WiFi.printDiag(Logger);
 
	/* callback when connected in station mode (when we get an IP) */
	OnConnected = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& event) {
		Logger << "WiFi verbunden" << endl;
	});

	/* callback when connected in station mode (when we get an IP) */
	OnGotIp = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& event) {
		Logger << "IP erhalten: " << (WiFi.localIP()) << endl;
	});
  
	/* callback when we get disconnected (in station mode) */
	OnDisconnected = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) {
		Logger << "WiFi-Verbindung verloren" << endl;
	});

	/* callback when somebody connects (in AP mode) */
	OnStationConnected = WiFi.onSoftAPModeStationConnected([](const WiFiEventSoftAPModeStationConnected& event) {
    Logger << "Client " << macToString(event.mac) << " verbunden" << endl;
	});

	/* callback when somebody disconnects (in AP mode) */
	OnStationDisconnected = WiFi.onSoftAPModeStationDisconnected([](const WiFiEventSoftAPModeStationDisconnected& event) {
		Logger << "Client " << macToString(event.mac) << " getrennt" << endl;
	});

	Logger << "Versuche verbindung mit '" << cfg.p.ssid << "'" << endl;
	WiFi.persistent(false);
	if (!WiFi.mode(WIFI_STA) || !WiFi.begin(cfg.p.ssid, cfg.p.pw) || (WiFi.waitForConnectResult(10000) != WL_CONNECTED)) {
		Logger << "WiFi-Verbindung fehlgeschlagen" << endl;

		// open AP
		uint8_t macAddr[6];
		String ssid = "Onsenei-";
		WiFi.softAPmacAddress(macAddr);
		for (int i = 4; i < 6; i++) {
			ssid += String(macAddr[i], HEX);
		}
		WiFi.mode(WIFI_AP);
		WiFi.softAP(ssid);
		Logger << "Access Point erstellt: '" << ssid << "'" << endl;
		WiFi.printDiag(Logger);
	} else {
    	Logger << "WiFi verbunden (IP: " << WiFi.localIP() << ")" << endl;
	}
	return;
}

// ----------------------------------------------------------------- OTA
void setup_OTA() {
    ArduinoOTA.setHostname(cfg.p.hostname.c_str());
  
	ArduinoOTA.onStart([]() {
		String type;
		if (ArduinoOTA.getCommand() == U_FLASH) {
			type = "sketch";
		} else {
			type = "filesystem";
			LittleFS.end();
		}
		lcd.clear();
		lcd.setCursor(0,0);
		lcd << "OTA Update...";
		Logger << "OTA Update beginnt (" << type << ")" << endl;
	});
  
    ArduinoOTA.onEnd([]() {
      Logger << "OTA update beendet" << endl;
    });
  
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		int x = (double)progress / (double)total * 16;
		lcd.setCursor(x, 1);
		lcd << "#";
    });
  
    ArduinoOTA.onError([](ota_error_t error) {
      Logger << "OTA update fehlgeschlagen" << endl;
    });
  
    ArduinoOTA.begin();
}


// ------------------------------------------------------------ NTP Task
/* task: ntp client */
int task_ntp() {
  Clock.update();
  return 10;
}


// --------------------------------------------------------- Recipe Task
int task_recipe() {
  sm.update();
  // write values from recipe to global struct for PID controller
  noInterrupts();
  p.set  = sm.out.set;
  p.kp   = sm.out.kp;
  p.tn   = sm.out.tn;
  p.tv   = sm.out.tv;
  p.emax = sm.out.emax;
  p.pmax = sm.out.pmax;
  p.released = (sm.getState() == State::COOKING);
  interrupts();

  return 1000;
}

// ---------------------------------------------------- Temperature Task
/* task: read temperature */
int task_read_temp() {
  static uint32_t counter = 0;
  if (thermometer.isConversionComplete()) {
    p.act = thermometer.getTempC();
    counter = 0;
    if (!p.sensorOK) {
      p.sensorOK = true;
      Logger << "Thermometer ok" << endl;
    }
    thermometer.requestTemperatures();
  } else {
    counter++;
    if (counter > 5) {
      if (p.sensorOK) {
        p.sensorOK = false;
        Logger << "Thermometer gestört" << endl;
      }
      thermometer.requestTemperatures();
    }
    controller_update(&p);
  }

  // simulation:
  //p.act = pt1((p.out_b ? 150.0 : 20.0), 200, cycle/1000.0);
  return 1000;
}

// ------------------------------------------------------------ LCD Task
/* task: update LCD */
int task_lcd() {
  LCDMenu_update();
  return 50;
}

// ------------------------------------------------------ Webserver Task
int task_webserver() {
  server.handleClient();
  MDNS.update();
  ArduinoOTA.handle();
  return 20;
}

// ----------------------------------------------------------- Alive Task
int task_lifesign() {
	Serial  << "Time: " << millis() << " - " << cfg.p.hostname << " is still alive" << endl;
	const char* const states[] = {"IDLE", "NO NETWORK", "SCAN_COMPLETED", "CONNECTED",
    "CONNECT FAILED", "CONNECTION LOST", "WRONG PASSWORD", "DISCONNECTED"};
	Serial << "WiFi: " << states[WiFi.status()];
	if (WiFi.status() == 3) {
		Serial << " / " << WiFi.localIP();
		Serial << " / " << WiFi.SSID();
		const char* const modes[] = { "NULL", "STA", "AP", "STA+AP" };
		Serial << " / " << modes[WiFi.getMode()];
		const char* const phymodes[] = { "", "b", "g", "n" };
		Serial << " / 802.11" << phymodes[(int) WiFi.getPhyMode()];
		Serial << " / Ch" << WiFi.channel();
		Serial << " / " << WiFi.RSSI() << "dBm" << endl;
	}
	Serial << "Mem:  " << ESP.getFreeHeap() << " / " << ESP.getMaxFreeBlockSize() << " / " << ESP.getHeapFragmentation() << endl;
	print_statistics();
	return 30000;
}

// ----------------------------------------------------------- Alive Task
int task_misc() {
	Logger.update();
	return 95;
}

// ---------------------------------------------------------------- loop
void loop() {
  run_tasks(millis());
}


// -------------------------------------------------- setup Temp. sensor
void setup_ds18b20() {
  Logger << "DS18B20 Bibliothek version: " << DS18B20_LIB_VERSION << endl;
  thermometer.begin();
  thermometer.setResolution(12);
  thermometer.requestTemperatures();
}

void onStateChanged() {
  switch (sm.getState()) {
    case State::COOKING:
      dl_startBatch();
      setScreen(Screen::MAIN_COOK);
      break;
    case State::FINISHED:
      dl_endBatch();
      break;
    case State::IDLE:
      dl_endBatch();
      break;
    default: 
      // TODO
      break;
  }
}

// --------------------------------------------------------------- setup
void setup() {
	Serial.begin(115200);
	LittleFS.begin();
	Logger.begin();

	Logger << "------ REBOOT" << endl;

	FSInfo fsinfo;
	LittleFS.info(fsinfo);
	Logger << "LittleFS blocksize: " << fsinfo.blockSize << endl;

	Dir dir = LittleFS.openDir("/");
	while (dir.next()) {
		String fileName = dir.fileName();
		size_t fileSize = dir.fileSize();
		Serial << "- '" << fileName << "', " << fileSize << " bytes" << endl;
	}
	cfg.setFilename("/config.ini");
	cfg.read();

	Clock.setTimeOffset(cfg.p.tzoffset * 3600);

	LCDMenu_setup();
	Logger << "LCD initialisiert" << endl;

	start_WiFi();
	Logger << "WiFi gestartet" << endl;

	setup_webserver(&server);
	Logger << "Webserver gestartet" << endl;

	MDNS.begin(cfg.p.hostname);
	MDNS.addService("http", "tcp", 80);
	Logger << "MDNS gestartet" << endl;

	setup_OTA();
	Logger << "OTA gestartet" << endl;

	setup_dl();
	Logger << "DataLogger gestartet" << endl;

	setup_ds18b20();
	Logger << "Thermometer initialisiert" << endl;

	start_task(task_lcd, "lcd");
	start_task(task_ntp, "ntp");
	start_task(task_recipe, "recipe");
	start_task(task_read_temp, "r_temp");
	start_task(task_webserver, "apache");
	start_task(task_lifesign, "lifesign");
	start_task(task_misc, "misc");

	// controller is running in timer interrupt
	setup_controller();

	sm.setCallback(&onStateChanged);
}




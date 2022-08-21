#include <LCDMenu.h>

#include <Arduino.h>
#include <LiquidCrystal.h>
#include <Process.h>
#include <Clock.h>
#include <PString.h>
#include <util.h>
#include <Config.h>
#include <ESP8266WiFi.h>
#include <Global.h>

Screen screen;
int rec_i, step_i, timer_mode, starttime;
String wifi_ssid, wifi_pw;

char buf1[17], buf2[17], buf3[17], buf4[17];
PString line1(buf1, sizeof(buf1));
PString line2(buf2, sizeof(buf2));
PString line1_old(buf3, sizeof(buf3));	// needed in OnsenPID.ino
PString line2_old(buf4, sizeof(buf4));
int cursor = -1, cursor_old;

static const char degc[3] = {'\1', 'C', '\0'};
#define CHAR_DEGC '\1'
#define CHAR_UPDN '\2'

LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);


ButtonEnum LCDMenu_readKey() {
	static int count = 0;
	static ButtonEnum previous;
	ButtonEnum btn;

	int ai = analogRead(LCD_AI);
	if (abs(ai - cfg.p.btn_up) < cfg.p.btn_tol) btn = BTN_UP;
	else if (abs(ai - cfg.p.btn_dn) < cfg.p.btn_tol) btn = BTN_DN;
	else if (abs(ai - cfg.p.btn_le) < cfg.p.btn_tol) btn = BTN_LE;
	else if (abs(ai - cfg.p.btn_ri) < cfg.p.btn_tol) btn = BTN_RI;
	else if (abs(ai - cfg.p.btn_sel) < cfg.p.btn_tol) btn = BTN_SEL;
	else               btn = BTN_NONE;

	// once on click, and start running while holding
	if (btn != previous) {
		previous = btn;
		count = 0;
		return btn;
	}
	count++;
	if (count > 10) {
		return btn;
	}
	return BTN_NONE;
}


void init_lcd()  {
		byte char_deg[8]  = {6,  9,  9, 6, 0,  0,  0, 0};
	byte char_updn[8] = {4, 14, 31, 4, 4, 31, 14, 4}; 

	lcd.begin(16, 2);
	lcd.createChar(1, char_deg);
	lcd.createChar(2, char_updn);
	lcd.clear();
	
	line1_old.begin();
	line2_old.begin();
}

/* set screen in case of external event */
void setScreen(Screen newScreen) {
	screen = newScreen;
}



/**
 * use this to show things like up/dn-character on the right side
 */
String combineStrChar(const char *s, char c) {
	String result;
	result.reserve(16);
	char si;
	while (si = *s++) result += si;
	while (result.length() < 16) result += ' ';
	result[15] = c;
	return result;
}


/**
 * This screen should be used for any value to be edited.
 * Edit mode should be activated when ENTER is pressed while the value is displayed
 * The value can be edited in line 2
 * The function will not touch line1.
 */
// There is only one global data structure for now.
// This is ok, because only one value can be edited at any time.
struct {
	double v, vmin, vmax, step;
	int decimals;
	String unit;
	EditMode mode;
	callback onChange;
	String s;
	int pos;
  int offset;
} editNumData;

String letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_.0123456789 ";

void start_edit_number(double value, int decimals, EditMode mode,
		double vmin, double vmax, double step, String unit, //const char *unit,
		callback onChange) {
	editNumData.v = value;
	editNumData.vmin = vmin;
	editNumData.vmax = vmax;
	editNumData.step = step;
	editNumData.unit = unit;
	//for (char *ptr = *unit; ptr++; *ptr != 0) editNumData.unit += *ptr;
	editNumData.decimals = decimals;
	editNumData.mode = mode;
	editNumData.onChange = onChange;
	screen = Screen::EDIT_NUMBER;
}


void disp_edit_number(ButtonEnum btn) {
	if (btn == BTN_UP) {
		editNumData.v += editNumData.step;
		if (editNumData.v > editNumData.vmax) editNumData.v = editNumData.vmin;
	}
	else if (btn == BTN_DN) {
		editNumData.v -= editNumData.step;
		if (editNumData.v < editNumData.vmin) editNumData.v = editNumData.vmax;
	}
	line2.begin();
	switch (editNumData.mode) {
		case EditMode::NUMBER:
			line2 = "->";
			line2 << String(editNumData.v, editNumData.decimals);
			line2 << " " << editNumData.unit;
			break;
		case EditMode::TIME:
			// v is in seconds since midnight. display as hh:mm
			line2 = "T>";
			line2 << secToTime(editNumData.v);
			break;
	}
	if (btn == BTN_SEL) {
		(editNumData.onChange)();
	};
}


void start_edit_text(String value, EditMode mode, callback onChange) {
  if (value.length() > 0) {
	  editNumData.s = value;
  } else {
    editNumData.s = " ";    
  }
	editNumData.pos = 0;
	editNumData.mode = mode;
	editNumData.onChange = onChange;
  editNumData.offset = 0;
	screen = Screen::EDIT_TEXT;
}


int findIndex(char c) {
	static int sc;
	static int si;
	
	if (sc == c) return si;
	for (int i=0; i<letters.length(); i++) {
		if (letters.charAt(i) == c) {
			sc = c;
			si = i;
			return si;
		}
	}
	return 0;
}


void disp_edit_text(ButtonEnum btn) {
	int i = findIndex(editNumData.s.charAt(editNumData.pos));
	cursor =  editNumData.pos + 2 - editNumData.offset;
	if (btn == BTN_LE) {
		editNumData.pos = dec(editNumData.pos, 0, 99, false);
    if (editNumData.pos < editNumData.offset) 
      editNumData.offset = editNumData.pos;
		return;
	}
	if (btn == BTN_RI) {
		editNumData.pos = inc(editNumData.pos, 0, 99, false);
		if (editNumData.pos >= editNumData.s.length()) editNumData.s += " ";
    if (editNumData.pos - editNumData.offset > 13)
      editNumData.offset = editNumData.pos - 13;
      //Serial << "Position: " << editNumData.pos << ", Offset: " << editNumData.offset << "\n";
		return;
	}
	if (btn == BTN_UP) i = inc(i, 0, letters.length()-1, true);
	else if (btn == BTN_DN) i = dec(i, 0, letters.length()-1, true);
	editNumData.s.setCharAt(editNumData.pos, letters.charAt(i));
	line2.begin();
	line2 = "x>";
	line2 << editNumData.s.substring(editNumData.offset);
	if (btn == BTN_SEL) {
		editNumData.s.trim();
		(editNumData.onChange)();
		cursor = -1;
	}
}


void disp_main_version(ButtonEnum btn) {
	uint32_t ts = Clock.getEpochTime();
	line1.begin();
	line1 << "OnsenPID " << VERSION;
	line2.begin();
	line2 << Clock.getFormattedTime();
	if (btn == BTN_DN) screen = Screen::MAIN_COOK;

}


void disp_main_cook(ButtonEnum btn) {
	switch (sm.getState()) {
		case State::COOKING:
			line1.begin();
			line1 << p.act << " / " << sm.out.set << degc;
			line2.begin();
			line2 << "Noch " << sm.getRemainingTime()/60+1 << " min";
			if (btn == BTN_RI) screen = Screen::COOK_ABORT;
			break;

		case State::WAITING:
			line1 = "Start in";
			line2.begin();
			line2 << sm.getRemainingTime()/60+1 << " min";
			if (btn == BTN_RI) screen = Screen::COOK_ABORT;
			break;

		case State::IDLE:
			line1 = "Kochen...";
			line2.begin();
			if (btn == BTN_RI) screen = Screen::COOK_RECIPE;
			break;

		case State::FINISHED:
			line1 = "Guten Appetit!";
			line2.begin();
			if (btn == BTN_SEL) {
				sm.next();
			}
			break;
	}
	if (btn == BTN_DN) screen = Screen::MAIN_RECIPE;
	else if (btn == BTN_UP) screen = Screen::MAIN_VERSION;
}


void disp_main_recipe(ButtonEnum btn) {
	line1 = combineStrChar("Rezepte", CHAR_UPDN);
	line2 = "bearbeiten...";
	if (btn == BTN_DN) screen = Screen::MAIN_SETTINGS;
	else if (btn == BTN_UP) screen = Screen::MAIN_COOK;
	else if (btn == BTN_RI) screen = Screen::REC_SELECT;
}


void disp_main_settings(ButtonEnum btn) {
	line1 = combineStrChar("Einstellungen...", CHAR_UPDN);
	line2.begin();
	if (btn == BTN_UP) screen = Screen::MAIN_RECIPE;
	else if (btn == BTN_RI) screen = Screen::SET_WIFI;
}


void disp_cook_abort(ButtonEnum btn) {
	line1 = "Abbrechen?";
	line2.begin();
	if (btn == BTN_LE) screen = Screen::MAIN_COOK;
	else if (btn == BTN_SEL) {
		sm.abort();
		screen = Screen::MAIN_COOK;
	}
}


void disp_cook_recipe(ButtonEnum btn) {
	line1 = combineStrChar("Rezept:", CHAR_UPDN);
	line2.begin();
	line2 << recipe[rec_i].name;
	if (btn == BTN_LE) screen = Screen::MAIN_COOK;
	else if (btn == BTN_UP) rec_i = dec(rec_i, 0, REC_COUNT - 1, false);
	else if (btn == BTN_DN) rec_i = inc(rec_i, 0, REC_COUNT - 1, false);
	else if (btn == BTN_RI) screen = Screen::COOK_TIMER;
	else if (btn == BTN_SEL) screen = Screen::COOK_TIMER;
}


void disp_cook_timer(ButtonEnum btn) {
	line1 = combineStrChar("Timer-Modus", CHAR_UPDN);
	line2.begin();
	switch (timer_mode) {
		case 0:
			line2 << "Sofort";
			break;
		case 1:
			line2 << "Fertig um " << secToTime(starttime);
			break;
		case 2:
			line2 << "Start um " << secToTime(starttime);
			break;
		}
	if (btn == BTN_LE) screen = Screen::COOK_RECIPE;
	else if (btn == BTN_RI) screen = Screen::COOK_START;
	else if (btn == BTN_UP) timer_mode = dec(timer_mode, 0, 2, false);
	else if (btn == BTN_DN) timer_mode = inc(timer_mode, 0, 2, false);
	else if (btn == BTN_SEL) {
		if (timer_mode == 0) screen = Screen::COOK_START;
		else {
			start_edit_number(starttime, 0, EditMode::TIME,
			0.0, 86400.0, 300.0, "hh:mm",
			[](){
				// anonymous function, called when edit is complete
				starttime = editNumData.v;
				screen = Screen::COOK_START;
			});
		}
	}
}


void disp_cook_start(ButtonEnum btn) {
	line1 = "Starten?";
	line2.begin();
	if (btn == BTN_LE) screen = Screen::COOK_TIMER;
	else if (btn == BTN_SEL) {
		Logger << "timer mode: " << timer_mode << endl;
		switch(timer_mode) {
			case 0:
				sm.startCooking(rec_i);
				break;
			case 1:
				sm.startByEndTime(rec_i, starttime);
				break;
			case 2:
				sm.startByStartTime(rec_i, starttime);
				break;
		}
		screen = Screen::MAIN_COOK;
	}
}


void disp_rec_select(ButtonEnum btn) {
	line1 = combineStrChar("Rezept:", CHAR_UPDN);
	line2.begin();
	line2 << recipe[rec_i].name;
	if (btn == BTN_UP)       rec_i = dec(rec_i, 0, REC_COUNT-1, false);
	else if (btn == BTN_DN)  rec_i = inc(rec_i, 0, REC_COUNT-1, false);
	else if (btn == BTN_LE)  screen = Screen::REC_EXIT_SAVE;
	else if (btn == BTN_RI)  screen = Screen::REC_NAME;
	else if (btn == BTN_SEL) screen = Screen::REC_NAME;
}


void disp_rec_name(ButtonEnum btn) {
	line1 = "Name:";
	line2.begin();
	line2 << recipe[rec_i].name;
	if (btn == BTN_LE)  screen = Screen::REC_SELECT;
	else if (btn == BTN_RI)  {
		step_i = 0;
		screen = Screen::REC_STEP_i;
	}
	else if (btn == BTN_SEL) {
		start_edit_text(recipe[rec_i].name, EditMode::TEXT,
		[](){
			// anonymous function, called when edit is complete
			strncpy(recipe[rec_i].name, editNumData.s.c_str(), sizeof(recipe[rec_i].name));
			screen = Screen::REC_NAME;
		});
	}
}


void disp_rec_step_i(ButtonEnum btn) {
	line1.begin();
	line1 << CHAR_UPDN << "Schritt " << step_i + 1;
	line2.begin();
	line2 << String(recipe[rec_i].temps[step_i], 1) << degc;
	line2 << ", " << String(recipe[rec_i].times[step_i]/60) << "min";
	if (btn == BTN_SEL) screen = Screen::REC_STEP_i_TEMP;
	else if (btn == BTN_LE)	{
		if (step_i == 0) screen = Screen::REC_NAME;
		else step_i--;
	}
	else if (btn == BTN_RI) {
		if (step_i == 4) screen = Screen::REC_PARAM_i;
		else {
			step_i++;
			screen = Screen::REC_STEP_i;
		}
	}
}


void disp_rec_step_i_temp(ButtonEnum btn) {
	line1.begin();
	line1 << CHAR_UPDN << " #" << step_i+1 << " Temp. :";
	line2.begin();
	line2 << String(recipe[rec_i].temps[step_i], 1) << degc;
	if (btn == BTN_DN)			screen = Screen::REC_STEP_i_TIME;
	else if (btn == BTN_UP)		screen = Screen::REC_STEP_i;
	else if (btn == BTN_LE)	{
		if (step_i == 0) screen = Screen::REC_NAME;
		else {
			step_i--;
			screen = Screen::REC_STEP_i;
		}
	}
		else if (btn == BTN_RI) {
		if (step_i == 4) screen = Screen::REC_PARAM_i;
		else {
			step_i++;
			screen = Screen::REC_STEP_i;
		}
	}
	else if (btn == BTN_SEL)	{
		start_edit_number(recipe[rec_i].temps[step_i], 1, EditMode::NUMBER,
		0.0, 200.0, 0.5, degc,
		[](){
			// anonymous function, called when edit is complete
			recipe[rec_i].temps[step_i] = editNumData.v;
			screen = Screen::REC_STEP_i_TEMP;
		});
	}
}


void disp_rec_step_i_time(ButtonEnum btn) {
	line1.begin();
	line1 << CHAR_UPDN << " #" << step_i+1 << " Zeit:";
	line2.begin();
	line2 << String(recipe[rec_i].times[step_i]/60) << "min";
	if (btn == BTN_UP)		screen = Screen::REC_STEP_i_TEMP;
	else if (btn == BTN_LE)	{
		if (step_i == 0) screen = Screen::REC_NAME;
		else {
			step_i--;
			screen = Screen::REC_STEP_i;
		}
	}
		else if (btn == BTN_RI) {
		if (step_i == 4) screen = Screen::REC_PARAM_i;
		else {
			step_i++;
			screen = Screen::REC_STEP_i;
		}
	}
	else if (btn == BTN_SEL)	{
		start_edit_number(recipe[rec_i].times[step_i] / 60.0, 0, EditMode::NUMBER,
		0.0, 999.0, 1.0, "min",
		[](){
			// anonymous function, called when edit is complete
			recipe[rec_i].times[step_i] = (int)editNumData.v * 60;
			screen = Screen::REC_STEP_i_TIME;
		});
	}
}


void disp_rec_param_i(ButtonEnum btn) {
	static int param_i = 0;
	line1.begin();
	line1 << CHAR_UPDN << pararray[param_i].name << ":";
	line2.begin();
	line2 << String(recipe[rec_i].param[param_i], 1) << " " << pararray[param_i].unit;

	if (btn == BTN_UP)			param_i = dec(param_i, 0, REC_PARAM_COUNT-1, false);
	else if (btn == BTN_DN)		param_i = inc(param_i, 0, REC_PARAM_COUNT-1, false);
	else if (btn == BTN_LE)		screen = Screen::REC_STEP_i;
	else if (btn == BTN_RI)		screen = Screen::REC_EXIT_SAVE;
	else if (btn == BTN_SEL)	{
		start_edit_number(recipe[rec_i].param[param_i], 1, EditMode::NUMBER,
		pararray[param_i].min, pararray[param_i].max, 0.1, pararray[param_i].unit,
		[](){
			// anonymous function, called when edit is complete
			recipe[rec_i].param[param_i] = editNumData.v;
			screen = Screen::REC_PARAM_i;
			Serial << "disp_rec_param_i: edit complete" << endl;
		});
	}
}


void disp_rec_exit_save(ButtonEnum btn) {
	line1 = combineStrChar("Aenderungen", CHAR_UPDN);
	line2.begin();
	line2 << " speichern?";
	if (btn == BTN_LE)			screen = Screen::REC_PARAM_i;
	else if (btn == BTN_DN)		screen = Screen::REC_EXIT_ABORT;
	else if (btn == BTN_SEL)	{
		cfg.save();
		screen = Screen::MAIN_RECIPE;
	}
}


void disp_rec_exit_abort(ButtonEnum btn) {
	line1 = combineStrChar("Aenderungen", CHAR_UPDN);
	line2.begin();
	line2 << " verwerfen?";
	if (btn == BTN_LE)			screen = Screen::REC_PARAM_i;
	else if (btn == BTN_UP)		screen = Screen::REC_EXIT_SAVE;
	else if (btn == BTN_SEL)	{
		cfg.readRecipes();
		screen = Screen::MAIN_RECIPE;
	}
}


void disp_set_wifi(ButtonEnum btn) {
	static int page = 0;
	line1.begin(); 
	line2.begin();
	
	switch (WiFi.getMode()) {
	case (WIFI_OFF):
		line1 << "Offline";
		break;

	case (WIFI_STA):
		if (page == 0) {
			line1 << "Station mode";
			line2 << (WiFi.isConnected() ? "verbunden" : "nicht verbunden");
		} else 
		if (page == 1) {
			line1 << WiFi.SSID();
			line2 << WiFi.localIP();
		} else 
		if (page == 2) {
			line1 << cfg.p.hostname; // WiFi.hostname();
		}
		break;

	case (WIFI_AP_STA):
		line1 << "Station + AP";
		// can't happen, not used
		break;
		
	case (WIFI_AP):
		if (page == 0) {
			line1 << "AP aktiv";
			line2 << WiFi.softAPSSID();
		} else 
		if (page == 1) {
			line1 << WiFi.softAPIP();
			line2 << cfg.p.hostname;
		} else 
		if (page == 2) {
			line1 << "Verbunden: " << WiFi.softAPgetStationNum();
		}
		break;
	}

	if (btn == BTN_DN) page = inc(page, 0, 2, false);
	else if (btn == BTN_UP) page = dec(page, 0, 2, false);
	else if (btn == BTN_RI) {
		page = 0;
		screen = Screen::SET_WIFI_SCAN;
	}
	else if (btn == BTN_LE) {
		page = 0;
		screen = Screen::MAIN_SETTINGS;
	}
}


void disp_set_wifi_scan(ButtonEnum btn) {
	static int netNo = 0;
	int scanState = WiFi.scanComplete();
	switch (scanState) {
		case -2:
			// scan has not been started
			line1 = "WLAN-Netzsuche";
			line2 = "starten?";
			if (btn == BTN_SEL) WiFi.scanNetworks(true, false);
			netNo = 0;
			break;
		case -1:
			// scan running
			line1 = "WLAN-Netzsuche";
			line2 = "laeuft...";
			netNo = 0;
			break;
		default:
			// scan finished
			line1 =  String(scanState) + (scanState == 1 ? " Netz:" : " Netze:");
			line2 = String(netNo) + ": " + WiFi.SSID(netNo);
			if (btn == BTN_UP) netNo = dec(netNo, 0, scanState, false);
			if (btn == BTN_DN) netNo = inc(netNo, 0, scanState-1, false);
			if (btn == BTN_SEL) {
				wifi_ssid = WiFi.SSID(netNo);
				WiFi.scanDelete();
				screen = Screen::SET_WIFI_PW;
			}
			break;
	}
	if (btn == BTN_LE) {
		WiFi.scanDelete();
		screen = Screen::SET_WIFI;
	}
	if (btn == BTN_RI) {
		WiFi.scanDelete();
		screen = Screen::SET_TIMEZONE;
	}
}


void disp_set_wifi_pw(ButtonEnum btn) {
	line1 = "Passwort:";
	line2 = wifi_pw;
	if (btn == BTN_LE) screen = Screen::SET_WIFI_SCAN;
	if (btn == BTN_SEL) 
		start_edit_text(wifi_pw, EditMode::TEXT,
		[](){
			// anonymous function, called when edit is complete
			wifi_pw = editNumData.s;
			screen = Screen::SET_WIFI_SAVE;
		});
}


void disp_set_wifi_save(ButtonEnum btn) {
	static int page = 0;
	switch (page) {
		case 0:
			line1 = "Speichern und";
			line2 = "verbinden?";
			if (btn == BTN_SEL) {
				cfg.p.ssid = wifi_ssid;
				cfg.p.pw = wifi_pw;
				cfg.save();
				start_WiFi();
				screen = Screen::SET_WIFI;
			}
			break;
		case 1: 
			line1 = "Aenderungen";
			line2 = "verwerfen?";
			if (btn == BTN_SEL) {
				screen = Screen::SET_WIFI;
				break;
			}
	}
	if (btn == BTN_UP) page = dec(page, 0, 1, false);
	if (btn == BTN_DN) page = inc(page, 0, 1, false);
	if (btn == BTN_LE) screen = Screen::SET_WIFI_PW;
}


void disp_set_tz(ButtonEnum btn) {
	if ((WiFi.status() == WL_CONNECTED)) {
		// time comes from NTP
		line1 = "Zeitzone:";
		line2.begin();
		line2 << String(cfg.p.tzoffset);
		if (btn == BTN_SEL)
		start_edit_number(cfg.p.tzoffset / 3600, 0, EditMode::NUMBER,
		-12.0, 12.0, 1.0, "h",
		[](){
			// anonymous function, called when edit is complete
			cfg.p.tzoffset = (int)editNumData.v;
			screen = Screen::SET_TIMEZONE;
			Clock.setTimeOffset(cfg.p.tzoffset * 3600);
			cfg.save();
		});
	} else {
		line1 = "Uhr stellen";
		line2.begin();
		line2 << Clock.getFormattedTime();
		if (btn == BTN_SEL)
		start_edit_number(Clock.getEpochTime() % 86400, 0, EditMode::TIME,
		0.0, 86400.0, 60.0, "",
		[](){
			// anonymous function, called when edit is complete
			screen = Screen::SET_TIMEZONE;
			// TODO: Uhr stellen geht nicht
     Serial << "Set clock to " << (int)editNumData.v;
			Clock.setTime((int)editNumData.v);
		});
	}
	if (btn == BTN_LE) screen = Screen::SET_WIFI_SCAN;
}


void disp_set_button(ButtonEnum btn) {
	int ai = analogRead(LCD_AI);
	line1 = "Tasten testen:";
	line2.begin();
	if (ai < 10) line2 << " ";
	if (ai < 100) line2 << " ";
	if (ai < 1000) line2 << " ";
	line2 << ai;
	line2 << " - ";
	switch (btn) {
		case BTN_UP:	line2 << "UP";	break;
		case BTN_DN:	line2 << "DN";	break;
		case BTN_LE:	line2 << "LE";	break;
		case BTN_RI:	line2 << "RI";	break;
		case BTN_SEL:	line2 << "SEL";	break;
		default:		line2 << "---";
	}
}



int inc(int val, int min, int max, bool wrap) {
	val++;
	if (wrap) {
		if (val > max) val = min;
	} else {
		if (val > max) val = max;
	}
	return val;
}

int dec(int val, int min, int max, bool wrap) {
	val--;
	if (wrap) {
		if (val < min) val = max;
	} else {
		if (val < min) val = min;
	}
	return val;
}

// synchronize last used reciepe between web interface and LCD
int get_selected_recipe_no() {
	return rec_i;
}
void set_selected_recipe_no(int no) {
	rec_i = no;
}

// -------------------------------------------------------------------------- Setup
void LCDMenu_setup() {
	init_lcd();
	lcd << "Halte RECHTS";
	lcd.setCursor(0,1);
	lcd << "fuer Tastentest";

	int ai = analogRead(LCD_AI);
	if (ai < 512) screen = Screen::SET_BUTTONS;
}

// -------------------------------------------------------------------------- LCD UI Task
/* read keyboard and write menu */
void LCDMenu_update() {
	static int counter = 0;
	counter++;
	if (counter > 1200) {
		// init display every minute
		counter = 0;
		init_lcd();
	}
	ButtonEnum btn = LCDMenu_readKey();

	switch (screen) {
		case Screen::MAIN_VERSION:		disp_main_version(btn);		break;
		case Screen::MAIN_COOK:			disp_main_cook(btn);		break;
		case Screen::MAIN_RECIPE:		disp_main_recipe(btn);		break;
		case Screen::MAIN_SETTINGS:		disp_main_settings(btn);	break;

		case Screen::COOK_ABORT:		disp_cook_abort(btn);		break;
		case Screen::COOK_RECIPE:		disp_cook_recipe(btn);		break;
		case Screen::COOK_TIMER:		disp_cook_timer(btn);		break;
		case Screen::COOK_START:		disp_cook_start(btn);		break;

		case Screen::REC_SELECT:		disp_rec_select(btn);		break;
		case Screen::REC_NAME:			disp_rec_name(btn);			break;
		case Screen::REC_STEP_i:		disp_rec_step_i(btn);		break;
		case Screen::REC_STEP_i_TEMP:	disp_rec_step_i_temp(btn);	break;
		case Screen::REC_STEP_i_TIME:	disp_rec_step_i_time(btn);	break;
		case Screen::REC_PARAM_i:		disp_rec_param_i(btn);		break;
		case Screen::REC_EXIT_SAVE:		disp_rec_exit_save(btn);	break;
		case Screen::REC_EXIT_ABORT:	disp_rec_exit_abort(btn);	break;

		case Screen::SET_WIFI:			disp_set_wifi(btn);			break;
		case Screen::SET_WIFI_SCAN:		disp_set_wifi_scan(btn);	break;
		case Screen::SET_WIFI_PW:		disp_set_wifi_pw(btn);		break;
		case Screen::SET_WIFI_SAVE:		disp_set_wifi_save(btn);	break;
		case Screen::SET_TIMEZONE:		disp_set_tz(btn);			break;
		case Screen::SET_BUTTONS:		disp_set_button(btn);		break;

		case Screen::EDIT_NUMBER:		disp_edit_number(btn);		break;
		case Screen::EDIT_TEXT:			disp_edit_text(btn);		break;
	}
	if (!((line1 == line1_old) && (line2 == line2_old) && (cursor == cursor_old))) {
		lcd.clear();
		lcd.setCursor(0,0);
		lcd << buf1; //line1;
		lcd.setCursor(0,1);
		lcd << buf2; // line2;
		line1_old.begin(); line1_old << buf1; //line1;
		line2_old.begin(); line2_old << buf2; //line2;
		if (cursor >= 0) {
			lcd.setCursor(cursor, 1);
			lcd.blink();
		} else {
			lcd.noBlink();
		}
		cursor_old = cursor;
	}
}
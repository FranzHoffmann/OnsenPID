#include <Arduino.h>
#include <LiquidCrystal.h>
#include "Process.h"
#include "src/Clock/Clock.h"




Screen screen;
int rec_i, param_i, wifi_i, step_i, timer_mode, starttime;

char buf1[17], buf2[17];
PString line1(buf1, sizeof(buf1));
PString line2(buf2, sizeof(buf2));

static const char degc[2] = {'\1', 'C'};


void LCDMenu_setup() {
	byte char_deg[8]  = {6,  9,  9, 6, 0,  0,  0, 0};
	byte char_updn[8] = {4, 14, 31, 4, 4, 31, 14, 4}; 

	LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
	lcd.begin(16, 2);
	lcd.createChar(1, char_deg);
	lcd.createChar(2, char_updn);
	lcd.clear();
}


ButtonEnum LCDMenu_readKey() {
	static int count = 0;
	static ButtonEnum previous;
	ButtonEnum btn;

	int ai = analogRead(LCD_AI);
	if (ai < 100)      btn = BTN_RI;
	else if (ai < 250) btn = BTN_UP;
	else if (ai < 450) btn = BTN_DN;
	else if (ai < 720) btn = BTN_LE;
	else if (ai < 999) btn = BTN_SEL;
	else               btn = BTN_NONE;

	// once on click, and start running while holding
	if (btn != previous) {
		previous = btn;
		count = 0;
		return btn;
	}
	count++;
	if (count > 100) {
		if (btn == BTN_UP) return BTN_UP_FAST;
		if (btn == BTN_DN) return BTN_DN_FAST;
	}
	return BTN_NONE;
}

/**
 * This screen should be used for any value to be edited.
 * Edit mode should be activated when ENTER is pressed while the value is displayed
 * The value can be edited in line 2
 * The function will not touch line1.
 */
struct {
	double v, vmin, vmax, step;
	int decimals;
	String unit;
	callback onChange;
} editNumData;

void start_edit_number(double value, int decimals,
		double vmin, double vmax, double step, const char *unit,
		callback onChange) {
	editNumData.v = value;
	editNumData.vmin = vmin;
	editNumData.vmin = vmax;
	editNumData.step = step;
	editNumData.unit = String(*unit);
	editNumData.decimals = decimals;
	editNumData.onChange = onChange;
	screen = Screen::EDIT_NUMBER;
}
 
void disp_edit_number(ButtonEnum btn) {
	if (btn == BTN_UP) editNumData.v += editNumData.step;
	else if (btn == BTN_UP) editNumData.v -= editNumData.step;
	limit(editNumData.v, editNumData.vmin, editNumData.vmax);
	line2.begin();
	line2 << String(editNumData.v, editNumData.decimals) << " " << editNumData.unit;
	if (btn == BTN_SEL) (*editNumData.onChange);
}


// -------------------------------------------------------------------------- LCD UI Task
/* read keyboard and write menu */
void LCDMenu_update() {
	ButtonEnum btn = LCDMenu_readKey();

	switch (screen) {
		case Screen::MAIN_VERSION:		disp_main_version(btn);		break;
		case Screen::MAIN_TIME:			disp_main_time(btn);		break;
		case Screen::MAIN_COOK:			disp_main_cook(btn);		break;
		case Screen::MAIN_RECIPE:		disp_main_recipe(btn);		break;
		case Screen::MAIN_SETTINGS:		disp_main_settings(btn);	break;

		case Screen::COOK_ABORT:		disp_cook_abort(btn);		break;
		case Screen::COOK_RECIPE:		disp_cook_recipe(btn);		break;
		case Screen::COOK_TIMER:		disp_cook_timer(btn);		break;
		case Screen::COOK_START:		disp_cook_start(btn);		break;

		case Screen::REC_SELECT:		disp_rec_select(btn);		break;
		case Screen::REC_STEP_i:		disp_rec_step_i(btn);		break;
		case Screen::REC_STEP_i_TEMP:	disp_rec_step_i_temp(btn);	break;
		case Screen::REC_STEP_i_TIME:	disp_rec_step_i_time(btn);	break;
		case Screen::REC_PARAM_i:		disp_rec_param_i(btn);		break;
		case Screen::REC_EXIT_SAVE:		disp_rec_exit_save(btn);	break;
		case Screen::REC_EXIT_ABORT:	disp_rec_exit_abort(btn);	break;

		case Screen::SET_WIFI:			disp_set_wifi(btn);			break;
		case Screen::SET_TIMEZONE:		disp_set_tz(btn);			break;

		case Screen::EDIT_NUMBER:		disp_edit_number(btn);		break;
	}
}


void disp_main_version(ButtonEnum btn) {
	line1 = "OnsenPID";
	line2.begin();
	line2 << "Version " << VERSION; 
	if (btn == BTN_DN) screen = Screen::MAIN_TIME;
	else if (btn == BTN_UP) screen = Screen::MAIN_SETTINGS;
}


void disp_main_time(ButtonEnum btn) {
	uint32_t ts = Clock.getEpochTime();
	line1.begin();
	line1 << Clock.getFormattedTime();
	line2.begin(); //TODO: Datum
	if (btn == BTN_DN) screen = Screen::MAIN_COOK;
	else if (btn == BTN_UP) screen = Screen::MAIN_VERSION;
}


void disp_main_cook(ButtonEnum btn) {
	switch (sm.getState()) {
		case State::COOKING:
			line1.begin();
			line1 << p.act << " / " << sm.out.set << degc;
			line2.begin();
			line2 << "Noch " << sm.getRemainingTime()/60 << " min";
			if (btn == BTN_RI) screen = Screen::COOK_ABORT;
			break;

		case State::WAITING:
			line1 = "Start in";
			line2.begin();
			line2 << sm.getRemainingTime()/60 << " min";
			if (btn == BTN_RI) screen = Screen::COOK_ABORT;
			break;

		case State::IDLE:
			line1 = "Kochen";
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
	else if (btn == BTN_UP) screen = Screen::MAIN_TIME;
}


void disp_main_recipe(ButtonEnum btn) {
	line1 = "Rezepte";
	line2 = "bearbeiten";
	if (btn == BTN_DN) screen = Screen::MAIN_SETTINGS;
	else if (btn == BTN_UP) screen = Screen::MAIN_COOK;
}


void disp_main_settings(ButtonEnum btn) {
	line1 = "Einstellungen";
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
	line1.begin();
	line1 << "Rezept:";
	line2.begin();
	line2 << recipe[rec_i].name;
	if (btn == BTN_LE) screen = Screen::MAIN_COOK;
	else if (btn == BTN_UP) rec_i = (rec_i - 1) % REC_COUNT;
	else if (btn == BTN_DN) rec_i = (rec_i + 1) % REC_COUNT;
	else if (btn == BTN_SEL) screen = Screen::COOK_TIMER;
}


void disp_cook_timer(ButtonEnum btn) {
	line1 = "Timer-Modus";
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
	else if (btn == BTN_UP) timer_mode = (timer_mode - 1) % 3;
	else if (btn == BTN_DN) timer_mode = (timer_mode + 1) % 3;
	else if (btn == BTN_SEL) {
		start_edit_number(starttime,
		0, 0.0, 1440.0, 5.0, "hh:mm",
		[](){
			// anonymous function, called when edit is complete
			starttime = editNumData.v * 60.0;
			screen = Screen::COOK_START;
		});
	}
	// TODO: make a edit screen/function that displays time correctly
}


void disp_cook_start(ButtonEnum btn) {
	if (btn == BTN_LE) screen = Screen::COOK_TIMER;
	else if (btn == BTN_SEL) {
		sm.startCooking(rec_i);
		screen = Screen::MAIN_COOK;
	}
}


void disp_rec_select(ButtonEnum btn) {
	line1 = "Rezept:";
	line2.begin();
	line2 << "Rezept[" << rec_i << "].name";
	if (btn == BTN_UP)       rec_i = (rec_i - 1) % REC_COUNT;
	else if (btn == BTN_DN)  rec_i = (rec_i + 1) % REC_COUNT;
	else if (btn == BTN_LE)  screen = Screen::MAIN_RECIPE;
	else if (btn == BTN_RI)  screen = Screen::REC_NAME;
	else if (btn == BTN_SEL) screen = Screen::REC_NAME;
}

void disp_rec_step_i(ButtonEnum btn) {
	line1.begin();
	line1 << "Schritt " << step_i + 1;
	line2 = "TODO";
	if (btn == BTN_DN) screen = Screen::REC_STEP_i_TEMP;
	else if (btn == BTN_LE)	{
		if (step_i == 0) screen = Screen::REC_NAME;
		else step_i--;
	}
	else if (btn == BTN_RI) {
		if (step_i == 4) screen = Screen::REC_PARAM_i;
		else step_i++;
	}
}


void disp_rec_step_i_temp(ButtonEnum btn) {
	line1.begin();
	line1 << "Schritt " << step_i;
	line2.begin();
	if (btn == BTN_DN)			screen = Screen::REC_STEP_i_TIME;
	else if (btn == BTN_UP)		screen = Screen::REC_STEP_i;
	else if (btn == BTN_SEL)	{
		start_edit_number(recipe[rec_i].temps[step_i],
		1, 20.0, 200.0, 0.5, degc,
		[](){
			// anonymous function, called when edit is complete
			recipe[rec_i].temps[step_i] = editNumData.v;
			screen = Screen::REC_STEP_i_TEMP;
		});
	}
}

void disp_rec_step_i_time(ButtonEnum btn) {
	line1.begin();
	line1 << "Schritt " << step_i;
	line2.begin();
	if (btn == BTN_UP)		screen = Screen::REC_STEP_i_TEMP;
	else if (btn == BTN_SEL)	{
		start_edit_number(recipe[rec_i].times[step_i] / 60.0,
		0, 0.0, 999.0, 1.0, "min",
		[](){
			// anonymous function, called when edit is complete
			recipe[rec_i].times[step_i] = (int)editNumData.v * 60;
			screen = Screen::REC_STEP_i_TIME;
		});
	}
}

void disp_rec_param_i(ButtonEnum btn) {
	line1.begin();
	line1 << pararray[param_i].name << ":";
	line2.begin();
	line1 << "" << recipe[rec_i].param[param_i] << " " << pararray[param_i].unit;

	if (btn == BTN_UP)		param_i = (param_i - 1) % REC_PARAM_COUNT;
	else if (btn == BTN_DN)		param_i = (param_i + 1) % REC_PARAM_COUNT;
	else if (btn == BTN_LE)		screen = Screen::REC_STEP_i;
	else if (btn == BTN_RI)		screen = Screen::REC_EXIT_SAVE;
	else if (btn == BTN_SEL)	{
		start_edit_number(recipe[rec_i].param[param_i],
		1, pararray[param_i].min, pararray[param_i].max, 0.1, pararray[param_i].unit,
		[](){
			// anonymous function, called when edit is complete
			recipe[rec_i].param[param_i] = editNumData.v;
			screen = Screen::REC_PARAM_i;
		});
	}
}

void disp_rec_exit_save(ButtonEnum btn) {
	line1.begin();
	line1 << "Änderungen";
	line2.begin();
	line1 << "speichern?";
	if (btn == BTN_LE)			screen = Screen::REC_PARAM_i;
	else if (btn == BTN_DN)		screen = Screen::REC_EXIT_ABORT;
	else if (btn == BTN_SEL)	{
		cfg.save();
		screen = Screen::REC_SELECT;
	}
}

void disp_rec_exit_abort(ButtonEnum btn) {
	line1.begin();
	line1 << "Änderungen";
	line2.begin();
	line1 << "verwerfen?";
	if (btn == BTN_LE)			screen = Screen::REC_PARAM_i;
	else if (btn == BTN_UP)		screen = Screen::REC_EXIT_SAVE;
	else if (btn == BTN_SEL)	{
		cfg.readRecipes();
		screen = Screen::REC_SELECT;
	}
}

void disp_set_wifi(ButtonEnum btn) {
	line1.begin(); 
	line2.begin();
	switch (wifi_i) {
		 case 0:
			line1 << "WLAN";
			if (p.AP_mode == WIFI_OFFLINE)	line2 << "offline";
			if (p.AP_mode == WIFI_CONN)		line2 << "verbunden";
			if (p.AP_mode == WIFI_APMODE)	line2 << "AP aktiv";
			break;
		case 1:
			line1 << "SSID";
			line2 << cfg.p.ssid;
			break;
		case 2:
			line1 << "Hostname";
			line2 << cfg.p.hostname;
			break;
		case 3:
			line1 << "IP";
			if (p.AP_mode == WIFI_OFFLINE)	line2 << "---";
			if (p.AP_mode == WIFI_CONN)		line2 << WiFi.localIP();
			if (p.AP_mode == WIFI_APMODE)	line2 << WiFi.softAPIP();
	}
	if (btn == BTN_DN) wifi_i = (wifi_i + 1) % 4;
	else if (btn == BTN_UP) wifi_i = (wifi_i - 1) % 4;
	else if (btn == BTN_RI) {
		wifi_i = 0;
		screen = Screen::SET_TIMEZONE;
	}
	else if (btn == BTN_LE) {
		wifi_i = 0;
		screen = Screen::MAIN_SETTINGS;
	}
}

void disp_set_tz(ButtonEnum btn) {
	// TODO
}


/**
 * convert seconds-since-midnight in hh:mm
 */
String secToTime(int sec) {
	int hours = sec / 3600;
	int min = (sec - 3600 * hours) / 60;
	String s;
	s += (hours > 9) ? "" + hours : "0" + hours;
	s += ":";
	s += (min > 9) ? "" + min : "0" + min;
	return s; 
}

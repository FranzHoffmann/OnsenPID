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
	//if (btn == BTN_DN) return Screen::MAIN_VERSION;
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
		// TODO: edit
	}
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
	static bool edit = false;
	static double value;
	// jump to edit screen if editing is active
	if (edit) {
		if (disp_edit_number(btn, value, 20.0, 200.0, 0.5, degc)) {
			// edit finished
			edit = false;
		} else {
			return;
		}
	}
	line1.begin();
	line1 << "Schritt " << step_i;
	line2 = "Temp.: TODO";
	if (btn == BTN_DN)			screen = Screen::REC_STEP_i_TIME;
	else if (btn == BTN_UP)		screen = Screen::REC_STEP_i;
	else if (btn == BTN_SEL)	{
		//start editing
		edit = true;
		value = 0.0; // TODO:recipe[rec_i].temp[step_i]
	}
}

void disp_rec_step_i_time(ButtonEnum btn) {
}

void disp_rec_param_i(ButtonEnum btn) {
}

void disp_rec_exit_save(ButtonEnum btn) {
}

void disp_rec_exit_abort(ButtonEnum btn) {
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

void disp_set_tz(ButtonEnum btn) {}


/**
 * This screen should be used for any value to be edited.
 * Edit mode should be activated when ENTER is pressed while the value is displayed
 * The value can be edited in line 2
 * The function will not touch line1.
 */
bool disp_edit_number(ButtonEnum btn,
	double &value, double vmin, double vmax, double step,
	const char *unit)
{
	if (btn == BTN_UP) value = value + step;
	else if (btn == BTN_UP) value = value - step;
	value = (value > vmax) ? vmax : value;
	value = (value < vmin) ? vmin : value;
	line2.begin();
	line2 << value << " " << unit;
	if (btn == BTN_SEL) {
		return true;
	} else {
		return false;
	}
}

/*
  ButtonEnum b = readKey(A0);
  static int screen;
  
  // generally: left/right switch screen.
  // all other buttons handled seperatuely for each screen
  switch (b) {
    case BTN_RI:
      screen = (screen + 1) % SCREEN_COUNT;
      break;
    case BTN_LE:
      screen = (screen - 1 + SCREEN_COUNT) % SCREEN_COUNT;
      break;
    default:
      ; // do nothing
  }

    switch (screen) {
    case DISP_MAIN:
      line1.begin();
      line1 << "p.act" << " / " << sm.getCookingTemp() << DEGC;
      if (b == BTN_UP) sm.setCookingTemp(sm.getCookingTemp() + 0.1);
      if (b == BTN_DN) sm.setCookingTemp(sm.getCookingTemp() - 0.1);
      
      line2.begin();
      switch (sm.getState()) {
        case State::COOKING:
          line2 << "Noch " << sm.getRemainingTime() / 60 << " min";
          break;
          
        case State::IDLE:
          line2 << "Starten?";
          if (b == BTN_SEL) {
            sm.startCooking();
          }
          break;
          
        case State::FINISHED:
          line2 << "Guten Appetit!";
          if (b == BTN_SEL) {
            sm.next();
          }
          break;

        case State::ERROR:
          line2 << "Störung :-(";
          if (b == BTN_SEL) {
            sm.next();
          }
          break;

        default:
          ;        
      }
      break;

    case DISP_IP:
      line1.begin(); 
      line2.begin();
      switch (p.AP_mode) {
        case WIFI_OFFLINE:
          line1 << "WiFi nicht";
          line2 << "verbunden.";
          break;
          
        case WIFI_APMODE:
          line1 << cfg.p.ssid;
          line2 << WiFi.softAPIP();
          break;
        
        case WIFI_CONN:
          line1 << cfg.p.hostname << ".local";
          line2 << WiFi.localIP();
          break;
      }
      break;

    case DISP_SET_SET:
      if (b == BTN_UP) sm.setCookingTemp(sm.getCookingTemp() + 0.1);
      if (b == BTN_DN) sm.setCookingTemp(sm.getCookingTemp() - 0.1);
      line1.begin();
      line1 << "Solltemperatur:";
      line2.begin();
      line2 << ARROW << sm.getCookingTemp() << DEGC;
      break;

    case DISP_SET_TIME:
      if (b == BTN_UP) sm.setCookingTime(sm.getCookingTime() + 60);
      if (b == BTN_DN) sm.setCookingTime(sm.getCookingTime() + 60);
      line1.begin();
      line1 << "Kochdauer:";
      line2.begin();
      line2 << ARROW << sm.getCookingTime() / 60 << "min";
      break;

    case DISP_SET_KP:
      if (b == BTN_UP) cfg.p.kp += 0.1;
      if (b == BTN_DN) cfg.p.kp -= 0.1;
      line1.begin();
      line1 << "Verstaerkung:";
      line2.begin();
      line2 << ARROW << cfg.p.kp << "%/" << DEGC;
      break;

    case DISP_SET_TN:
      if (b == BTN_UP) cfg.p.tn += 0.1;
      if (b == BTN_DN) cfg.p.tn -= 0.1;
      line1.begin();
      line1 << "Nachstellzeit:";
      line2.begin();
      line2 << ARROW << cfg.p.tn << "s";;
      break;

    case DISP_OUTPUT:
      line1.begin();
      line1 << "Heizleistung:";
      line2.begin();
      line2 << p.out << "%";
      
    default:
    ;  
  }

  return 10;
  */


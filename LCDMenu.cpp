#include "LCDMenu.h"

#include <Arduino.h>
#include <LiquidCrystal.h>
#include "Process.h"
#include "src/Clock/Clock.h"

#define LCD_RS D5
#define LCD_EN D6
#define LCD_D4 D0
#define LCD_D5 D1
#define LCD_D6 D2
#define LCD_D7 D3
#define LCD_AI A0

#define REC_COUNT 5;

LCDMenu::LCDMenu(Process p) :
	line1(buf1, sizeof(buf1)),
	line2(buf2, sizeof(buf2))
{
	process = p;
}


void LCDMenu::setup() {
	byte char_deg[8]  = {6,  9,  9, 6, 0,  0,  0, 0};
	byte char_updn[8] = {4, 14, 31, 4, 4, 31, 14, 4}; 

	LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
	lcd.begin(16, 2);
	lcd.createChar(1, char_deg);
	lcd.createChar(2, char_updn);
	lcd.clear();
}


LCDMenu::ButtonEnum LCDMenu::readKey() {
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
void LCDMenu::update() {
	ButtonEnum btn = readKey();

	line1.begin();
	line2.begin();

	switch (screen) {
		case Screen::MAIN_VERSION:
			display_main_version(btn);
			break;

		case Screen::MAIN_TIME:
			display_main_time(btn);
			break;

		case Screen::MAIN_COOK:
			display_main_cook(btn);
			break;

		case Screen::MAIN_RECIPE:
			display_main_recipe(btn);
			break;

		case Screen::MAIN_SETTINGS:
			display_main_settings(btn);
			break;

		case Screen::REC_SELECT:
			display_rec_select(btn);
			break;

		case Screen::REC_STEP_i:
			display_rec_step_i(btn);
			break;

		case Screen::REC_STEP_i_TEMP:
			display_rec_step_i_temp(btn);
			break;

		case Screen::REC_STEP_i_TIME:
			display_rec_step_i_time(btn);
			break;
	}
	screen = newScreen;
}

void LCDMenu::display_main_version(ButtonEnum btn) {
	line1 << "OnsenPID";
	line2 << "Version 1.0.7"; //TODO
	if (btn == BTN_DN) screen = Screen::MAIN_TIME;
	else if (btn == BTN_UP) screen = Screen::MAIN_SETTINGS;
}

void LCDMenu::display_main_time(ButtonEnum btn) {
	uint32_t ts = Clock.getEpochTime();
	line1 << Clock.getFormattedTime();
	line2 << "TODO: Datum";
	if (btn == BTN_DN) screen = Screen::MAIN_COOK;
	else if (btn == BTN_UP) screen = Screen::MAIN_VERSION;
}

void LCDMenu::display_main_cook(ButtonEnum btn) {
	switch (process.getState()) {
		case State::COOKING:
			line1 << process.getCookingTemp() << degc << "C";;
			line2 << "Noch " << process.getRemainingTime()/60 << " min";
			if (btn == BTN_RI) screen = Screen::COOK_ABORT;
			break;

		case State::WAITING:
			line1 << "Start in";
			line2 << process.getRemainingTime()/60 << " min";
			if (btn == BTN_RI) screen = Screen::COOK_ABORT;
			break;

		case State::IDLE:
			line1 << "Kochen";
			line2 << "";
			if (btn == BTN_RI) screen = Screen::COOK_RECIPE;
			break;

		case State::FINISHED:
			line1 << "Guten Appetit!";
			line2 << "";
			if (btn == BTN_SEL) {
				process.next();
			}
			break;
	}
	if (btn == BTN_DN) screen = Screen::MAIN_RECIPE;
	else if (btn == BTN_UP) screen = Screen::MAIN_TIME;
}

void LCDMenu::display_main_recipe(ButtonEnum btn) {
	line1 << "Rezepte";
	line2 << "bearbeiten";
	if (btn == BTN_DN) screen = Screen::MAIN_SETTINGS;
	else if (btn == BTN_UP) screen = Screen::MAIN_COOK;
}

void LCDMenu::display_main_settings(ButtonEnum btn) {
	line1 << "Einstellungen";
	line2 << "";
	//if (btn == BTN_DN) return Screen::MAIN_VERSION;
	if (btn == BTN_UP) screen = Screen::MAIN_RECIPE;
	else if (btn == BTN_RI) screen = Screen::SET_WIFI;
}

void LCDMenu::display_rec_select(ButtonEnum btn) {
	line1 << "Rezept:";
	line2 << "Rezept[" << rec_i << "].name";
	if (btn == BTN_UP)			rec_i = (rec_i - 1)%REC_COUNT;
	else if (btn == BTN_DN)		rec_i = (rec_i + 1)%REC_COUNT;
	else if (btn == BTN_LE)		screen = Screen::MAIN_RECIPE;
	else if (btn == BTN_RI) 	screen = Screen::REC_NAME;
	else if (btn == BTN_SEL)	screen = Screen::REC_NAME;
}

void LCDMenu::display_rec_step_i(ButtonEnum btn) {
	line1 << "Schritt " << step_i + 1;
	line2 << "TODO";
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

void LCDMenu::display_rec_step_i_temp(ButtonEnum btn) {
	line1 << "Schritt " << step_i
	line2 << "Temp.: TODO"
	if (btn == BTN_DN)			screen = Screen::REC_STEP_i_TIME;
	else if (btn == BTN_UP)		screen = Screen::REC_STEP_i;
	else if (btn == BTN_SEL)	{
		//TODO edit
	}
}

void LCDMenu::display_rec_step_i_time(ButtonEnum btn) {
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


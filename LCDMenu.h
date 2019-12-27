#ifndef lcdmenu_h
#define lcdmenu_h

#include "Process.h"
#include <PString.h>



/* Buttons and menu p.screens */
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


class LCDMenu {

	enum class Screen {
		MAIN_VERSION, MAIN_TIME, MAIN_COOK, MAIN_RECIPE, MAIN_SETTINGS,
		COOK_RECIPE, COOK_TIMER, COOK_START, COOK_ABORT,
		REC_SELECT, REC_NAME, REC_STEP_i, REC_STEP_i_TIME, REC_STEP_i_TEMP,
		REC_PARAM_i, REC_EXIT_SAVE, REC_EXIT_ABORT,
		SET_WIFI, SET_TIMEZONE
	};

	enum ButtonEnum {BTN_NONE, BTN_SEL,
					BTN_UP, BTN_DN, BTN_LE, BTN_RI,
					BTN_UP_FAST, BTN_DN_FAST
	};

public:
	LCDMenu(Process p);				// constructor
	void setup();
	void update();

private:
	void display_main_version(ButtonEnum btn);
	void display_main_time(ButtonEnum btn);
	void display_main_cook(ButtonEnum btn);
	void display_main_recipe(ButtonEnum btn);
	void display_main_settings(ButtonEnum btn);

	void display_rec_select(ButtonEnum btn);
	void display_rec_step_i(ButtonEnum btn);
	void display_rec_step_i_temp(ButtonEnum btn);
	void display_rec_step_i_time(ButtonEnum btn);

	ButtonEnum readKey();
	Screen screen;
	Process process;
	int rec_i, param_i, wifi_i, step_i;
	PString line1, line2;
	char buf1[17], buf2[17];

	static const char degc = '\1';
};


#endif

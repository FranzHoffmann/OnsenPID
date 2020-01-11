#include <LiquidCrystal.h>

#define LCD_RS D5
#define LCD_EN D6
#define LCD_D4 D0
#define LCD_D5 D1
#define LCD_D6 D2
#define LCD_D7 D3
#define LCD_AI A0

#define REC_COUNT 5

enum ButtonEnum {BTN_NONE, BTN_SEL,
				BTN_UP, BTN_DN, BTN_LE, BTN_RI,
				BTN_UP_FAST, BTN_DN_FAST
};

enum class Screen {
	MAIN_VERSION, MAIN_COOK, MAIN_RECIPE, MAIN_SETTINGS,
	COOK_RECIPE, COOK_TIMER, COOK_START, COOK_ABORT,
	REC_SELECT, REC_NAME, REC_STEP_i, REC_STEP_i_TIME, REC_STEP_i_TEMP,
	REC_PARAM_i, REC_EXIT_SAVE, REC_EXIT_ABORT,
	SET_WIFI, SET_WIFI_SCAN, SET_WIFI_PW, SET_WIFI_SAVE,
	SET_TIMEZONE, SET_BUTTONS,
	EDIT_NUMBER, EDIT_TEXT
};


typedef void (*callback)();
enum class EditMode {NUMBER, TIME, TEXT};
extern LiquidCrystal lcd;

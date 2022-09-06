#include <Controller.h>

#include <Logfile.h>
#include <Arduino.h>
#include <Streaming.h>
#include <util.h>
#include <Config.h>
#include <Global.h>

int pwm_port;
uint32_t IRAM_ATTR Ta = 10; 	// ms
uint32_t IRAM_ATTR Tpwm = 1000;	// ms PWM cycle time

IRAM_ATTR struct param_int {
  int32_t set;			// temperature setpoint, from recipe
  int32_t kp, tn, tv, emax, pmax;
  int32_t act;			// actual temperature, from thermometer
  int32_t out;			// actual power, from controller
  int32_t p, i, d, e, q, di;
} ip;

/* PWM and PID controller */
//
// ISR Rules:
// - read access to variables should be no problem
// - write access only to volatile variables
// - only call functions from IRAM
//   floating point library is NO LONGER in IRAM since Arduino core 3.0

// scale everything to int32
void controller_update_old(param &p) {
	noInterrupts();
	ip.act = p.act * 100;		
	ip.set = p.set * 100;		
	ip.emax = p.emax * 100;	
	ip.kp = p.kp * 100;
	ip.tn = p.tn * 1000;
	ip.pmax = p.pmax * 100;	// 1e4 = 100%
	ip.tv = p.tv * 1000;
	p.out = ip.out / 100;
	interrupts();
	// debug
	Logger << "p:" << ip.p / 100 << "%, I=" << ip.i/100 << "+" << ip.di / 100 << "%, D=" << ip.d /100 << "%, E=" << ip.e << ", Q=" << ip.q << endl;
}

static IRAM_ATTR uint32_t iOut;

void controller_update(param &p) {
	// controller - execute once per PWM cycle
	double e = p.set - p.act;
	double ppart, dpart;
	double out;
	static double eold = 0.0; 
	static double ipart = 0.0;
	static unsigned long prev_millis = millis();
	unsigned long act_millis;
	
	act_millis = millis();
	double Ta = (act_millis - prev_millis) / 1000.0;
	prev_millis = act_millis;

	if (abs(e) < abs(p.emax)) {
		// normal PI(D) mode
		ppart = p.kp * e;
		ipart = limit(ipart + 100.0*Ta/p.tn * e, 0.0, p.pmax);
		dpart = p.tv * (e - eold);
	} else {
		// error is very high, use to two-point/controller
		out = (e < 0.0) ? 0.0 : 100.0;
		ppart = 0.0;
		ipart = 0.0;
		dpart = 0.0;
	}
	out = limit(ppart + ipart + dpart, 0.0, p.pmax);
	eold = e;

	iOut = out * 100; // integer output 0-10000 for PWM
	p.out = out;	  // for display
	Serial << "Ta=" << Ta << ", p:" << ppart << "%, I=" << ipart << "%, D=" << dpart << "%, E=" << e << ", Q=" << out << endl;
}

int32_t IRAM_ATTR limitIR(int32_t x, int32_t xmin, int32_t xmax) {
  if (x < xmin) return xmin;
  if (x > xmax) return xmax;
  return x; 
}

void IRAM_ATTR controllerISR_OLD() {
	static int32_t ipart = 0;
	static uint32_t T = 0;
	static int32_t out;
	static int32_t eold = 0;
 
	T += Ta;
	if (T >= Tpwm) {
		T -= Tpwm;
		// controller - execute once per PWM cycle
		int32_t e = ip.set - ip.act;
		int32_t ppart, dpart, di;

		if (abs(e) < abs(ip.emax)) {
			// normal PI(D) mode
			ppart = ip.kp * e / 100;
			//ipart = limit(ipart + 100.0*Ta/p.tn * e, 0.0, p.pmax);
			di = e * 10000 * (int32_t)Ta / ip.tn;
			ipart = limitIR(ipart + di, 0, ip.pmax);
			//dpart = p.tv * (e - eold);
			dpart = p.tv * (e - eold) / 100;
			out = limitIR(ppart + ipart + dpart, 0, ip.pmax);
		} else {
			// error is very high, use to two-point/controller
			out = (e < 0) ? 0 : 10000;
			ppart = 0;
			dpart = 0;
			ipart = 0;
			di = 0;
		}
		eold = e;
		ip.p = ppart;
		ip.d = dpart;
		ip.i = ipart;
		ip.e = e;
		ip.q = out;
		ip.di = di;
	}

	// failsafe
	if (!p.released) {
		ipart = 0;
	}
	if (p.sensorOK && p.released) {
		ip.out = out;
	} else {
		ip.out = 0;
	} 


	// PWM
	boolean bOut = T < (ip.out * Tpwm / 10000);
	digitalWrite(pwm_port, bOut);
}

void IRAM_ATTR controllerISR() {
	static uint32_t T = 0;
	boolean bOut;

	T = (T + Ta) % Ta;
	bOut = 
		p.sensorOK &&
		p.released &&
		T < (iOut * Tpwm / 10000);
	digitalWrite(pwm_port, bOut);
}


void setup_controller() {
	Logger << "Controller: Stelle leistung über Port D" << cfg.p.pwm_port << endl;
	Logger << "Controller: Abtastzeit: " << Ta << "ms" << ", ";
	Logger << "Controller: PWM-Zykluszeit: " << Tpwm << "ms" << endl;
	switch (cfg.p.pwm_port) {
	case 4: 
		pwm_port = D4;
		break;
	case 8:
		pwm_port = D8;
		break;
	default:
		Logger << "Controller: PWM-Port nicht unterstützt" << endl;
	}
	pinMode(pwm_port, OUTPUT);

	// TIM_DIV16: 80MHz/16 = 5MHz, Timer counts up by 5000 every ms
	uint32_t counts = Ta * 5000;
	Logger << "Controller: Setze Interrupt auf " << counts << endl;
	noInterrupts();
	timer1_isr_init();
	timer1_attachInterrupt(controllerISR);
	timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
	timer1_write(counts);
	interrupts();
}
#include <Controller.h>

#include <Logfile.h>
#include <Arduino.h>
#include <Streaming.h>
#include <util.h>
#include <Config.h>
#include <Global.h>

int pwm_port;
uint32_t Ta = 10; 		// ms
uint32_t Tpwm = 1000;	// ms PWM cycle time

IRAM_ATTR struct param_int {
  int32_t set;			// temperature setpoint, from recipe
  int32_t kp, tn, tv, emax, pmax;
  int32_t act;			// actual temperature, from thermometer
  int32_t out;			// actual power, from controller
  int32_t p, i, d;
} ip;

/* PWM and PID controller */
//
// ISR Rules:
// - read access to variables should be no problem
// - write access only to volatile variables
// - only call functions from IRAM
//   floating point library is NO LONGER in IRAM since Arduino core 3.0

// scale everything to int32
void controller_update(param &p) {
	noInterrupts();
	ip.act = p.act * 1000;
	ip.set = p.set * 1000;
	ip.emax = p.emax * 1e6;
	ip.kp = p.kp * 1000;
	ip.tn = p.tn * 1000; // ms
	ip.pmax = p.pmax * 1e6;
	ip.tv = p.tv * 1000;
	p.out = ip.out / 1e6;
	interrupts();
	// debug
	Logger << ip.p << " / " << ip.i << " - " << ip.d << endl;
}


int32_t IRAM_ATTR limitIR(int32_t x, int32_t xmin, int32_t xmax) {
  if (x < xmin) return xmin;
  if (x > xmax) return xmax;
  return x; 
}

void IRAM_ATTR controllerISR() {
	static int32_t ipart = 0.0;
	static uint32_t T = 0.0;
	static int32_t out;
	static int32_t eold = 0.0;
 
	T += Ta;
	if (T >= Tpwm) {
		T -= Tpwm;
		// controller - execute once per PWM cycle
		int32_t e = ip.set - ip.act;
		int32_t ppart, dpart;

		if (abs(e) < abs(ip.emax)) {
			// normal PI(D) mode
			ppart = ip.kp * e;
			//ipart = limit(ipart + 100.0*Ta/p.tn * e, 0.0, p.pmax);
			ipart = limitIR(ipart + 100000 * Ta * e / ip.tn, 0, ip.pmax);
			//dpart = p.tv * (e - eold);
			dpart = p.tv * (e - eold);
			out = limitIR(ppart + ipart + dpart, 0, ip.pmax);
		} else {
			// error is very high, use to two-point/controller
			out = (e < 0) ? 0 : 1000000;
			ipart = 0;
		}
		eold = e;
		ip.p = ppart;
		ip.d = dpart;
		ip.i = ipart;
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
	boolean bOut = T < (ip.out / 1000000 * Tpwm);
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
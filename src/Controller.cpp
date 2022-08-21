#include <Controller.h>

#include <Logfile.h>
#include <Arduino.h>
#include <Streaming.h>
#include <util.h>
#include <Config.h>
#include <Global.h>

int pwm_port;
double Ta = 0.010;
double Tpwm = 1.0;	// PWM cycle time


/* PWM and PID controller */
//
// interrupt routine seems to crash if I call function in Process object?
// so this communicates by global variables in p
//
ICACHE_RAM_ATTR void controllerISR() {
	static double ipart = 0.0;
	static double T = 0.0;
	static double out;
	static double eold = 0.0; 

	T += Ta;
	if (T >= Tpwm) {
		T -= Tpwm;
		// controller - execute once per PWM cycle
		double e = p.set - p.act;
		double ppart, dpart;

		if (abs(e) < abs(p.emax)) {
			// normal PI(D) mode
			ppart = p.kp * e;
			ipart = limit(ipart + 100.0*Ta/p.tn * e, 0.0, p.pmax);
			dpart = p.tv * (e - eold);
			out = limit(ppart + ipart + dpart, 0.0, p.pmax);
		} else {
			// error is very high, use to two-point/controller
			out = (e < 0.0) ? 0.0 : 100.0;
			ipart = 0.0;
		}
		eold = e;
	}

	// failsafe
	if (!p.released) {
		ipart = 0.0;
	}
	if (p.sensorOK && p.released) {
		p.out = out;
	} else {
		p.out = 0.0;
	} 

	// PWM
	boolean bOut = T < (p.out / 100.0 * Tpwm);
	digitalWrite(pwm_port, bOut);

}


void setup_controller() {
	Logger << "Stelle leistung über Port D" << cfg.p.pwm_port << endl;
	Logger << "Abtastzeit: " << Ta*1000.0 << "ms" << ", ";
	Logger << "PWM-Zykluszeit: " << Tpwm*1000.0 << "ms" << endl;
	switch (cfg.p.pwm_port) {
	case 4: 
		pwm_port = D4;
		break;
	case 8:
		pwm_port = D8;
		break;
	default:
		Logger << "PWM-Port nicht unterstützt" << endl;
	}
	pinMode(pwm_port, OUTPUT);

	// TIM_DIV16: 80MHz/16 = 5MHz, Timer counts up every 0.2µs
	unsigned int counts = Ta / 0.2e-6;
	Logger << "Setze Interrupt auf " << counts << endl;
	noInterrupts();
	timer1_isr_init();
	timer1_attachInterrupt(controllerISR);
	timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
	timer1_write(counts);
	interrupts();
}
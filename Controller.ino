int pwm_port;

void setup_controller() {
	switch (cfg.p.pwm_port) {
	case 4: 
		pwm_port = D4;
		break;
	case 8:
		pwm_port = D8;
		break;
	}
	pinMode(pwm_port, OUTPUT);

	noInterrupts();
	timer1_isr_init();
	timer1_attachInterrupt(controllerISR);
	timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
	timer1_write(50000);							// 10ms
	interrupts();
}

/* PWM and PID controller */
//
// interrupt routine seems to crash if I call function in Process object?
// so this communicates by global variables in p
//
ICACHE_RAM_ATTR void controllerISR() {

	// Controller
	static double ipart;
	int ta = 10;
	if (!p.sensorOK) {
		p.out = 0.0;
	} else
	if (p.released) {
		double e = p.set - p.act;
		double ppart = p.kp * e;
		if (abs(e) < abs(p.emax)) {
			ipart = limit(ipart + ta/1000.0/p.tn * e, 0, p.pmax);
			p.out = limit(ppart + ipart, 0.0, p.pmax);
		} else {
			//TODO: ist das richtig?
			p.out = limit( 10.0*e, 0.0, p.pmax);
		}  
	} else {
		p.out = 0.0;
		ipart = 0.0;
	}

	// PWM
	static unsigned long T;
	
	int dt   = 10; // ms
	int Tmax = 1000;
	unsigned long Thi = (p.out / 100.0 * Tmax);
	boolean bOut;
	
	T += dt;
	if (T>=Tmax)  T = 0;
	bOut = T < Thi;
	digitalWrite(pwm_port, bOut);

}

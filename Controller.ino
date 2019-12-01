
void setup_controller() {
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
// so this communicates by global variables in p and cfg.p
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
		double ppart = cfg.p.kp * e;
		if (abs(e) < abs(cfg.p.emax)) {
			ipart = limit(ipart + ta/1000.0/cfg.p.tn * e, 0, 100.0);
			p.out = limit(ppart + ipart, 0.0, 100.0);
		} else {
			p.out = limit( 10.0*e, 0.0, 100.0);
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
	digitalWrite(PWM_PORT, bOut);

}

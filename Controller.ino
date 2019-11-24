
void setup_controller() {
  noInterrupts();
  timer1_isr_init();
  timer1_attachInterrupt(controllerISR);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
  timer1_write(50000);
  interrupts();
}
  
// -------------------------------------------------------------------------- PID Task
/* task: PID controller */
ICACHE_RAM_ATTR void controllerISR() {

  // PWM
  static unsigned long T;
  
  int dt   = 10; // ms
  int Tmax = 1000;
  unsigned long Thi = (p.out / 100.0 * Tmax);

  if (T>=Tmax)  T = 0;
  T += dt;
  p.out_b = T <= Thi;
  if (p.sensorOK) {
    digitalWrite(PWM_PORT, p.out_b);
  } else {
    digitalWrite(PWM_PORT, false);    
  }

  // Controller
  static double ipart;
  int ta = 10;

  if (p.state == STATE_COOKING) {
    double e = p.set - p.act;
    double ppart = cfg.p.kp * e;
    if (abs(e) < abs(cfg.p.emax)) {
      ipart = limit(ipart + ta/1000.0/cfg.p.tn * e, 0, 100.0);
      p.out = limit(ppart + ipart, 0.0, 100.0);
    } else {
      p.out = limit( 10.0*e, 0.0, 100.0);
    }

  } else {
    ipart = 0.0;
    p.out = 0.0;
  }  
}

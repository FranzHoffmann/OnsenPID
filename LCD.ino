ButtonEnum readKey(int port) {
  static ButtonEnum previous;
  static ButtonEnum pressed;
  static int count = 0;

  int ai = analogRead(port);
  if (ai < 100)      pressed = BTN_RI;
  else if (ai < 250) pressed = BTN_UP;
  else if (ai < 450) pressed = BTN_DN;
  else if (ai < 720) pressed = BTN_LE;
  else if (ai < 999) pressed = BTN_SEL;
  else               pressed = BTN_NONE;

  // once on click, and start running while holding
  if (pressed != previous) {
    previous = pressed;
    count = 0;
    return pressed;
  }
  count++;
  if (count > 100) return pressed;
  else return BTN_NONE;
}
// -------------------------------------------------------------------------- LCD UI Task
/* read keyboard and write menu */
int task_keyboard() {
  ButtonEnum b = readKey(A0);

  // generally: left/right switch p.screen.
  // all other buttons handled seperatuely for each p.screen
  switch (b) {
    case BTN_RI:
      p.screen = (p.screen + 1) % SCREEN_COUNT;
      break;
    case BTN_LE:
      p.screen = (p.screen - 1 + SCREEN_COUNT) % SCREEN_COUNT;
      break;
    default:
      ; // do nothing
  }

    switch (p.screen) {
    case DISP_MAIN:
      line1.begin();
      line1 << p.act << " / " << sm.getCookingTemp() << DEGC;
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
}

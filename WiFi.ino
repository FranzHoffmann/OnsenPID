WiFiEventHandler OnConnected, OnGotIp, OnDisconnected;
WiFiEventHandler OnStationConnected, OnStationDisconnected;

// ---------------------------------------------------------- setup WiFi
void start_WiFi() {
  Logger << "WiFi-Configuration:" << endl;
  WiFi.printDiag(Logger);
 
	/* callback when connected in station mode (when we get an IP) */
	OnConnected = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& event) {
		Logger << "WiFi verbunden" << endl;
	});

	/* callback when connected in station mode (when we get an IP) */
	OnGotIp = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& event) {
		Logger << "IP erhalten: " << (WiFi.localIP()) << endl;
	});
  
	/* callback when we get disconnected (in station mode) */
	OnDisconnected = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) {
		Logger << "WiFi-Verbindung verloren" << endl;
	});

	/* callback when somebody connects (in AP mode) */
	OnStationConnected = WiFi.onSoftAPModeStationConnected([](const WiFiEventSoftAPModeStationConnected& event) {
		Logger << "Client " << macToString(event.mac) << " verbunden" << endl;
	});

	/* callback when somebody disconnects (in AP mode) */
	OnStationDisconnected = WiFi.onSoftAPModeStationDisconnected([](const WiFiEventSoftAPModeStationDisconnected& event) {
		Logger << "Client " << macToString(event.mac) << " getrennt" << endl;
	});

  Logger << "Versuche verbindung mit '" << cfg.p.ssid << "'" << endl;
  WiFi.mode(WIFI_STA);
  WiFi.hostname(cfg.p.hostname);
  WiFi.begin(cfg.p.ssid, cfg.p.pw);
  for (int i=0; i<100; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      Logger << "WiFi verbunden (IP: " << WiFi.localIP() << ")" << endl;
      return;
    }
    delay(100);
  }
  Logger << "WiFi-Verbindung fehlgeschlagen" << endl;

  // open AP
  uint8_t macAddr[6];
  String ssid = "Onsenei-";
  WiFi.softAPmacAddress(macAddr);
  for (int i = 4; i < 6; i++) {
    ssid += String(macAddr[i], HEX);
  }
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid);
  Logger << "Access Point erstellt: '" << ssid << "'" << endl;
  WiFi.printDiag(Logger);

  return;
}

String macToString(const unsigned char* mac) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

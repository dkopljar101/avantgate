// main.ino
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// Physical buttons
#define BUTTON_PIN      0    // K1 enters Setup Mode
#define BUTTON_HOLD_MS 4000  // hold time in ms

// Global objects and settings
Preferences prefs;
WiFiClientSecure secureClient;
PubSubClient mqtt(secureClient);
bool setupMode = false;

// MQTT prefix and identifiers
static const char* tPrefix = "avant/";
static const char* gateName = "avant0001";
static const char* defaultCACert = R"EOF(
-----BEGIN CERTIFICATE-----
...PEM CONTENT...
-----END CERTIFICATE-----
)EOF";

// Forward declarations
void checkButton();
bool hasConfig();
void startSetupMode();
void startNormalMode();
void listSPIFFS();
void mqttCallback(char* topic, uint8_t* payload, unsigned int length);

// MQTT callback implementation
auto mqttCallbackPtr = mqttCallback;
void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
  Serial.print("MQTT Message on topic: ");
  Serial.println(topic);
  Serial.print("Payload: ");
  for (unsigned int i = 0; i < length; i++) Serial.print((char)payload[i]);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Mount SPIFFS and list existing files
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
    return;
  }
  listSPIFFS();

  prefs.begin("cfg", false);
  checkButton();

  if (!hasConfig() || setupMode) {
    startSetupMode();
  } else {
    startNormalMode();
  }
}

void loop() {
  // In normal mode, maintain MQTT connection
  if (WiFi.getMode() != WIFI_AP) {
    if (!mqtt.connected()) {
      startNormalMode();
    }
    mqtt.loop();
  }
  delay(1); // feed RTOS watchdog
}

bool hasConfig() {
  return prefs.getString("ssid", "").length()
      && prefs.getString("pass", "").length()
      && prefs.getString("mqttHost", "").length()
      && prefs.getUInt("mqttPort", 0) > 0
      && prefs.getString("mqttUser", "").length()
      && prefs.getString("mqttPass", "").length()
      && prefs.getUInt("PULSE_ds", 0) > 0;
}

void startSetupMode() {
  WiFi.softAP("Device-Setup", "setup123");
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
  WebServer server(80);

  // Serve configuration page
  server.on("/", HTTP_GET, [&server]() {
    // Load existing values
    String ssidV = prefs.getString("ssid", "");
    String passV = prefs.getString("pass", "");
    String hostV = prefs.getString("mqttHost", "");
    String portV = String(prefs.getUInt("mqttPort", 8883));
    String userV = prefs.getString("mqttUser", "");
    String passM = prefs.getString("mqttPass", "");
    String pulseV= String(prefs.getUInt("PULSE_ds", 10));
    String caV;
    if (SPIFFS.exists("/ca_cert.pem")) {
      File f = SPIFFS.open("/ca_cert.pem","r"); caV = f.readString(); f.close();
    } else caV = defaultCACert;

    // Build HTML
    String html = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><title>Setup</title><style>
body{font:16px Arial;margin:2em;}nav a{margin:0 1em;color:#007BFF;cursor:pointer;text-decoration:none}nav a.active{font-weight:bold;text-decoration:underline}section{display:none;margin-top:1em}section.active{display:block}form{margin-bottom:2em;padding:1em;border:1px solid #ccc;border-radius:8px}label{display:block;margin:0.5em 0}input,textarea{width:100%;padding:0.4em;border:1px solid #aaa;border-radius:4px}button{margin-top:0.5em;padding:0.5em 1em;border:none;border-radius:4px;background:#007BFF;color:#fff;cursor:pointer}button.reset{background:#dc3545}button:hover{opacity:0.9}</style></head><body>
<h2>Configuration Wizard</h2>
<nav><a id="link-wifi" class="active" onclick="showStep('wifi')">WiFi</a><a id="link-mqtt" onclick="showStep('mqtt')">MQTT</a><a id="link-cert" onclick="showStep('cert')">Certificates</a></nav>
)rawliteral";
    // WiFi section
    html += "<section id='wifi' class='active'><form action='/save-wifi' method='POST'>";
    html += "<label>SSID<input name='ssid' value='"+ssidV+"'></label>";
    html += "<label>Password<input name='pass' value='"+passV+"'></label>";
    html += "<button type='submit'>Save WiFi</button><button type='button' class='reset' onclick=\"location='/reset'\">Reset</button></form></section>";
    // MQTT section
    html += "<section id='mqtt'><form action='/save-mqtt' method='POST'>";
    html += "<label>Host<input name='mqttHost' value='"+hostV+"'></label>";
    html += "<label>Port<input name='mqttPort' value='"+portV+"'></label>";
    html += "<label>User<input name='mqttUser' value='"+userV+"'></label>";
    html += "<label>Pass<input name='mqttPass' value='"+passM+"'></label>";
    html += "<label>Pulse<input name='pulse' value='"+pulseV+"'></label>";
    html += "<button type='submit'>Save MQTT</button><button type='button' class='reset' onclick=\"location='/reset'\">Reset</button></form></section>";
    // Cert section
    html += "<section id='cert'><form action='/save-cert' method='POST'>";
    html += "<label>CA<textarea name='caCert' rows='8'>"+caV+"</textarea></label>";
    html += "<button type='submit'>Save Cert</button><button type='button' class='reset' onclick=\"location='/reset'\">Reset</button></form></section>";
    html += R"rawliteral(<script>function showStep(s){document.querySelectorAll('nav a').forEach(a=>a.classList.remove('active'));document.querySelectorAll('section').forEach(sec=>sec.classList.remove('active'));document.getElementById('link-'+s).classList.add('active');document.getElementById(s).classList.add('active')}</script></body></html>)rawliteral";
    server.send(200, "text/html", html);
  });

  // Save WiFi
  server.on("/save-wifi", HTTP_POST, [&server](){
    prefs.putString("ssid", server.arg("ssid"));
    prefs.putString("pass", server.arg("pass"));
    server.send(200, "text/plain", "WiFi settings saved");
  });
  // Save MQTT
  server.on("/save-mqtt", HTTP_POST, [&server](){
    prefs.putString("mqttHost", server.arg("mqttHost"));
    prefs.putUInt("mqttPort", server.arg("mqttPort").toInt());
    prefs.putString("mqttUser", server.arg("mqttUser"));
    prefs.putString("mqttPass", server.arg("mqttPass"));
    prefs.putUInt("PULSE_ds", server.arg("pulse").toInt());
    server.send(200, "text/plain", "MQTT settings saved");
  });
  // Save Cert
  server.on("/save-cert", HTTP_POST, [&server](){
    File f = SPIFFS.open("/ca_cert.pem","w"); if(f){f.print(server.arg("caCert")); f.close();}
    server.send(200, "text/plain", "Certificate saved");
  });

  // Reset
  server.on("/reset", HTTP_GET, [&server](){
    server.send(200, "text/plain", "Rebooting..."); delay(100); ESP.restart();
  });

  server.begin();
  while (WiFi.getMode() == WIFI_AP) { server.handleClient(); delay(1); }
}

void startNormalMode() {
  // Connect to WiFi
  WiFi.begin(prefs.getString("ssid").c_str(), prefs.getString("pass").c_str());
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) delay(100);
  if (WiFi.status() != WL_CONNECTED) { listSPIFFS(); ESP.restart(); }

  // Load CA cert
  if (SPIFFS.exists("/ca_cert.pem")) {
    File f = SPIFFS.open("/ca_cert.pem","r"); secureClient.setCACert(f.readString().c_str()); f.close();
  } else secureClient.setCACert(defaultCACert);

  // Setup MQTT
  mqtt.setCallback(mqttCallback);
  mqtt.setServer(prefs.getString("mqttHost").c_str(), prefs.getUInt("mqttPort"));
  while (!mqtt.connected()) {
    if (mqtt.connect(gateName, prefs.getString("mqttUser").c_str(), prefs.getString("mqttPass").c_str())) {
      mqtt.publish((String(tPrefix)+"info").c_str(), "online");
      mqtt.subscribe((String(tPrefix)+gateName+"/cmd/#").c_str());
    } else delay(5000);
  }
  Serial.println("MQTT connected");
}

void checkButton() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    unsigned long t0 = millis();
    while (digitalRead(BUTTON_PIN) == LOW) if (millis() - t0 > BUTTON_HOLD_MS) { setupMode = true; break; }
  }
}

void listSPIFFS() {
  Serial.println("SPIFFS files:");
  File root = SPIFFS.open("/"); File f = root.openNextFile();
  while (f) { Serial.println(f.name()); f = root.openNextFile(); }
}

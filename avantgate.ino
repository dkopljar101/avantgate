// avantgate.ino
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// Physical buttons
#define BUTTON_PIN      0    // K1 enters Setup Mode
#define BUTTON_HOLD_MS 4000  // hold time in ms

// Relay control
#define RELAY1      2    // GPIO pin for relay 1 control
#define RELAY2      4    // GPIO pin for relay 2 control
#define PULSE_DEFAULT 70  // relay pulse time in 1/10 seconds

// Global objects and settings
Preferences prefs;
WiFiClientSecure secureClient;
PubSubClient mqtt(secureClient);
bool setupMode = false;
// Global config variables
String wifiSsid, wifiPass, mqttHost, mqttUser, mqttPass;
String caCert;
uint16_t mqttPort;
unsigned int PULSE_ds = PULSE_DEFAULT;


// MQTT prefix and identifiers
static const char* tPrefix = "avant/";
static const char* gateName = "avant0001";
// Global CA certificate buffer (empty by default)
// LetsEncrypt Root CA as default certificate
static const char* defaultCACert = R"EOF(-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

// Forward declarations
void checkButton();
bool hasConfig();
void startSetupMode();
void startNormalMode();
void listSPIFFS();
void mqttCallback(char* topic, uint8_t* payload, unsigned int length);
void pulseRelay(uint8_t gpioPin); // pulses relay for PULSE_ds

// Load all preferences into global variables
void loadPrefs() {
  wifiSsid  = prefs.getString("ssid", "");
  wifiPass  = prefs.getString("pass", "");
  mqttHost  = prefs.getString("mqttHost", "");
  mqttPort  = prefs.getUInt("mqttPort", 8883);
  mqttUser  = prefs.getString("mqttUser", "");
  mqttPass  = prefs.getString("mqttPass", "");
  PULSE_ds  = prefs.getUInt("PULSE_ds", PULSE_DEFAULT);
  if (PULSE_ds == 0) PULSE_ds = PULSE_DEFAULT;
  
  // Load CA certificate from SPIFFS or use default
  if (SPIFFS.exists("/ca_cert.pem")) {
    File f = SPIFFS.open("/ca_cert.pem", "r");
    caCert = f.readString();
    f.close();
  } else {
    caCert = defaultCACert;
  }

}

// MQTT callback implementation
void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
  Serial.print("MQTT Message on topic: "); Serial.println(topic); // for debugging
  // parse Topic
  String tStr = String(topic);
  int cmdIdx = tStr.indexOf("/cmd/");
  String wTopic;
  if (cmdIdx >= 0) {
    wTopic = tStr.substring(cmdIdx + 5); // 5 is the length of "/cmd/"
    wTopic.trim(); // remove any leading/trailing whitespace
    Serial.print("wTopic: "); Serial.println(wTopic);
    if (wTopic.equals("opengate1")) {
      Serial.println("Opening Gate 1");
      pulseRelay(RELAY1);
    } else if (wTopic.equals("opengate2")) {
      Serial.println("Opening Gate 2");
      pulseRelay(RELAY2);
    } else {
      Serial.print("Unknown command: ");
      Serial.println(wTopic);
    }
  } else {
    Serial.println("wTopic: '/cmd/' not found in topic");
  }
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
  loadPrefs(); // <--- load all config values
  Serial.println("Loaded preferences:");
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
  // Check if all required configuration parameters are set
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
    String caV = caCert;
    String ssidV = wifiSsid:
    String passV = wifiPass;
    String hostV = mqttHost;
    String portV = String(mqttPort);
    String userV = mqttUser;
    String passM = mqttPass;
    String pulseV = String(PULSE_ds);
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

  // MQTT set CA cert
  secureClient.setCACert(caCert.c_str());
  
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

// Function to pulse a relay: turns GPIO on, waits PULSE_ds*100 ms, then turns it off
void pulseRelay(uint8_t gpioPin) {
  pinMode(gpioPin, OUTPUT);
  digitalWrite(gpioPin, HIGH);
  delay(PULSE_ds*100); // PULSE_ds is in 1/10 seconds, so multiply by 100 for milliseconds
  digitalWrite(gpioPin, LOW);
}

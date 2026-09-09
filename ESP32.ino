#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <FS.h>
#include <SPIFFS.h>

#define CONFIG_BUTTON 14  // You can change to another available pin
#define buzzer_Pin 26
#define DHTPIN 13
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
#define Gas_Pin_1 18
#define Gas_Pin_2 19
#define Flame_Pin_1 27
#define Flame_Pin_2 25

LiquidCrystal_I2C lcd(0x27, 20, 4);

String deviceId = "matSci";
char serverUrl[100] = "https://sensor-backened.onrender.com";
char tempThresholdStr[6] = "32";
int tempThreshold = 32;

WiFiManagerParameter* custom_url;
WiFiManagerParameter* custom_threTemp;
bool alertTempSent = false, alertGas1Sent = false, alertGas2Sent = false, alertFlameSent = false;

float lastTemp = -1000, lastHum = -1000;
int lastGas1 = HIGH, lastGas2 = HIGH, lastFlame = HIGH;

void loadConfig() {
  if (!SPIFFS.begin(true)) return;
  File configFile = SPIFFS.open("/config1.json", "r");
  if (!configFile) return;
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, configFile)) return;
  configFile.close();
  strlcpy(serverUrl, doc["serverUrl"] | serverUrl, sizeof(serverUrl));
  tempThreshold = doc["tempThreshold"] | 32;
}

void saveConfig() {
  StaticJsonDocument<256> doc;
  doc["serverUrl"] = serverUrl;
  doc["tempThreshold"] = tempThreshold;
  File configFile = SPIFFS.open("/config1.json", "w");
  if (configFile) {
    serializeJson(doc, configFile);
    configFile.close();
  }
}

void sendPush(const String &body) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(String(serverUrl) + "/notify");
    http.addHeader("Content-Type", "application/json");
    StaticJsonDocument<200> doc;
    doc["title"] = "Alert";
    doc["body"] = body;
    doc["deviceId"] = deviceId;
    String json;
    serializeJson(doc, json);
    http.POST(json);
    http.end();
  }
}

void sendAllToServer(float temp, float hum, int gas1, int gas2, int flame) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(String(serverUrl) + "/data");
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<256> doc;
    doc["deviceId"] = deviceId;
    doc["temp"] = temp;
    doc["hum"] = hum;
    doc["gas_1"] = (gas1 == LOW ? "Gas Detected" : "Gas Not Detected");
    doc["gas_2"] = (gas2 == LOW ? "Gas Detected" : "Gas Not Detected");
    doc["flame"] = (flame == LOW ? "Flame Detected" : "Flame Not Detected");

    String json;
    serializeJson(doc, json);
    http.POST(json);
    http.end();
  }
}


void setup() {
  Serial.begin(115200);
  pinMode(buzzer_Pin,OUTPUT);
  pinMode(Gas_Pin_1, INPUT);
  pinMode(Gas_Pin_2, INPUT);
  pinMode(Flame_Pin_1, INPUT);
  pinMode(Flame_Pin_2, INPUT);
  pinMode(CONFIG_BUTTON, INPUT_PULLUP);

  // Initialize I2C, LCD, DHT, etc. here
  // Wire.begin(21, 22);
  // lcd.init();
  // lcd.backlight();

  loadConfig();

  snprintf(tempThresholdStr, sizeof(tempThresholdStr), "%d", tempThreshold);
  custom_url = new WiFiManagerParameter("server", "Server URL", serverUrl, 100);
  custom_threTemp = new WiFiManagerParameter("threshold", "Threshold Temp (°C)", tempThresholdStr, 6);

  WiFiManager wm;
  wm.addParameter(custom_url);
  wm.addParameter(custom_threTemp);

  bool buttonPressed = digitalRead(CONFIG_BUTTON) == LOW;

  if (buttonPressed || !wm.autoConnect("EnvMonitor-Setup")) {
    Serial.println("⚙️ Entering config portal...");
    if (wm.startConfigPortal("EnvMonitor-Setup")) {

      strlcpy(serverUrl, custom_url->getValue(), sizeof(serverUrl));
      int newThresh = atoi(custom_threTemp->getValue());
      if (newThresh > 0) tempThreshold = newThresh;
      saveConfig();

      delay(1000);
      ESP.restart();
    } else {
      Serial.println("Config portal failed");
      delay(2000);
      ESP.restart();
    }
  }
  Serial.println("Server URL: " + String(serverUrl));
  Serial.print("Temp Threshold: "); Serial.println(tempThreshold);

  dht.begin();
}

void loop() {
 float temp = random(250, 450) / 10.0;   // 25.0 to 45.0 °C
 float hum = random(400, 800) / 10.0;    // 40.0 to 80.0 %

 int gas1 = random(0, 2);  // Random 0 (LOW) or 1 (HIGH)
 int gas2 = random(0, 2);
  int flame = random(0, 2);
    // float temp = dht.readTemperature();
    // float hum = dht.readHumidity();
    // int gas1 = digitalRead(Gas_Pin_1) == LOW|| digitalRead(Gas_Pin_2) == LOW ? 0 : 1 ;
    // int gas2 = 0;
    // int flame = digitalRead(Flame_Pin_1) == LOW || digitalRead(Flame_Pin_2) == LOW ? 0 : 1 ;
  Serial.println(gas1);
  Serial.println(flame);


  // bool changed = false;
  // String alertMessage = "";

  // if (abs(temp - lastTemp) > 0.2) { lastTemp = temp; changed = true; }
  // if (abs(hum - lastHum) > 0.5) { lastHum = hum; changed = true; }
  // if (gas1 != lastGas1) { lastGas1 = gas1; changed = true; }
  // if (gas2 != lastGas2) { lastGas2 = gas2; changed = true; }
  // if (flame != lastFlame) { lastFlame = flame; changed = true; }

  if (true) sendAllToServer(temp, hum, gas1, gas2, flame);

  // Temp Alert
  if (temp > tempThreshold && !alertTempSent) {
    // digitalWrit(26,HIGH);
    sendPush("Temp: " + String(temp, 1) + "°C");
    alertTempSent = true;
  } else if (temp < (tempThreshold - 1)) {
    // digitalWrit(26,LOW);
    alertTempSent = false;
  }

  // Gas1 Alert
  if (gas1 == LOW && !alertGas1Sent) {
    digitalWrite(26,HIGH);
    sendPush("Gas1: Detected");
    alertGas1Sent = true;
  } else if (gas1 == HIGH){ 
    digitalWrite(26,LOW);
    alertGas1Sent = false;
  }

  // Gas2 Alert
  if (gas2 == LOW && !alertGas2Sent) {
    digitalWrite(26,HIGH);
    sendPush("Gas2: Detected");
    alertGas2Sent = true;
  } else if (gas2 == HIGH){ 
    digitalWrite(26,LOW);
    alertGas2Sent = false;
    }

  // Flame Alert
  if (flame == LOW && !alertFlameSent) {
    digitalWrite(26,HIGH);
    sendPush("Flame: Detected");
    alertFlameSent = true;
  } else if (flame == HIGH){
    digitalWrite(26,LOW);
    alertFlameSent = false;
  }

  // lcd.setCursor(0,0); lcd.print("Temp: "); lcd.print(temp,1); lcd.print((char)223); lcd.print("C   ");
  // lcd.setCursor(0,1); lcd.print("Hum : "); lcd.print(hum,1); lcd.print("%     ");
  // lcd.setCursor(0,2); lcd.print("Gas1: "); lcd.print(gas1 == LOW ? "YES" : "No ");
  // lcd.setCursor(10,2); lcd.print("Gas2: "); lcd.print(gas2 == LOW ? "YES" : "No ");
  // lcd.setCursor(0,3); lcd.print("Flame: "); lcd.print(flame == LOW ? "YES" : "No ");
  
  delay(1000);
}

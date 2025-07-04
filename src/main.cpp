//mqtt ausgaben erweitert; Flaschenauswahl optimiert

#include <Arduino.h>

#include <WiFi.h>
#include <WebServer.h>
#include <HX711.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "mainpage_template.h"

const char* HTML_MENU =
  "<nav>"
  "<a href='/'>Status</a> | "
  "<a href='/kalibrierung'>Kalibrierung</a> | "
  "<a href='/tara'>Tara</a> | "
  "<a href='/tempkomp'>Temp.Komp.</a> | "
  "<a href='/wifi'>WLAN</a> | "
  "<a href='/mqtt'>MQTT</a>"
  "</nav>";


#define DT1 18
#define SCK1 19
#define DT2 21
#define SCK2 22

#define ONE_WIRE_BUS 4  // z. B. GPIO 4 am ESP32
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

HX711 scale1;
HX711 scale2;
Preferences prefs;
WebServer server(80);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

float temperatureC = 20.0;
bool tempSensorOK = false;

//neu sekündlich lesen:
double g1_aktuell = 0, g2_aktuell = 0;
bool e1_aktuell = false, e2_aktuell = false;
unsigned long lastSensorUpdate = 0;

double sensor1_r0 = 0, sensor1_r1 = 1, sensor1_g1 = 100;
double sensor1_tara = 0;
double sensor2_r0 = 0, sensor2_r1 = 1, sensor2_g1 = 100;
double sensor2_tara = 0;
bool sensor1_error = false, sensor2_error = false;

int bottle1 = 11000, bottle2 = 11000;
float komp_t1_1 = 0, komp_g1_1 = 0, komp_t2_1 = 0, komp_g2_1 = 0;
float komp_t1_2 = 0, komp_g1_2 = 0, komp_t2_2 = 0, komp_g2_2 = 0;
double g1_roh = 0, g2_roh = 0;

char ssid[32] = "";
char password[32] = "";
char mqttServer[64] = "";
int mqttPort = 1883;
char mqttTopic[64] = "sensor1/gewicht";
bool apMode = false;
const char* apSSID = "Waage-Setup";
const char* apPassword = "12345678";
unsigned long lastMqttSent = 0;

//neu sekündlich lesen:
//long raw = s.read();
double calcGewicht(HX711& s, double r0, double r1, double g1, double tara, bool& err) {
  long raw = 0;
  const int samples = 9;
  for (int i = 0; i < samples; i++) {
    raw += s.read();
    delay(5);
  }
  raw /= samples;

  if (raw == 8388607 || raw == -8388608) {
    err = true;
    return 0.0;
  }
  err = false;
  double scale = (r1 != r0) ? (g1 / (r1 - r0)) : 0;
  double gewicht = (raw - r0) * scale - tara;
  return gewicht < 0 ? 0 : gewicht;
}
float kompKorrektur(float t0, float g0, float t1, float g1, float tIst) {
  if (abs(t1 - t0) < 0.1) return 0.0;  // vermeidet Division durch 0
  float m = (g1 - g0) / (t1 - t0);
  return (tIst - t0) * m;
}
// 2. updateGewicht ebenfalls separat
void updateGewicht() {
  bool dummy;
  g1_roh = calcGewicht(scale1, sensor1_r0, sensor1_r1, sensor1_g1, sensor1_tara, dummy);  // ohne Komp.
float komp1 = (tempSensorOK) ? kompKorrektur(komp_t1_1, 0, komp_t2_1, komp_g2_1 - komp_g1_1, temperatureC) : 0;
g1_aktuell = g1_roh - komp1;  g1_aktuell = g1_roh - komp1;

  g2_roh = calcGewicht(scale2, sensor2_r0, sensor2_r1, sensor2_g1, sensor2_tara, dummy);
float komp2 = (tempSensorOK) ? kompKorrektur(komp_t1_2, 0, komp_t2_2, komp_g2_2 - komp_g1_2, temperatureC) : 0;
g2_aktuell = g2_roh - komp2;  g2_aktuell = g2_roh - komp2;
}

/*
  void publishMqttData() {
  if (!apMode && mqttClient.connected()) {
    double g1 = calcGewicht(scale1, sensor1_r0, sensor1_r1, sensor1_g1, sensor1_tara, sensor1_error);
    double g2 = calcGewicht(scale2, sensor2_r0, sensor2_r1, sensor2_g1, sensor2_tara, sensor2_error);
    long raw1 = scale1.read();
    long raw2 = scale2.read();

    char topic[128], payload[32];

    // --- LINKS ---
    if (!sensor1_error) {
      snprintf(topic, sizeof(topic), "%s/links/Gasgewicht", mqttTopic);
      snprintf(payload, sizeof(payload), "%.2f", g1);
      mqttClient.publish(topic, payload, true);

      snprintf(topic, sizeof(topic), "%s/links/GasinhaltProzent", mqttTopic);
      snprintf(payload, sizeof(payload), "%.1f", (g1 / bottle1) * 100.0);
      mqttClient.publish(topic, payload, true);

      snprintf(topic, sizeof(topic), "%s/links/Tara", mqttTopic);
      snprintf(payload, sizeof(payload), "%.0f", sensor1_tara);
      mqttClient.publish(topic, payload, true);

      snprintf(topic, sizeof(topic), "%s/links/Flaschengroesse", mqttTopic);
      snprintf(payload, sizeof(payload), "%d", bottle1);
      mqttClient.publish(topic, payload, true);

      snprintf(topic, sizeof(topic), "%s/links/Gesamtgewicht", mqttTopic);
      snprintf(payload, sizeof(payload), "%.2f", g1 + sensor1_tara);
      mqttClient.publish(topic, payload, true);
    }

    // --- RECHTS ---
    if (!sensor2_error) {
      snprintf(topic, sizeof(topic), "%s/rechts/Gasgewicht", mqttTopic);
      snprintf(payload, sizeof(payload), "%.2f", g2);
      mqttClient.publish(topic, payload, true);

      snprintf(topic, sizeof(topic), "%s/rechts/GasinhaltProzent", mqttTopic);
      snprintf(payload, sizeof(payload), "%.1f", (g2 / bottle2) * 100.0);
      mqttClient.publish(topic, payload, true);

      snprintf(topic, sizeof(topic), "%s/rechts/Tara", mqttTopic);
      snprintf(payload, sizeof(payload), "%.0f", sensor2_tara);
      mqttClient.publish(topic, payload, true);

      snprintf(topic, sizeof(topic), "%s/rechts/Flaschengroesse", mqttTopic);
      snprintf(payload, sizeof(payload), "%d", bottle2);
      mqttClient.publish(topic, payload, true);

      snprintf(topic, sizeof(topic), "%s/rechts/Gesamtgewicht", mqttTopic);
      snprintf(payload, sizeof(payload), "%.2f", g2 + sensor2_tara);
      mqttClient.publish(topic, payload, true);
    }
  }*/

//neu sekündlich lesen:
void publishMqttData() {
  if (!apMode && mqttClient.connected()) {
    char topic[128], payload[32];

    // 🌡 Temperaturdaten (global, einmal fürs Haupttopic)
    snprintf(topic, sizeof(topic), "%s/Temperatur", mqttTopic);
    snprintf(payload, sizeof(payload), "%.1f", temperatureC);
    mqttClient.publish(topic, payload, true);

    snprintf(topic, sizeof(topic), "%s/Temperatursensorstatus", mqttTopic);
    mqttClient.publish(topic, tempSensorOK ? "OK" : "Fehler", true);

    // --- LINKS ---
    if (!e1_aktuell) {
      snprintf(topic, sizeof(topic), "%s/links/Gasgewicht", mqttTopic);
      snprintf(payload, sizeof(payload), "%.2f", g1_aktuell);
      mqttClient.publish(topic, payload, true);

      snprintf(topic, sizeof(topic), "%s/links/GasinhaltProzent", mqttTopic);
      snprintf(payload, sizeof(payload), "%.1f", (g1_aktuell / bottle1) * 100.0);
      mqttClient.publish(topic, payload, true);

      snprintf(topic, sizeof(topic), "%s/links/Tara", mqttTopic);
      snprintf(payload, sizeof(payload), "%.0f", sensor1_tara);
      mqttClient.publish(topic, payload, true);

      snprintf(topic, sizeof(topic), "%s/links/Flaschengroesse", mqttTopic);
      snprintf(payload, sizeof(payload), "%d", bottle1);
      mqttClient.publish(topic, payload, true);

      snprintf(topic, sizeof(topic), "%s/links/Gesamtgewicht", mqttTopic);
      snprintf(payload, sizeof(payload), "%.2f", g1_aktuell + sensor1_tara);
      mqttClient.publish(topic, payload, true);

      // ✅ Neue Datenpunkte
      snprintf(topic, sizeof(topic), "%s/links/Gewicht_ohne_Kompensation", mqttTopic);
      snprintf(payload, sizeof(payload), "%.2f", g1_roh);
      mqttClient.publish(topic, payload, true);

      snprintf(topic, sizeof(topic), "%s/links/TempKorrektur", mqttTopic);
      snprintf(payload, sizeof(payload), "%.2f", g1_roh - g1_aktuell);
      mqttClient.publish(topic, payload, true);
    }

    // --- RECHTS ---
    if (!e2_aktuell) {
      snprintf(topic, sizeof(topic), "%s/rechts/Gasgewicht", mqttTopic);
      snprintf(payload, sizeof(payload), "%.2f", g2_aktuell);
      mqttClient.publish(topic, payload, true);

      snprintf(topic, sizeof(topic), "%s/rechts/GasinhaltProzent", mqttTopic);
      snprintf(payload, sizeof(payload), "%.1f", (g2_aktuell / bottle2) * 100.0);
      mqttClient.publish(topic, payload, true);

      snprintf(topic, sizeof(topic), "%s/rechts/Tara", mqttTopic);
      snprintf(payload, sizeof(payload), "%.0f", sensor2_tara);
      mqttClient.publish(topic, payload, true);

      snprintf(topic, sizeof(topic), "%s/rechts/Flaschengroesse", mqttTopic);
      snprintf(payload, sizeof(payload), "%d", bottle2);
      mqttClient.publish(topic, payload, true);

      snprintf(topic, sizeof(topic), "%s/rechts/Gesamtgewicht", mqttTopic);
      snprintf(payload, sizeof(payload), "%.2f", g2_aktuell + sensor2_tara);
      mqttClient.publish(topic, payload, true);

      // ✅ Neue Datenpunkte
      snprintf(topic, sizeof(topic), "%s/rechts/Gewicht_ohne_Kompensation", mqttTopic);
      snprintf(payload, sizeof(payload), "%.2f", g2_roh);
      mqttClient.publish(topic, payload, true);

      snprintf(topic, sizeof(topic), "%s/rechts/TempKorrektur", mqttTopic);
      snprintf(payload, sizeof(payload), "%.2f", g2_roh - g2_aktuell);
      mqttClient.publish(topic, payload, true);
    }
  }
}
void updateTemperature() {
  
  sensors.requestTemperatures();
  float t = sensors.getTempCByIndex(0);

  if (t > -50 && t < 100) {
    temperatureC = t;
    tempSensorOK = true;
    Serial.print("Temperatur: ");
    Serial.print(temperatureC, 1);
    Serial.println(" °C");
  } else {
    tempSensorOK = false;
    Serial.println("[Fehler] DS18B20 nicht erreichbar!");
  }
}


void setup() {
  Serial.begin(115200);
  setCpuFrequencyMhz(80);  // Stromsparmodus: 80 MHz statt 240
  sensors.begin();
  prefs.begin("waage", false);
  scale1.begin(DT1, SCK1);
  scale2.begin(DT2, SCK2);
      
  //Abfrage des DS18b20
  //start debugging
Serial.print("Sensoren gefunden: ");
Serial.println(sensors.getDeviceCount());
  DeviceAddress tempDeviceAddress;
if (sensors.getDeviceCount() == 0) {
  Serial.println("[Fehler] Kein Sensor gefunden (getDeviceCount == 0)");
} else {
  if (sensors.getAddress(tempDeviceAddress, 0)) {
    Serial.print("Sensoradresse: ");
    for (uint8_t i = 0; i < 8; i++) {
      if (tempDeviceAddress[i] < 16) Serial.print("0");
      Serial.print(tempDeviceAddress[i], HEX);
    }
    Serial.println();
  } else {
    Serial.println("[Fehler] Sensoradresse konnte nicht gelesen werden.");
  }
} //end debugging

  // Lese gespeicherte Werte
  sensor1_r0 = prefs.getFloat("sensor1_r0", 0);
  sensor1_r1 = prefs.getFloat("sensor1_r1", 1);
  sensor1_g1 = prefs.getFloat("sensor1_g1", 100);
  sensor1_tara = prefs.getFloat("sensor1_tara", 0);
  sensor2_r0 = prefs.getFloat("sensor2_r0", 0);
  sensor2_r1 = prefs.getFloat("sensor2_r1", 1);
  sensor2_g1 = prefs.getFloat("sensor2_g1", 100);
  sensor2_tara = prefs.getFloat("sensor2_tara", 0);
  bottle1 = prefs.getInt("bottle1", 11000);
  bottle2 = prefs.getInt("bottle2", 11000);
  
  komp_t1_1 = prefs.getFloat("komp_t1_1", 0);
  komp_g1_1 = prefs.getFloat("komp_g1_1", 0);
  komp_t2_1 = prefs.getFloat("komp_t2_1", 0);
  komp_g2_1 = prefs.getFloat("komp_g2_1", 0);

  komp_t1_2 = prefs.getFloat("komp_t1_2", 0);
  komp_g1_2 = prefs.getFloat("komp_g1_2", 0);
  komp_t2_2 = prefs.getFloat("komp_t2_2", 0);
  komp_g2_2 = prefs.getFloat("komp_g2_2", 0);
  // WLAN und MQTT
  prefs.getString("ssid", ssid, sizeof(ssid));
  prefs.getString("pass", password, sizeof(password));
  prefs.getString("mqttServer", mqttServer, sizeof(mqttServer));
  mqttPort = prefs.getInt("mqttPort", 1883);
  prefs.getString("mqttTopic", mqttTopic, sizeof(mqttTopic));



  WiFi.begin(ssid, password);
  WiFi.setSleep(true);  // ✅ hinzugefügt
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500); Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.softAP(apSSID, apPassword);
    apMode = true;
  }

  mqttClient.setServer(mqttServer, mqttPort);

  if (WiFi.status() == WL_CONNECTED) {
    delay(500);  // Netz stabilisieren
    Serial.print("[MQTT] Verbinde mit "); Serial.println(mqttServer);
    if (mqttClient.connect("GaswaageClient")) {
      Serial.println("[MQTT] Verbindung erfolgreich");
    } else {
      Serial.print("[MQTT] Fehler: "); Serial.println(mqttClient.state());
    }
  } else {
    Serial.println("[MQTT] Übersprungen – kein WLAN");
  }

  /*  // JSON /data Ausgabe
    server.on("/data", []() {
      double g1 = calcGewicht(scale1, sensor1_r0, sensor1_r1, sensor1_g1, sensor1_tara, sensor1_error);
      double g2 = calcGewicht(scale2, sensor2_r0, sensor2_r1, sensor2_g1, sensor2_tara, sensor2_error);
      String json = "{\"gewicht1\":" + String(g1,2) + ",\"gewicht2\":" + String(g2,2);
      json += ",\"sensor1_error\":" + String(sensor1_error ? "true" : "false");
      json += ",\"sensor2_error\":" + String(sensor2_error ? "true" : "false") + "}";
      server.send(200, "application/json", json);
    });
  */
  //neu sekündlich lesen:
 /*/ server.on("/data", []() {
    String json = "{\"gewicht1\":" + String(g1_aktuell, 2);
    json += ",\"gewicht2\":" + String(g2_aktuell, 2);
    json += ",\"sensor1_error\":" + String(e1_aktuell ? "true" : "false");
    json += ",\"sensor2_error\":" + String(e2_aktuell ? "true" : "false") + "}";
    server.send(200, "application/json", json);
  });
*/

server.on("/data", []() {
  //double roh1 = calcGewicht(scale1, sensor1_r0, sensor1_r1, sensor1_g1, sensor1_tara, sensor1_error);
//  double roh2 = calcGewicht(scale2, sensor2_r0, sensor2_r1, sensor2_g1, sensor2_tara, sensor2_error);

  String json = "{";
  json += "\"gewicht1\":" + String(g1_aktuell, 2);
  json += ",\"rohgewicht1\":" + String(g1_roh, 2);
  json += ",\"gewicht2\":" + String(g2_aktuell, 2);
  json += ",\"rohgewicht2\":" + String(g2_roh, 2);
  json += ",\"sensor1_error\":" + String(e1_aktuell ? "true" : "false");
  json += ",\"sensor2_error\":" + String(e2_aktuell ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
});

  server.on("/", HTTP_GET, []() {
    String page = MAIN_PAGE;
    page.replace("%HTML_MENU%", HTML_MENU);  // <- das ist entscheidend!
    page.replace("%B1_5%",  bottle1 == 5000 ? "checked" : "");
    page.replace("%B1_11%", bottle1 == 11000 ? "checked" : "");
    page.replace("%B2_5%",  bottle2 == 5000 ? "checked" : "");
    page.replace("%B2_11%", bottle2 == 11000 ? "checked" : "");
    server.send(200, "text/html", page);
  });

 server.on("/setbottles", HTTP_POST, []() {
  if (server.hasArg("bottle1")) {
    bottle1 = server.arg("bottle1").toInt();
    Serial.print("[SET] Neue Flaschengröße links: ");
    Serial.println(bottle1);
    prefs.putInt("bottle1", bottle1);
  } else {
    Serial.println("[SET] bottle1 nicht vorhanden!");
  }

  if (server.hasArg("bottle2")) {
    bottle2 = server.arg("bottle2").toInt();
    Serial.print("[SET] Neue Flaschengröße rechts: ");
    Serial.println(bottle2);
    prefs.putInt("bottle2", bottle2);
  } else {
    Serial.println("[SET] bottle2 nicht vorhanden!");
  }

  updateGewicht();
  if (!apMode && mqttClient.connected()) {
    publishMqttData();
  }

  server.sendHeader("Location", "/");
  server.send(303);
});
  server.on("/tempkomp", HTTP_GET, []() {
  // Innerhalb der Lambda alle Werte lokal laden
  float t1_1 = prefs.getFloat("komp_t1_1", 0);
  float g1_1 = prefs.getFloat("komp_g1_1", 0);
  float t2_1 = prefs.getFloat("komp_t2_1", 0);
  float g2_1 = prefs.getFloat("komp_g2_1", 0);

  float t1_2 = prefs.getFloat("komp_t1_2", 0);
  float g1_2 = prefs.getFloat("komp_g1_2", 0);
  float t2_2 = prefs.getFloat("komp_t2_2", 0);
  float g2_2 = prefs.getFloat("komp_g2_2", 0);

  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Temp.Komp.</title><style>body{font-family:sans-serif;margin:20px;}nav a{margin-right:10px;}button{margin:5px;}</style></head><body>";
  html += "<h2>Temperaturkompensation</h2>";
  html += HTML_MENU;

  html += "<p>Sensorstatus: ";
  html += tempSensorOK ? "<b>OK</b></p>" : "<b style='color:red'>Fehler</b></p>";

  html += "<form method='post' action='/tempkomp'>";

  html += "<h3>Flasche links</h3>";
  html += "Aktuelle Temp: " + String(temperatureC, 1) + " °C<br>";
  html += "Aktuelles Gewicht: " + String(g1_aktuell, 2) + " g<br>";
  html += "<button name='set' value='p1_1'>Punkt 1 speichern</button> ";
  html += "<button name='set' value='p2_1'>Punkt 2 speichern</button><br>";
  html += "Gespeichert:<br>";
  html += "T1 = " + String(t1_1, 1) + " °C, G1 = " + String(g1_1, 2) + " g<br>";
  html += "T2 = " + String(t2_1, 1) + " °C, G2 = " + String(g2_1, 2) + " g<br>";

  html += "<h3>Flasche rechts</h3>";
  html += "Aktuelle Temp: " + String(temperatureC, 1) + " °C<br>";
  html += "Aktuelles Gewicht: " + String(g2_aktuell, 2) + " g<br>";
  html += "<button name='set' value='p1_2'>Punkt 1 speichern</button> ";
  html += "<button name='set' value='p2_2'>Punkt 2 speichern</button><br>";
  html += "Gespeichert:<br>";
  html += "T1 = " + String(t1_2, 1) + " °C, G1 = " + String(g1_2, 2) + " g<br>";
  html += "T2 = " + String(t2_2, 1) + " °C, G2 = " + String(g2_2, 2) + " g<br>";

  html += "<hr><button name='set' value='reset_1'>Links zurücksetzen</button> ";
  html += "<button name='set' value='reset_2'>Rechts zurücksetzen</button>";

  html += "</form></body></html>";
  server.send(200, "text/html", html);
});

server.on("/tempkomp", HTTP_POST, []() {
  String which = server.arg("set");

  if (!tempSensorOK) {
    Serial.println("[Warnung] Kein gültiger Temperatursensor – Speichern blockiert!");
    server.sendHeader("Location", "/tempkomp");
    server.send(303);
    return;
  }

  if (which == "p1_1") {
    komp_t1_1 = temperatureC;
    komp_g1_1 = g1_aktuell;
    prefs.putFloat("komp_t1_1", komp_t1_1);
    prefs.putFloat("komp_g1_1", komp_g1_1);
  }
  if (which == "p2_1") {
    komp_t2_1 = temperatureC;
    komp_g2_1 = g1_aktuell;
    prefs.putFloat("komp_t2_1", komp_t2_1);
    prefs.putFloat("komp_g2_1", komp_g2_1);
  }
  if (which == "p1_2") {
    komp_t1_2 = temperatureC;
    komp_g1_2 = g2_aktuell;
    prefs.putFloat("komp_t1_2", komp_t1_2);
    prefs.putFloat("komp_g1_2", komp_g1_2);
  }
  if (which == "p2_2") {
    komp_t2_2 = temperatureC;
    komp_g2_2 = g2_aktuell;
    prefs.putFloat("komp_t2_2", komp_t2_2);
    prefs.putFloat("komp_g2_2", komp_g2_2);
  }

if (which == "reset_1") {
  komp_t1_1 = komp_t2_1 = komp_g1_1 = komp_g2_1 = 0;
  prefs.putFloat("komp_t1_1", 0);
  prefs.putFloat("komp_t2_1", 0);
  prefs.putFloat("komp_g1_1", 0);
  prefs.putFloat("komp_g2_1", 0);
}

if (which == "reset_2") {
  komp_t1_2 = komp_t2_2 = komp_g1_2 = komp_g2_2 = 0;
  prefs.putFloat("komp_t1_2", 0);
  prefs.putFloat("komp_t2_2", 0);
  prefs.putFloat("komp_g1_2", 0);
  prefs.putFloat("komp_g2_2", 0);
}

  server.sendHeader("Location", "/tempkomp");
  server.send(303);
});

  server.on("/kalibrierung", HTTP_GET, []() {
    long val1 = scale1.read();
    long val2 = scale2.read();
    String html = "";
    html += "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Kalibrierung</title><style>body{font-family:sans-serif;margin:20px;background:#f7f7f7;}nav a{margin-right:10px;}</style></head><body><h2>Tara</h2>";
    html += HTML_MENU;
    html += "<form method='post' action='/kalibrierung'>";
    html += "<h3>Flasche links</h3>Rohwert: " + String(val1) + "<br><button name='set' value='r0_1'>Leergewicht übernehmen</button><br><button name='set' value='r1_1'>Referenz übernehmen</button> Referenzgewicht (kg): <input name='g1_1' value='" + String(sensor1_g1 / 1000.0, 2) + "'><br>";
    html += "<h3>Flasche rechts</h3>Rohwert: " + String(val2) + "<br><button name='set' value='r0_2'>Leergewicht übernehmen</button><br><button name='set' value='r1_2'>Referenz übernehmen</button> Referenzgewicht (kg): <input name='g1_2' value='" + String(sensor2_g1 / 1000.0, 2) + "'><br>";
    html += "<input type='submit'></form></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/kalibrierung", HTTP_POST, []() {
    long val1 = scale1.read();
    long val2 = scale2.read();
    String which = server.arg("set");
    if (which == "r0_1") {
      sensor1_r0 = val1;
      prefs.putFloat("sensor1_r0", val1);
    }
    if (which == "r1_1") {
      sensor1_r1 = val1;
      String g = server.arg("g1_1"); g.replace(',', '.');
      sensor1_g1 = g.toFloat() * 1000.0;
      prefs.putFloat("sensor1_r1", val1);
      prefs.putFloat("sensor1_g1", sensor1_g1);
    }
    if (which == "r0_2") {
      sensor2_r0 = val2;
      prefs.putFloat("sensor2_r0", val2);
    }
    if (which == "r1_2") {
      sensor2_r1 = val2;
      String g = server.arg("g1_2"); g.replace(',', '.');
      sensor2_g1 = g.toFloat() * 1000.0;
      prefs.putFloat("sensor2_r1", val2);
      prefs.putFloat("sensor2_g1", sensor2_g1);
    }
    server.sendHeader("Location", "/kalibrierung");
    server.send(303);
  });

  server.on("/tara", HTTP_GET, []() {
  String html = "";
  html += "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Tara</title><style>body{font-family:sans-serif;margin:20px;background:#f7f7f7;}nav a{margin-right:10px;} button{margin-top:5px;}</style></head><body><h2>Tara</h2>";
  html += HTML_MENU;

  html += "<form method='post' action='/tara'>";
  
  html += "<h3>Linke Flasche</h3>";
  html += "Tara (kg): <input name='tara1' value='" + String(sensor1_tara / 1000.0, 2) + "'>";
  html += "<button type='submit' name='set' value='left'>Übernehmen</button><br><br>";

  html += "<h3>Rechte Flasche</h3>";
  html += "Tara (kg): <input name='tara2' value='" + String(sensor2_tara / 1000.0, 2) + "'>";
  html += "<button type='submit' name='set' value='right'>Übernehmen</button>";

  html += "</form></body></html>";
  server.send(200, "text/html", html);
});

server.on("/tara", HTTP_POST, []() {
  String which = server.arg("set");

  if (which == "left" && server.hasArg("tara1")) {
    String t = server.arg("tara1");
    t.replace(',', '.');
    sensor1_tara = t.toFloat() * 1000.0;
    prefs.putFloat("sensor1_tara", sensor1_tara);
    Serial.printf("[TARA] links gesetzt: %.1f g\n", sensor1_tara);
  }

  if (which == "right" && server.hasArg("tara2")) {
    String t = server.arg("tara2");
    t.replace(',', '.');
    sensor2_tara = t.toFloat() * 1000.0;
    prefs.putFloat("sensor2_tara", sensor2_tara);
    Serial.printf("[TARA] rechts gesetzt: %.1f g\n", sensor2_tara);
  }

  updateGewicht();  // sofort neu berechnen
  server.sendHeader("Location", "/tara");
  server.send(303);
});

  server.on("/wifi", HTTP_GET, []() {
    String html = "";
    html += "<!DOCTYPE html><html><head><meta charset='utf-8'><title>WLAN</title><style>body{font-family:sans-serif;margin:20px;background:#f7f7f7;}nav a{margin-right:10px;}</style></head><body><h2>WLAN</h2>";
    html += HTML_MENU;    html += "<form method='post' action='/wifi'>SSID: <input name='ssid' value='" + String(ssid) + "'><br>Passwort: <input name='pass' value='" + String(password) + "'><br><input type='submit'></form></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/wifi", HTTP_POST, []() {
    prefs.putString("ssid", server.arg("ssid"));
    prefs.putString("pass", server.arg("pass"));
    server.send(200, "text/html", "WLAN gespeichert. Neustart...");
    delay(1000); ESP.restart();
  });

  server.on("/mqtt", HTTP_GET, []() {
    String html = "";
    html += "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Tara</title><style>body{font-family:sans-serif;margin:20px;background:#f7f7f7;}nav a{margin-right:10px;}</style></head><body><h2>Tara</h2>";
    html += HTML_MENU;
    html += "<form method='post' action='/mqtt'>Broker: <input name='server' value='" + String(mqttServer) + "'><br>Port: <input name='port' value='" + String(mqttPort) + "'><br>Topic: <input name='topic' value='" + String(mqttTopic) + "'><br><input type='submit'></form></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/mqtt", HTTP_POST, []() {
    prefs.putString("mqttServer", server.arg("server"));
    prefs.putInt("mqttPort", server.arg("port").toInt());
    prefs.putString("mqttTopic", server.arg("topic"));
    server.send(200, "text/html", "MQTT gespeichert. Neustart...");
    delay(1000); ESP.restart();
  });

  server.begin();
  Serial.println("Webserver gestartet");
}

void loop() {
  server.handleClient();
  mqttClient.loop();

  //neu sekündlich lesen:
  // Messung alle 1 Sekunde
  if (millis() - lastSensorUpdate > 1000) {
    updateGewicht();
    updateTemperature();  // Temperatur sekündlich lesen + ausgeben
    lastSensorUpdate = millis();
  }

  // MQTT reconnect bei Bedarf (nur wenn WLAN steht)
  if (!apMode && WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
    static unsigned long lastReconnectAttempt = 0;
    if (millis() - lastReconnectAttempt > 5000) { // alle 5 Sekunden versuchen
      lastReconnectAttempt = millis();
      Serial.println("[MQTT] Verbindung wird versucht...");
      if (mqttClient.connect("GaswaageClient")) {
        Serial.println("[MQTT] Verbunden");
      } else {
        Serial.print("[MQTT] Fehler: ");
        Serial.println(mqttClient.state());
      }
    }

  }

  // MQTT Daten senden alle 10 Sekunden
  if (!apMode && mqttClient.connected() && millis() - lastMqttSent > 10000) {
    publishMqttData();
    lastMqttSent = millis();
  }
}
# Gaswaage mit ESP32, HX711 und Temperaturkompensation

Dieses Projekt ist eine smarte Gasflaschenwaage zur gleichzeitigen Überwachung von zwei Gasflaschen. Es basiert auf einem **ESP32**, **zwei HX711-Wägezellenverstärkern** und einem **DS18B20-Temperatursensor** zur Temperaturkompensation. Das System stellt eine **Weboberfläche** zur Konfiguration bereit und sendet die aktuellen Daten strukturiert per **MQTT**.

---

## 🔧 Hardwareaufbau

### Komponenten:
- ESP32-Dev-Board
- 2× Wägezelle mit HX711
- 1× DS18B20 Temperatursensor (1-Wire)
- Stromversorgung (z. B. USB oder 5 V-Netzteil)

### Pinbelegung:

| Funktion         | ESP32 GPIO |
|------------------|-------------|
| HX711 Links DT   | GPIO 18     |
| HX711 Links SCK  | GPIO 19     |
| HX711 Rechts DT  | GPIO 21     |
| HX711 Rechts SCK | GPIO 22     |
| DS18B20          | GPIO 4      |

---

## 🌐 Weboberfläche

Die Weboberfläche ist über das lokale Netzwerk erreichbar, sobald das Gerät verbunden ist. Im Setup-Modus öffnet sich ein Access Point (`Waage-Setup`).

### Seitenstruktur:

| URL            | Funktion                           |
|----------------|------------------------------------|
| `/`            | Statusanzeige (Füllstand, Gewicht) |
| `/kalibrierung`| Rohwert-Kalibrierung               |
| `/tara`        | Taragewicht setzen                 |
| `/tempkomp`    | Temperaturkompensation konfigurieren|
| `/wifi`        | WLAN-Zugangsdaten speichern        |
| `/mqtt`        | MQTT-Konfiguration                 |

---

## 🧪 Bedienung über Webinterface

### Flaschenauswahl:
- Auswahl zwischen 5 kg und 11 kg pro Flasche (links/rechts)

### Kalibrierung:
1. Plattform leer → `/kalibrierung` → "Leergewicht übernehmen"
2. Referenzgewicht auflegen → "Referenz übernehmen" + Eingabe des Gewichts

### Tara:
- Manuelle Eingabe des Taragewichts pro Seite in **kg**

### Temperaturkompensation:
1. Referenzlast auflegen
2. Bei Temperatur A → "Punkt 1 speichern"
3. Später bei Temperatur B → "Punkt 2 speichern"

Die Kompensation erfolgt linear zwischen den beiden Punkten.

---

## 📡 MQTT-Schnittstelle

### Strukturierte Topics (Beispiel bei mqttTopic=`gaswaage`):

```plaintext
gaswaage/links/Gasgewicht
gaswaage/links/Tara
gaswaage/links/Flaschengroesse
gaswaage/links/GasinhaltProzent
gaswaage/links/Gesamtgewicht
gaswaage/links/Gewicht_ohne_Kompensation
gaswaage/links/TempKorrektur

gaswaage/rechts/...
gaswaage/Temperatur
gaswaage/Temperatursensorstatus
```

### MQTT-Publish:
- Automatisch alle 10 Sekunden, wenn WLAN und MQTT aktiv

---

## 📥 MQTT-Steuerung (Subscribe)

Geplant oder optional umsetzbar (Version 1.6.4+):

- `gaswaage/links/tara` → Tara-Wert in kg
- `gaswaage/links/kalibrierung/referenzgewicht` → z. B. 11.0
- `gaswaage/links/kalibrierung/speichern_r0` → `1`
- `gaswaage/links/tempkomp/speichern_punkt1` → `1`
- `gaswaage/links/gasflaschenart 0_5Kg, 1_11Kg` → `0` oder `1`

---

## 📦 Dateistruktur (PlatformIO)

```plaintext
├── include/              # Header-Dateien
├── lib/                  # Externe Bibliotheken
├── src/
│   └── main.cpp          # Hauptprogramm
├── platformio.ini        # Projektkonfiguration
```

---

## 🛠 Projektstatus

- Aktuell: Version 1.6.3
- Nächste Schritte (1.6.4):
  - MQTT-Subscribe (Kalibrierung, Tara, Flaschentyp)
  - UI-Tara mit Einzel-Buttons
  - README/Repo-Pflege

---

## 🔗 Lizenz

MIT License – Nutzung auf eigene Gefahr. Beiträge willkommen!

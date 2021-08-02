///////////////////////////////////////////
// PROGRAM FOR EASY BIKE TAP CARD SYSTEM //
//       By Easy Bike Feature Team       //
///////////////////////////////////////////

// Intialize library
#include <SPI.h> // RFID
#include <MFRC522.h>
#include <ESP8266WiFi.h> // Wifi and HTTP
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <Wire.h> // OLED
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSerif12pt7b.h>
#include <Fonts/FreeSerif18pt7b.h>
#include <Fonts/FreeSerif24pt7b.h>
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#include "Arduino.h"
#include "PCF8574.h" // PCF8574

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Declaration for PCF8574 adress
PCF8574 pcf8574(0x20);

// Wifi name and password
const char *ssid =  "MF90_452200";
const char *pass =  "2U346J2J88";

// API for checking rfid input
const char *serverName = "http://api.easybike.web.id/rfidcheck";

// RFID PIN //
#define SDA_PIN D3
#define RST_PIN D4
// SCK => D5
// MOSI => D7
// MISO =>  D6
// RQ = Not connected

// OLED PIN //
// SDA => D2
// SCK => D1
// VCC => 3v below TX RX
// GND => GNB below TX RX

MFRC522 mfrc522(SDA_PIN, RST_PIN);   // Create MFRC522 instance.

// LOCK, LED and Buzzer PIN
#define LOCK_PIN D8 // Pin to control relay
#define VCC_PIN D0 // Vcc pin for LED and buzzer
#define RED_LED_PIN P0 // Pin 0 in PCF, Control Red LED
#define GREEN_LED_PIN P1 // Pin 1 in PCF, Control Green LED and Buzzer
#define BATTERY_PIN A0 // Analog input for bike battery percentage

// Battery analog read
int batteryValue;
int batteryPercentage;

void setup() {
  Serial.begin(115200);

  // Setup pin output and input
  pinMode(LOCK_PIN, OUTPUT); // Control Relay
  pinMode(VCC_PIN, OUTPUT); // Control VCC for LED and Buzzer
  pcf8574.pinMode(RED_LED_PIN, OUTPUT);
  pcf8574.pinMode(GREEN_LED_PIN, OUTPUT);
  delay(1000);

  // Check OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.setTextColor(WHITE);

  // Show not connected sign in oled and led before connecting to wifi
  // In case wifi failed to connect
  wifiNotConnectedState();

  // Connect to wifi
  WifiConnecting();

  // Initiate RFID
  SPI.begin();      // Initiate  SPI bus
  mfrc522.PCD_Init();   // Initiate MFRC522
  Serial.println("Approximate your card to the reader...");
  Serial.println();
}


void loop() {
  // Battery analog read
  batteryValue = analogRead(BATTERY_PIN);
  int openAnalogInput = 825;
  batteryPercentage = (((batteryValue-openAnalogInput)));
  // minus open analog input, because when bike not connected, the input is not zero

  // State for all parameter
  lockedState();

  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }
  
  // Show UID on serial monitor
  Serial.print("UID tag :");
  String content = "";
  byte letter;
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  Serial.println();
  Serial.print("Message : ");
  content.toUpperCase();

  // JsonObject
  StaticJsonDocument<300> jsonBuffer;
  JsonObject root = jsonBuffer.to<JsonObject>();
  root["rfid"] = content.substring(1);
  char dataRFID[300];
  serializeJson(jsonBuffer, (char*)dataRFID, measureJson(jsonBuffer) + 1);
  Serial.println(dataRFID);

  if (WiFi.status() == WL_CONNECTED) {
    // RFID card detected
    // Show loading status in oled
    oledUnlocking();

    // Send RFID data to API
    HTTPClient http;
    http.begin(serverName);
    // Specify content-type header
    http.addHeader("Content-Type", "application/json");
    // Send HTTP POST request
    int httpResponseCode = http.POST(dataRFID);
    String payload = http.getString();
    Serial.print("HTTP Response code: " + String(httpResponseCode));
    Serial.println(payload);

    // Conditioning RFID(Tester)
    if (httpResponseCode == 200) {
      // Access Success
      Serial.println("Authorized access");
      // Set to unlock state for selnoid lock, oled, led and buzzer
      unlockedState();
      delay(500);
    } else if (httpResponseCode == -1) {
      // Access Failed
      Serial.println(" Connection Failed");
      oledConnectionFailed();
      pcf8574.digitalWrite(P0, HIGH);
      pcf8574.digitalWrite(P1, LOW);
      delay(400);
      pcf8574.digitalWrite(P0, LOW);
      pcf8574.digitalWrite(P1, HIGH);
      delay(400);
      } else {
      // Access Failed
      Serial.println(" Access denied");
      accessFail();
      delay(500);
    }
    // Free resources
    http.end();
  } else {
    Serial.println("WiFi DC");
    WifiConnecting();
  }
}

// State //
// When wifi not yet connected
void wifiNotConnectedState() {
  digitalWrite(VCC_PIN, HIGH); // VCC for LED and buzzer
  pcf8574.digitalWrite(RED_LED_PIN, LOW); // Red LED ON
  pcf8574.digitalWrite(GREEN_LED_PIN, HIGH); // Green LED & Buzzer OFF
  oledWifiNotConnected();
}


// Set locked state for lock, led, buzzer and oled
void lockedState() {
  // P0 is VCC of LED
  // VCC must be from nodemcu, if from PCF the voltage will be small
  digitalWrite(LOCK_PIN, LOW); // Relay OFF
  digitalWrite(VCC_PIN, HIGH); // VCC for LED and buzzer
  pcf8574.digitalWrite(RED_LED_PIN, LOW); // Red LED ON
  pcf8574.digitalWrite(GREEN_LED_PIN, HIGH); // Green LED & Buzzer OFF

  // Show battery percentage when lock locked
  // This can be used to know if bike connected
  // We assume if battery less than few percent, the bike is not connected
  if(batteryPercentage < 5){
    oledNotConnected();
  } else {
    oledCharging();
  }
}

// Set unlocked state for lock, led, buzzer and oled
void unlockedState() {
  // Open lock for 5 sec and set unlock condition
  digitalWrite(LOCK_PIN, HIGH); // Relay ON
  digitalWrite(VCC_PIN, HIGH); // VCC for LED and buzzer
  pcf8574.digitalWrite(RED_LED_PIN, HIGH); // Red LED OFF
  pcf8574.digitalWrite(GREEN_LED_PIN, LOW); // Green LED & Buzzer ON
  oledUnlcoked();
  // Delay 5s for lock to open
  delay(5000);
}

void accessFail() {
  // Show unlock status failed in oled
  oledUnlcokedFailed();

  // Make green and buzzer blinking 3 times
  pcf8574.digitalWrite(P0, HIGH);
  pcf8574.digitalWrite(P1, LOW);
  delay(400);
  pcf8574.digitalWrite(P0, LOW);
  pcf8574.digitalWrite(P1, HIGH);
  delay(400);
  pcf8574.digitalWrite(P0, HIGH);
  pcf8574.digitalWrite(P1, LOW);
  delay(400);
  pcf8574.digitalWrite(P0, LOW);
  pcf8574.digitalWrite(P1, HIGH);
  delay(400);
  pcf8574.digitalWrite(P0, HIGH);
  pcf8574.digitalWrite(P1, LOW);
  delay(400);
  pcf8574.digitalWrite(P0, LOW);
  pcf8574.digitalWrite(P1, HIGH);
  delay(400);
}


// OLED State //
void oledWifiNotConnected() {
  digitalWrite(VCC_PIN, HIGH); // VCC for LED and buzzer
  pcf8574.digitalWrite(RED_LED_PIN, LOW); // Red LED ON
  pcf8574.digitalWrite(GREEN_LED_PIN, HIGH); // Green LED & Buzzer OFF
  // Oled
  display.clearDisplay();
  display.setCursor(50, 40);
  display.setFont(&FreeSerif24pt7b);
  display.println("X");
  display.display();
}
void oledCharging() {
  display.clearDisplay();
  display.setCursor(20, 20);
  display.setFont(&FreeSerif12pt7b);
  display.println("Charging");
  display.setCursor(40, 50);
  display.setFont(&FreeSerif18pt7b);
  display.print(batteryPercentage);
  display.print("%");
  display.display();
}
void oledNotConnected() {
  display.clearDisplay();
  display.setCursor(48, 25);
  display.setFont(&FreeSerif12pt7b);
  display.println("Not");
  display.setCursor(13, 50);
  display.println("Connected");
  display.display();
}
void oledUnlocking() {
  display.clearDisplay();
  display.setCursor(6, 38);
  display.setFont(&FreeSerif12pt7b);
  display.println("Unlocking...");
  display.display();
}
void oledUnlcoked() {
  display.clearDisplay();
  display.setCursor(20, 40);
  display.setFont(&FreeSerif12pt7b);
  display.println("Unlocked");
  display.display();
}
void oledUnlcokedFailed() {
  display.clearDisplay();
  display.setCursor(21, 25);
  display.setFont(&FreeSerif12pt7b);
  display.println("Card Not");
  display.setCursor(15, 50);
  display.println("Registered");
  display.display();
}
void oledConnectionFailed() {
  display.clearDisplay();
  display.setCursor(21, 25);
  display.setFont(&FreeSerif12pt7b);
  display.println("Connection");
  display.setCursor(15, 50);
  display.println("Error");
  display.display();
}

void WifiConnecting() {
  Serial.println("Connecting to ");
  WiFi.mode(WIFI_STA);
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

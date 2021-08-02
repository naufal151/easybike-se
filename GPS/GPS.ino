#include<ESP8266WiFi.h>
#include<SoftwareSerial.h>
#include<TinyGPS++.h>
#include<ESP8266HTTPClient.h>
#include<ArduinoJson.h>

/* EASY BIKE UNPAD FEATURE
    TRACKING (UNPAD AROUND)
    CONTROLLING DC MOTOR (TURN OFF DC MOTOR)
    CORE CONCEPT IS A GPS TRACKING EASY BIKE. TRACKING THIS BIKE USING GEOFENCE
*/

#define rxPin 4
#define txPin 5
#define relayPin 7
#define ssid "71Sumedang"
#define pass "23462071"


String id = "2";
String latitude, longitude;                                     //gps latitude and longitude
String serverName = "http://api.easybike.web.id/gpsaccept";

TinyGPSPlus gps;                                            //tinygps object
SoftwareSerial ss(rxPin, txPin);                            //the serial software object
HTTPClient http;                                            //Declare object of class HTTPClient


void postData(char JSONmessageBuffer[]) {
  http.begin(serverName);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(JSONmessageBuffer);

  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] POST... code: %d\n", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      const String& payload = http.getString();
      Serial.println("received payload:\n<<");
      Serial.println(payload);
      Serial.println(">>");

      //      // parsing start
      //      StaticJsonBuffer<200> jsonBuffer;
      //      JsonObject& root = jsonBuffer.parseObject(payload);
      //      if (!root.success()) {
      //        Serial.println("parseObject() failed");
      //        return;
      //      }
      //
      //      const char* state = root["Di dalam zona"];
      //      const char* bike_id = root["id"];
      //      double latitude = root["longitude"];
      //      double longitude = root["latitude"];
      //
      //      Serial.print("state: ");
      //      Serial.println(state);
      //      Serial.print("bike_id: ");
      //      Serial.println(bike_id);
      //      Serial.print("latitude: ");
      //      Serial.println(latitude);
      //      Serial.print("longitude: ");
      //      Serial.println(longitude);
      //
      //      if (state == "Tidak") {
      //        digitalWrite(relayPin, LOW);
      //      }
    }
  } else {
    Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}


void infoGPS() {
  while (ss.available() > 0) {
    //startTime = millis();
    gps.encode(ss.read());
    if (gps.location.isUpdated()) {
      latitude = String (gps.location.lat(), 6);
      longitude = String (gps.location.lng(), 6);
      Serial.print("latitude: ");
      Serial.println(latitude);
      Serial.print("longitude: ");
      Serial.println(longitude);

      //  json declaration
      StaticJsonBuffer<300> JSONbuffer;
      JsonObject& JSONencoder = JSONbuffer.createObject();
      JSONencoder["id"] = id;
      JSONencoder["latitude"] = latitude;
      JSONencoder["longitude"] = longitude;

      char JSONmessageBuffer[300];
      JSONencoder.prettyPrintTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
      Serial.println(JSONmessageBuffer);
      //  json end declaration

      postData(JSONmessageBuffer);
    }
  }
//  if (millis() > 5000 && gps.charsProcessed() < 10) {
//    Serial.println(F("No GPS detected: check wiring."));
//    while (true);
//    Serial.println(". . . ");
//  }
}

//void httpRequest() {
//  http.begin("http://pintas.unpad.ac.id/ac_portal/login.php");
//  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
//  String postData = "opr=pwdLogin&userName=usernamePAUS&pwd=passPAUS&rememberPwd=0";
//  int httpCode = http.POST(postData);
//  String payload = http.getString();
//  Serial.println(httpCode);
//  Serial.println(payload);
//}

void setup() {
  //wifi connection start
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  //wifi unpad connection
  //  httpRequest();
  //wifi connection end

  ss.begin(9600);
  //  pinMode(relayPin, OUTPUT);
}

void loop() {
  infoGPS();
}

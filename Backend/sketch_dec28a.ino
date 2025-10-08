#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <ESP32Servo.h>

// WiFi Credentials
const char* ssid = "Tejas";
const char* password = "tejas3008";

// WebSocket Server
WebSocketsServer webSocket = WebSocketsServer(81);

// Hardware Pins
#define DHTPIN 4
#define FAN_CONTROL_PIN 25 // Relay (active LOW)
#define VENT_SERVO_PIN 18
#define DHTTYPE DHT11

// Thresholds
const float MAX_TEMP = 30.0;    // Fan ON above this
const float MAX_HUMIDITY = 70.0; // Vent CLOSED above this

// Servo Positions
const int VENT_OPEN = 90;
const int VENT_CLOSED = 0;

// Objects
DHT dht(DHTPIN, DHTTYPE);
Servo ventServo;

// Status variables
bool fanStatus = false;
bool ventStatus = false; // true = OPEN, false = CLOSED
float temperature = 0;
float humidity = 0;

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
      
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %s\n", num, ip.toString().c_str());
        sendStatusUpdate(); // Send initial status
      }
      break;
      
    case WStype_TEXT:
      {
        String message = String((char*)payload);
        Serial.printf("[%u] Received: %s\n", num, message.c_str());
      }
      break;
  }
}

void sendStatusUpdate() {
  StaticJsonDocument<400> doc;
  
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["fan"] = fanStatus;
  doc["vent"] = ventStatus;
  doc["fanReason"] = getFanReason();
  doc["ventReason"] = getVentReason();
  doc["maxTemp"] = MAX_TEMP;
  doc["maxHumidity"] = MAX_HUMIDITY;
  doc["timestamp"] = millis();
  
  String output;
  serializeJson(doc, output);
  webSocket.broadcastTXT(output);
  Serial.println("Sent: " + output);
}

String getFanReason() {
  if (temperature > MAX_TEMP) {
    return "High Temperature";
  } else {
    return "Normal Temperature";
  }
}

String getVentReason() {
  if (humidity > MAX_HUMIDITY) {
    return "High Humidity";
  } else {
    return "Normal Humidity";
  }
}

void controlFan() {
  if (temperature > MAX_TEMP) {
    digitalWrite(FAN_CONTROL_PIN, LOW); // Fan ON (active LOW)
    fanStatus = true;
    Serial.println("Fan ON - High Temp");
  } else {
    digitalWrite(FAN_CONTROL_PIN, HIGH); // Fan OFF
    fanStatus = false;
    Serial.println("Fan OFF - Normal Temp");
  }
}

void controlVent() {
  if (humidity > MAX_HUMIDITY) {
    ventServo.write(VENT_CLOSED);
    ventStatus = false; // CLOSED
    Serial.println("Vent CLOSED - High Humidity");
  } else {
    ventServo.write(VENT_OPEN);
    ventStatus = true; // OPEN
    Serial.println("Vent OPEN - Normal Humidity");
  }
}

void setup() {
  Serial.begin(115200);
  
  // Initialize DHT
  dht.begin();
  
  // Setup Fan Pin
  pinMode(FAN_CONTROL_PIN, OUTPUT);
  digitalWrite(FAN_CONTROL_PIN, HIGH); // Fan OFF initially (active LOW)
  
  // Setup Servo
  ventServo.attach(VENT_SERVO_PIN, 500, 2500);
  ventServo.write(VENT_CLOSED); // Vent closed initially
  
  // Connect to WiFi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Start WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started on port 81");
  Serial.println("System Ready");
}

void loop() {
  webSocket.loop();
  
  // Read sensor values every 5 seconds
  static unsigned long lastSensorRead = 0;
  if (millis() - lastSensorRead > 5000) {
    lastSensorRead = millis();
    
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    
    if (isnan(temp) || isnan(hum)) {
      Serial.println("Sensor error");
      return;
    }
    
    temperature = temp;
    humidity = hum;
    
    Serial.print("Temp: "); 
    Serial.print(temperature); 
    Serial.print("°C | Humidity: "); 
    Serial.print(humidity);
    Serial.println("%");
    
    // Control devices based on sensor readings
    controlFan();
    controlVent();
    
    // Send updates to all connected clients
    sendStatusUpdate();
  }
}
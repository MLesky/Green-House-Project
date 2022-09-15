#include <Arduino.h>
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include <Wire.h>
#define ESP32
#include <Firebase_ESP_Client.h>
#include <Adafruit_Sensor.h>
#include "DHT.h"
#include "time.h"

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Digital pin connected to the DHT sensor
#define DHTPIN 4
// sensor type use
#define DHTTYPE DHT11 
// pin configuration for water sensor
#define POWER_PIN  17 // ESP32 pin GIOP17 connected to sensor's VCC pin
#define SIGNAL_PIN 36 // ESP32 pin GIOP36 (ADC0) connected to sensor's signal pin

// Insert your network credentials
#define WIFI_SSID "REPLACE_WITH_YOUR_SSID"
#define WIFI_PASSWORD "REPLACE_WITH_YOUR_PASSWORD"

// Insert Firebase project API Key
#define API_KEY "PROJECT_API_KEY"

// Insert Authorized Email and Corresponding Password
#define USER_EMAIL "USER_EMAIL"
#define USER_PASSWORD "USER_PASSWORD"

// Insert RTDB URLefine the RealTime DataBase URL
#define DATABASE_URL "DATABASE_URL"

// Define Firebase objects
FirebaseData stream;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variable to save USER UID
String uid;

// Database main path (to be updated in setup with the user UID)
String databasePath;

// Database child nodes
String tempPath;
String humidityPath;
String moisturePath;
String timePath;
String listenerPath;

// Initialise DHT
DHT dht(DHTPIN, DHTTYPE);

float temperature;
float humidity;
float moisture;

//Timer varriaable (sends new reading every 3 mins)
unsigned long sendDataPrevMillis = 0;
unsigned long timerDelay = 180000;

// Initialize WiFi
void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  Serial.println();
}

// write float values to database
void sendFloat(String path, float value){
  if(Firebase.RTDB.setFloat(&fbdo, path.c_str(),value)){
    Serial.print("Writing Value: ");
    Serial.print(value);
    Serial.print("on the following path");
    Serial.println(path);
    Serial.println("PASSED");
    Serial.println("PATH: "+ fbdo.dataPath()); 
    Serial.println("TYPE: "+ fbdo.dataType());  
  }
  else{
    Serial.println("FAILED");
    Serial.println("REASON " + fbdo.errorReason());
  }
}

// Callback function that runs database changes  to events
void streamCallback(FirebaseStream data){
  Serial.printf("stream path, %s\nevent path %s\ndata type, %s\nevent type, %s\n\n",
  data.streamPath().c_str(),
  data.dataPath().c_str(),
  data.dataType().c_str(),
  data.eventType().c_str()); 
  printResult(data);//see addons / RTDBHelper.H
  Serial.println();
  Serial.printf("Received stream payload size: %d (Max. %d)\n\n", 
  data.payloadLength(), data.maxPayloadLength());
}

void streamTimeoutCallback(bool timeout){
  if(timeout){
    Serial.println("Stream timeout , resuming...\n");
  }    
  if(!stream.httpConnected()) {   
    Serial.printf("error code: %d , reason: %s\n\n" ,stream.httpCode(),stream.errorReason().c_str());  
  }    
}

void setup() {
  Serial.begin(115200);

  // Initialize DHT sensor and Wifi
  dht.begin();
  initWiFi();
  
  pinMode(POWER_PIN, OUTPUT);   // configure pin as an OUTPUT for moisture 
    
  // Assign the firebase web api key (required)
  config.api_key = API_KEY;

  // Assign the user sign in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Assign the RealTime DataBase URL (required)
  config.database_url = DATABASE_URL;

  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);

  // Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  // Assign the maximum retry of token generation
  config.max_token_generation_retry = 5;

  // Initialize the library with the Firebase authen and config
  Firebase.begin(&config, &auth);

  // Getting the user UID might take a few seconds
  Serial.println("Getting User UID");
  while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
  }
  // Print user UID
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);

  // Update database path
  databasePath = "/UsersData/" + uid + "/readings";
  // Define database path for sensor reading
  tempPath + "/sensor/temperature";  
  humidityPath + "/sensor/humidity";
  moisturePath + "/sensor/moisture";
  //timePath + "/sensor/time";
  
  //Update databse path fro listening
  listenerPath  = databasePath +"/output"; 
  
  // stream whenever data changes path  
  if(!Firebase.RTDB.beginStream(&stream, listenerPath.c_str())){
    Serial.printf("Stream begin error, %s\n\n",stream.errorReason().c_str());
    // Assifn a all back function to run whenever it detect changes
    Firebase.RTDB.setStreamCallback(&stream, streamCallback, streamTimeoutCallback);
   delay(2000);
  }
}

void loop() {
  // Send new readings to database
  if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();
  }
  digitalWrite(POWER_PIN, HIGH);  // turn the sensor ON
 
  // Get latest sensor reading
  moisture = analogRead(SIGNAL_PIN);
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  moisture = moisture;  
  
  //send  reading to database
  sendFloat(tempPath,temperature); 
  sendFloat(humidityPath,humidity); 
  sendFloat(moisturePath,moisture); 
  // sendFloat(timePath,Time); 
}
// Include libraries
#include <PubSubClient.h>
#include <WiFi.h>
#include <DHTesp.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP32Servo.h>

// Define pins
#define BUZZER 12
#define LDR_L 35   // Left LDR
#define LDR_R 34   // Right LDR
#define SERVOMOTOR 18
const int DHT_PIN = 15;

//Parameters related to light intensity
float GAMMA = 0.75;
const float RL10 = 50;
float MIN_ANGLE = 30;

// Initializing servo motor
int position = 0;
Servo servo;

WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

DHTesp dhtSensor;

// Arrays to store data
char lightAr_L[6];
char lightAr_R[6];
char tempAr[6];
char humAr[6];

// Time variables
bool isScheduledON = false;
unsigned long scheduledOnTime;
unsigned long offsetTime;

void setup() {
  pinMode(LDR_L, INPUT);
  pinMode(LDR_R, INPUT);
  Serial.begin(115200);

  setupWiFi();   // Initialize WiFi connection
  setupMqtt();   // Initialize MQTT client
  timeClient.begin();   // Initialize NTP client

  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);
  servo.attach(SERVOMOTOR, 500, 2400);

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  analogRead(LDR_L);
  analogRead(LDR_R);
}

void loop() {
  // Reconnect to MQTT broker if not connected
  if (!mqttClient.connected()){
    connectToBroker();
  }
  mqttClient.loop();

  // Publish temperature and humidity values
  updateTempAndHumidity();
  mqttClient.publish("ENTC-TEMP", tempAr);
  mqttClient.publish("ENTC-HUMIDITY", humAr);

  // Publish light intensity values  
  mqttClient.publish("LEFT-LDR", lightAr_L);
  mqttClient.publish("RIGHT-LDR", lightAr_R);

  controlServo(); 

  timeClient.setTimeOffset(offsetTime);
  checkSchedule();

  delay(1000);
}

// Function to setup WiFi connection
void setupWiFi(){
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println("Wokwi-GUEST");

  WiFi.begin("Wokwi-GUEST", "");

  while (WiFi.status() != WL_CONNECTED){   // Wait for connection
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
}

// Function to setup MQTT client
void setupMqtt(){
  mqttClient.setServer("test.mosquitto.org", 1883);
  mqttClient.setCallback(receiveCallback);
}

// Function to establish a connection with the MQTT broker
void connectToBroker(){
  while (!mqttClient.connected()){
    Serial.print("Attemping MQTT connection...");
    if(mqttClient.connect("ESP32-477585895")){
      Serial.println("Connected...");
      mqttClient.subscribe("ENTC-ON-OFF");
      mqttClient.subscribe("ENTC-SCH-ON");
      mqttClient.subscribe("MINIMUM-ANGLE");
      mqttClient.subscribe("CONT-FACTOR");
      mqttClient.subscribe("TIME-OFFSET");
    }else {
      Serial.println("Failed");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}

// Callback function for receiving MQTT messages
void receiveCallback(char* topic, byte* payload, unsigned int length){
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("]");

  char payloadCharAr[length];
  for(int i=0; i<length; i++){   // Convert payload to char array
    Serial.print((char)payload[i]);
    payloadCharAr[i] = (char)payload[i];
  }
  Serial.println();

  // Handle received messages based on topic
  if(strcmp(topic, "ENTC-ON-OFF") == 0){
    buzzerOn(payloadCharAr[0]=='1');
  }else if(strcmp(topic, "ENTC-SCH-ON") == 0){
    if(payloadCharAr[0]=='N'){
      isScheduledON = false;
    }else{
      isScheduledON = true;
      scheduledOnTime = atol(payloadCharAr);
    }
  }

  // Receive minimum angle 
  if (strcmp(topic, "MINIMUM-ANGLE") == 0){
    MIN_ANGLE = atoi(payloadCharAr);
  }
  // Receive control factor
  if (strcmp(topic, "CONT-FACTOR") == 0) {
    GAMMA = atof(payloadCharAr);
    if(GAMMA>1){
      GAMMA = 1;
    }
  }
  // Receive time offset
  if (strcmp(topic, "TIME-OFFSET") == 0){   
    offsetTime = atoi(payloadCharAr);  
  }
}

// Function to update light intensity
float updateIntensity(int LDR, char* lightAr) {
  int analogValue = analogRead(LDR);
  float voltage = analogValue / 1024. * 5;
  float resistance = 2000 * voltage / (1 - voltage / 5);
  float maxlux = pow(RL10 * 1e3 * pow(10, GAMMA) / 322.58, (1 / GAMMA));
  float lux = pow(RL10 * 1e3 * pow(10, GAMMA) / resistance, (1 / GAMMA))/maxlux;
  String(lux).toCharArray(lightAr,6);
  return lux;
}

// Function to adjust servo motor position based on light intensity difference
void controlServo(){
  float lux_1 = updateIntensity(LDR_L, lightAr_L);
  float lux_2 = updateIntensity(LDR_R, lightAr_R);
  Serial.print("LeftLux :   ");
  Serial.print(lux_1);
  Serial.print("   RightLux:   ");
  Serial.println(lux_2);

  if(lux_1>lux_2){
    servoMotor(lux_1, 1.5);
  }else{
    servoMotor(lux_2, 0.5);
  }
}

// Function to calculate servo position
void servoMotor(float lux, float D){
  Serial.print("MinAngle:  ");
  Serial.print(MIN_ANGLE);
  Serial.print("   Gamma   :  ");
  Serial.println(GAMMA);
  Serial.println();

  position = MIN_ANGLE*D + (180 - MIN_ANGLE) * lux * GAMMA;
  if(position>1){
    position = 180;
  }
  servo.write(position);
}

///////////////////////// Additional Functions /////////////////////////

// Function to toggle buzzer on/off
void buzzerOn(bool on){
  if(on) {
    tone(BUZZER, 256);
  }else{
    noTone(BUZZER);
  }
}

// Function to get current time from NTP server
unsigned long getTime(){
  timeClient.update();
  return timeClient.getEpochTime();
}

// Function to check scheduled alarms
void checkSchedule(){
  if(isScheduledON){
    unsigned long currentTime = getTime();
    if(currentTime>scheduledOnTime){
      buzzerOn(true);
      isScheduledON = false;

      mqttClient.publish("ENTC-ADMIN-MAIN-ON-OFF-ESP", "1");
      mqttClient.publish("ENTC-ADMIN-SCH-ESP-ON", "0");

      Serial.println("Scheduled ON");
    }
  }
}

// Function to update temperature and humidity readings
void updateTempAndHumidity(){
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  String(data.temperature, 2).toCharArray(tempAr,6);
  String(data.humidity, 2).toCharArray(humAr,6);
  Serial.print("Temp    :  ");
  Serial.print(tempAr);
  Serial.print("   Humidity:  ");
  Serial.println(humAr);
}

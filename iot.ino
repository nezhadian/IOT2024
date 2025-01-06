#include <LiquidCrystal.h>
#include <WiFi.h>
#include <PubSubClient.h>

// fan config
const int fanPin = 18;
long curFanSpeed = 0;

//soilMoisture sensor
const int soilPin = 35 ;

// lcd config
LiquidCrystal lcd(13, 12, 14, 27, 26, 25);

//mqtt config
const char* ssid = "BB";
const char* password = "yasiyasi";

// Replace with your MQTT server settings
const char* mqttServer = "192.168.43.1"; // or your phone's IP address
const int mqttPort = 1884;
const char* mqttClientName = "ArashYasinOmid";
const char* mqttFanSpeedTopic = "AYO/fan";
const char* mqttSoilMoistureTopic = "AYO/soil";
WiFiClient espClient;
PubSubClient client(espClient);


void setup() {

  //setup pwm controller
  const int pwmFreq = 20000;   // Frequency in Hz (Typical for 4-wire fans)
  const int pwmChannel = 0;    // PWM channel
  const int pwmResolution = 8; // Resolution (8 bits = 0-255 duty cycle
  ledcAttachChannel(fanPin, pwmFreq, pwmResolution,8);
  setFanSpeed(255);

  //setup serial
  Serial.begin(115200);

  //soil
  pinMode(soilPin, INPUT);

  //lcd
  lcd.begin(16, 2);  // Set the LCD dimensions (16x2)

  //mqtt
  WiFi.begin(ssid, password);
  client.setServer(mqttServer, mqttPort);

}

void setFanSpeed(int speed){
  ledcWrite(fanPin, speed);  // Set the PWM duty cycle
  curFanSpeed = speed;
}

void setFanSpeedBasedOnSoilMoisture(int soilValue){
    if(soilValue < 30)
      setFanSpeed(255);
    else if(soilValue < 70)
      setFanSpeed(128);
    else if(soilValue <= 100)
      setFanSpeed(0);
    else
      setFanSpeed(255);
}

void refreshLCD(int soilValue,int speed){
  lcd.clear();
  lcd.setCursor(0, 0);  // Set the cursor to the first row and first column
  lcd.print("Soil  ");  // Print a message
  lcd.print(soilValue);
  lcd.setCursor(0, 1);  // Set the cursor to the first row and first column
  lcd.print("RPM  ");  // Print another message
  lcd.print(speed);
}

int readSoilMoistureValue(){
    int analog = analogRead(soilPin);
    int soilValue = 100 - map(analog,0,4095,0,100);
    Serial.print("Debug:soil analog ");
    Serial.println(analog);
    Serial.print("Debug:soil value ");
    Serial.println(soilValue);
    return soilValue;
}

char* intToCharPtr(int value) {
    // Determine the size of the buffer needed
    int size = snprintf(nullptr, 0, "%d", value) + 1; // +1 for the null terminator

    // Allocate memory for the buffer
    char* buffer = (char*)malloc(size);

    if (buffer == nullptr) {
        // Handle memory allocation failure
        return nullptr;
    }

    // Convert the integer to a string
    snprintf(buffer, size, "%d", value);

    return buffer;
}

void publishMQTTData(int soil,int fanSpeed){
    if(WiFi.status() != WL_CONNECTED){
      Serial.println("MQTT: Wifi is not connected");
      return;
    }

    if(!client.connected()){
         client.connect(mqttClientName);
         Serial.println("MQTT: Attempting to connect to broker...");
         return;
    }
     
    char* fanSpeedPtr = intToCharPtr(fanSpeed);
    char* soilSpeedPtr = intToCharPtr(soil);

    client.publish(mqttFanSpeedTopic,fanSpeedPtr);
    client.publish(mqttSoilMoistureTopic,soilSpeedPtr);

    free(fanSpeedPtr);
    free(soilSpeedPtr);

    Serial.println("MQTT: data published.");
}

int getActualFanSpeed(int value){
  return 12000 - map(value,0,255,0,12000);
}

void loop() {
    float soilValue = readSoilMoistureValue();
    Serial.print("Sensor: Soil ");
    Serial.println(soilValue);

    setFanSpeedBasedOnSoilMoisture(soilValue);
    int actualFanSpeed = getActualFanSpeed(curFanSpeed);
    Serial.print("Sensor: Fan speed ");
    Serial.println(actualFanSpeed);

    refreshLCD(soilValue, actualFanSpeed);
    publishMQTTData(soilValue,actualFanSpeed);
    delay(500);
}

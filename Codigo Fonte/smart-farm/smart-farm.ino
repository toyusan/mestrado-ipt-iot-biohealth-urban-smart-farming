/**
 ******************************************************************************
   @file        smart-farm.ino
   @author      Airton Toyofuku
   @copyright   None
   @date        11.07.2022
   @version     1.0
   @brief       Arduino Sketch referring to the biohealth-urban-smart-farming
                project, developed for the IoT discipline of the PROFESSIONAL
                MASTER'S IN APPLIED COMPUTING at IPT.
 ******************************************************************************
*/
/* Private Defines -----------------------------------------------------------*/

#define APP_NAME    "Smart Farm"
#define APP_VERSION "1.0.0"
#define SEPARATOR_LINE "****************************************************"

// Thehse Defines which sensors are in use and when it needs to send data to the Thingsboard plataform
#define USE_DHT
#define USE_BH1750
#define USE_CCS811
#define USE_SOIL_SENSOR
#define USE_THINGSBOARD

/* Private includes ----------------------------------------------------------*/
#include "Arduino.h"
#include "WiFi.h"              // WiFi Library

#ifdef USE_THINGSBOARD
  #include "ThingsBoard.h"     // The Things Board Library
#endif

#ifdef USE_DHT
  #include "DHT.h"             // Temperature and Humidity Library
#endif

#ifdef USE_BH1750
  #include "BH1750.h"          // luminosity Library
#endif

#ifdef USE_CCS811
  #include "Adafruit_CCS811.h" // CO2 Sensero Library
#endif

#include "Wire.h"              // I2C Communication Library

/* Board Configuration--------------------------------------------------------*/
int serialDataRate = 115200;

/* WiFi Configuration---------------------------------------------------------*/
char wifiSsid[] = "###"; // This is the WiFi network name!
char wifiPass[] = "###"; // This is the wifi password!
WiFiClient espClient;

/* ThingsBoard Configuration--------------------------------------------------*/
#ifdef USE_THINGSBOARD
  ThingsBoard thingsBoard(espClient);
  #define THINGSBOARD_SERVER  "###"
  #define TOKEN "###"
#endif

/* DHT Configuration----------------------------------------------------------*/
#ifdef USE_DHT
  DHT dht (13, DHT11); // The sensor is connect under the GPIO 13
  float dhtTemperature = 0;
  float dhtHumidity = 0;
#endif

/* BH1750 Configuration------------------------------------------------------*/
#ifdef USE_BH1750
  BH1750  lightMeter;
  float   bh1750LightMeterLux = 0;
  int     lampStatus = 0;
  #define LUX_MIN 30
 
#endif

/* CCS811 Configuration------------------------------------------------------*/
#ifdef USE_CCS811
  Adafruit_CCS811 ccs811;
  float ccs811temp = 0;
  float ccs811CO2 = 0;
  float ccs811Tvoc = 0;
#endif

/* Soil Humidity Configuration-----------------------------------------------*/
#ifdef USE_SOIL_SENSOR
const int sensorPin = 34;       //PIN USED BY THE SENSOR
  float valueRead;              //VARIABLE THAT STORE THE PERCENTAGE OF SOIL MOISTURE
  float analogSoilDry = 4000;   //VALUE MEASURED WITH DRY SOIL (YOU CAN MAKE TESTS AND ADJUST THIS VALUE)
  float analogSoilWet = 1000;   //VALUE MEASURED WITH WET SOIL (YOU CAN DO TESTS AND ADJUST THIS VALUE)
  float percSoilDry = 0;        //LOWER PERCENTAGE OF DRY SOIL (0% - DO NOT CHANGE)
  float percSoilWet = 100;      //HIGHER PERCENTAGE OF WET SOIL (100% - DO NOT CHANGE)
  int   waterPumpStatus = 0;
#endif

/* Outputs Configuration-----------------------------------------------*/
const int waterPump = 25;       // Water Pump connected to the relay on pin 25
const int lamp = 33;           // Lamp connected to the relay on pin 33

/* Main function ------------------------------------------------------------*/
/*
   @brief This function is used to initial setup to the project
*/
void setup() {
  // put your setup code here, to run once:

  // Setting the Console Serial Communication
  Serial.begin(serialDataRate);
  Serial.println(SEPARATOR_LINE);
  Serial.print(APP_NAME );
  Serial.print("_");
  Serial.println(APP_VERSION);
  Serial.println(SEPARATOR_LINE);

  // Setting the I2C Serial Communication
  Wire.begin();
  
#ifdef USE_BH1750
  lightMeter.begin();
#endif

#ifdef USE_CCS811
  // Setting the CCS811
  if (!ccs811.begin()) {
    Serial.println("Falha no sensor CCS811!");
    while (1);
  }
  //calibrate temperature sensor
  while (!ccs811.available());
  float temp = ccs811.calculateTemperature();
  ccs811.setTempOffset(temp - 25.0);
#endif

  // Configuring the relays control
  pinMode(waterPump, OUTPUT);
  pinMode(lamp, OUTPUT);

  // Init the wifi connection
  initWifi();
}

/*
   @brief This function is the main operational loop
*/
void loop() {
  // put your main code here, to run repeatedly:

  // Normally Open configuration, send LOW signal to let current flow
  // (if you're usong Normally Closed configuration send HIGH signal)
  digitalWrite(waterPump, LOW);
  digitalWrite(lamp, LOW);

#ifdef USE_DHT
  // Getting DHT sensor data
  dhtTemperature = dht.readTemperature();
  dhtHumidity = dht.readHumidity();
#endif

#ifdef USE_BH1750
  // Getting the BH1750 sensor data
  bh1750LightMeterLux = lightMeter.readLightLevel();

  // Checkin if it is dark and turn on the lamp
  if(bh1750LightMeterLux < LUX_MIN){
    digitalWrite(lamp, HIGH);
    lampStatus = 1;
  }
  else{
    digitalWrite(lamp, LOW);
    lampStatus = 0;
  }
#endif

#ifdef USE_SOIL_SENSOR
  // Geting the Soil Moisture sensor data
  //valueRead = constrain(analogRead(sensorPin), analogSoilWet, analogSoilDry); //KEEP valueRead WITHIN THE RANGE (BETWEEN analogSoilWet AND analogSoilDry)
  //valueRead = map(valueRead, analogSoilWet, analogSoilDry, percSoilWet, percSoilDry); //EXECUTE THE "map" FUNCTION ACCORDING TO THE PAST PARAMETERS
  valueRead = analogRead(sensorPin);
#endif

#ifdef USE_CCS811
  // Getting the CSS811 sensor data
  if (ccs811.available()) {
    if (!ccs811.readData()) {
      ccs811temp = ccs811.calculateTemperature();
      ccs811CO2 = ccs811.geteCO2();
      ccs811Tvoc = ccs811.getTVOC();
    }
  }
#endif

#ifdef USE_DHT
  // Print the data into the Console Serial Communication
  Serial.print("temperatura: ");
  Serial.print(dhtTemperature);
  Serial.print("\r\n");

  Serial.print("umidade: ");
  Serial.print(dhtHumidity);
  Serial.print("\r\n");
#endif

#ifdef USE_BH1750
  Serial.print("luminosidade: ");
  Serial.print(bh1750LightMeterLux);
  Serial.print(" Status Lampada: ");
  Serial.print(lampStatus);
  Serial.print("\r\n");
#endif

#ifdef USE_SOIL_SENSOR
  Serial.print("Umidade do solo: ");
  Serial.print(valueRead);
  Serial.print("%");
  Serial.print(" Status Bomba d'agua: ");
  Serial.print(waterPumpStatus);
  Serial.print("\r\n");
#endif

#ifdef USE_CCS811
  Serial.print("CO2: ");
  Serial.print(ccs811CO2);
  Serial.print("ppm, TVOC: ");
  Serial.print(ccs811Tvoc);
  Serial.print("ppb Temp:");
  Serial.println(ccs811temp);
#endif

  // Checking if the WiFi is connected
  if (WiFi.status() != WL_CONNECTED) {
    reconnecWifi();
    return;
  }

#ifdef USE_THINGSBOARD
  // Checking if it is connected to the Thingsboard Server
  if (!thingsBoard.connected()) {
    thingsBoard.connect(THINGSBOARD_SERVER, TOKEN);
  }
  Serial.print("Conectando ao ThingsBoard \n");
#endif

  // Sending Sensor data to the Thingsboard Plataform
#ifdef USE_DHT
  #ifdef USE_THINGSBOARD
    thingsBoard.sendTelemetryFloat("Temperatura", dhtTemperature);
    thingsBoard.sendTelemetryFloat("Umidade", dhtHumidity);
  #endif
#endif

#ifdef USE_BH1750
  #ifdef USE_THINGSBOARD
    thingsBoard.sendTelemetryFloat("Luminosidade", bh1750LightMeterLux);
    thingsBoard.sendTelemetryInt  ("Lampada", lampStatus);
  #endif
#endif

#ifdef USE_SOIL_SENSOR
  #ifdef USE_THINGSBOARD
    thingsBoard.sendTelemetryFloat("Solo", valueRead);
    thingsBoard.sendTelemetryInt  ("Bomba", waterPumpStatus);
  #endif
#endif

#ifdef USE_CCS811
  #ifdef USE_THINGSBOARD
    thingsBoard.sendTelemetryFloat("CO2", ccs811CO2);
    thingsBoard.sendTelemetryFloat("TVOC", ccs811Tvoc);
  #endif
#endif

  Serial.print("Dados Enviados! \n");

  delay(5000);
}

/* Auxiliary function ----------------------------------------------------------*/

/* WiFi function ---------------------------------------------------------------*/
/*
   @brief Initializes the wifi connection
*/
void initWifi(void) {
  Serial.println("Conectando Wifi...");
  WiFi.begin(wifiSsid, wifiPass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi Conectado!");
  delay(2000);
}

/*
   @brief Reinitialize the wifi connection
*/
void reconnecWifi() {
  Serial.println("Reconectando...");
  WiFi.begin(wifiSsid, wifiPass);
  delay(2000);
}

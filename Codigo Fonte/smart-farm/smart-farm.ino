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
/* Private includes ----------------------------------------------------------*/
#include "Arduino.h"
#include "DHT.h"              // Temperature and Humidity Library
#include "BH1750.h"           // luminosity Library
#include "WiFi.h"             // WiFi Library
#include "Adafruit_CCS811.h"  // CO2 Sensero Library
#include "ThingsBoard.h"      // The Things Board Library
#include "Wire.h"             // I2C Communication Library

/* Board Configuration--------------------------------------------------------*/
int serialDataRate = 115200;

/* WiFi Configuration---------------------------------------------------------*/
char wifiSsid[] = "SSID_WiFi"; // This is the WiFi network name!
char wifiPass[] = "WiFi_Password"; // This is the wifi password!
WiFiClient espClient;

/* ThingsBoard Configuration--------------------------------------------------*/
ThingsBoard thingsBoard(espClient);
#define THINGSBOARD_SERVER  "demo.thingsboard.io"
#define TOKEN "TOKEN"

/* DHT Configuration----------------------------------------------------------*/
DHT dht (13, DHT11); // The sensor is connect under the pin 13
float dhtTemperature = 0;
float dhtHumidity = 0;

/* BH1750 Configuration------------------------------------------------------*/
BH1750 bh1750;
float  bh1750LightMeterLux = 0;

/* CCS811 Configuration------------------------------------------------------*/
Adafruit_CCS811 ccs811;
float ccs811temp = 0;
float ccs811CO2 = 0;
float ccs811Tvoc = 0;

/* Soil Humidity Configuration-----------------------------------------------*/
const int sensorPin = A0;  //PIN USED BY THE SENSOR

int valueRead;              //VARIABLE THAT STORE THE PERCENTAGE OF SOIL MOISTURE

int analogSoilDry = 400;    //VALUE MEASURED WITH DRY SOIL (YOU CAN MAKE TESTS AND ADJUST THIS VALUE)
int analogSoilWet = 150;    //VALUE MEASURED WITH WET SOIL (YOU CAN DO TESTS AND ADJUST THIS VALUE)
int percSoilDry = 0;        //LOWER PERCENTAGE OF DRY SOIL (0% - DO NOT CHANGE)
int percSoilWet = 100;      //HIGHER PERCENTAGE OF WET SOIL (100% - DO NOT CHANGE)

/* Main function ------------------------------------------------------------*/
/*
   @brief This function is used to initial setup to the project
*/
void setup() {
  // put your setup code here, to run once:

  // Setting the Console Serial Communication
  Serial.begin(serialDataRate);
  Serial.println("Teste de comunicacao do ESP32-Wroom");

  // Setting the I2C Serial Communication
  Wire.begin();

  // Setting the CCS811
  if (!ccs811.begin()) {
    Serial.println("Falha no sensor CCS811!");
    while (1);
  }

  //calibrate temperature sensor
  while (!ccs811.available());
  float temp = ccs811.calculateTemperature();
  ccs811.setTempOffset(temp - 25.0);

  // Init the wifi connection
  initWifi();
}

/*
   @brief This function is the main operational loop
*/
void loop() {
  // put your main code here, to run repeatedly:

  while (1) {
    Serial.print("Teste: \r\n");
    delay(2000);
  }
  // Getting DHT sensor data
  dhtTemperature = dht.readTemperature();
  dhtHumidity = dht.readHumidity();

  // Getting the BH1750 sensor data
  bh1750LightMeterLux = bh1750.readLightLevel();

  // Geting the Soil Moisture sensor data
  valueRead = constrain(analogRead(sensorPin), analogSoilWet, analogSoilDry); //KEEP valueRead WITHIN THE RANGE (BETWEEN analogSoilWet AND analogSoilDry)
  valueRead = map(valueRead, analogSoilWet, analogSoilDry, percSoilWet, percSoilDry); //EXECUTE THE "map" FUNCTION ACCORDING TO THE PAST PARAMETERS

  // Getting the CSS811 sensor data
  if (ccs811.available()) {
    if (!ccs811.readData()) {
      ccs811temp = ccs811.calculateTemperature();
      ccs811CO2 = ccs811.geteCO2();
      ccs811Tvoc = ccs811.getTVOC();
    }
  }

  // Print the data into the Console Serial Communication
  Serial.print("temperatura: ");
  Serial.print(dhtTemperature);
  Serial.print("\r\n");

  Serial.print("umidade: ");
  Serial.print(dhtHumidity);
  Serial.print("\r\n");

  Serial.print("luminosidade: ");
  Serial.print(bh1750LightMeterLux);;
  Serial.print("\r\n");

  Serial.print("Umidade do solo: ");
  Serial.print(valueRead);
  Serial.println("%");
  Serial.print("\r\n");

  Serial.print("CO2: ");
  Serial.print(ccs811CO2);
  Serial.print("ppm, TVOC: ");
  Serial.print(ccs811Tvoc);
  Serial.print("ppb Temp:");
  //Serial.println(ccs811temp);

  // Checking if the WiFi is connected
  if (WiFi.status() != WL_CONNECTED) {
    reconnecWifi();
    return;
  }

  // Checking if it is connected to the Thingsboard Server
  if (!thingsBoard.connected()) {
    thingsBoard.connect(THINGSBOARD_SERVER, TOKEN);
  }
  Serial.print("Conectando ao ThingsBoard \n");

  // Sending Sensor data to the Thingsboard Plataform
  thingsBoard.sendTelemetryFloat("Temperatura", dhtTemperature);
  thingsBoard.sendTelemetryFloat("Umidade", dhtHumidity);
  thingsBoard.sendTelemetryFloat("Luminosidade", bh1750LightMeterLux);
  thingsBoard.sendTelemetryFloat("CO2", ccs811CO2);
  thingsBoard.sendTelemetryFloat("TVOC", ccs811Tvoc);
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

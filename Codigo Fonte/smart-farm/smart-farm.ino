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

#define TIME_LOOP   5
#define ONE_SECOND  1000
#define ONE_MINUTE  60 * 1000
#define ONE_HOUR    60 * ONE_MINUTE
#define ONE_DAY     24 * ONE_HOUR

/* Private includes ----------------------------------------------------------*/
#include "Arduino.h"
#include "WiFi.h"              // WiFi Library

#include <WiFiUdp.h>          // Servidor Relógio
#include <HTTPClient.h>       // Postar o código na web
#include "time.h"             // RTC

#include "ThingsBoard.h"     // The Things Board Library
#include "DHT.h"             // Temperature and Humidity Library
#include "BH1750.h"          // luminosity Library
#include "Adafruit_CCS811.h" // CO2 Sensero Library
#include "Wire.h"            // I2C Communication Library

/* Board Configuration--------------------------------------------------------*/
int serialDataRate = 115200;

/* WiFi Configuration---------------------------------------------------------*/
char wifiSsid[] = "...";     // This is the WiFi network name!
char wifiPass[] = "..."; // This is the wifi password!

unsigned long previousMillis = 0;
unsigned long interval = 5000;

WiFiClient espClient;

/* NTP Configuration ---------------------------------------------------------*/
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -10800; // For UTC -3 = -3*60*60 =
const int daylightOffset_sec = 0;

/* ThingsBoard Configuration--------------------------------------------------*/
ThingsBoard thingsBoard(espClient);
#define THINGSBOARD_SERVER  "..."
#define TOKEN               "..."
#define TIME_THINGSBOARD    (ONE_MINUTE / ONE_SECOND) / TIME_LOOP  // Send data every One minute ->(60*1000/1000)/5 = 12 Loops de 5 segundos
int countToSend = 0;

/* DHT Configuration----------------------------------------------------------*/
DHT dht (13, DHT11); // The sensor is connect under the GPIO 13
float dhtTemperature = 0;
float dhtHumidity = 0;

/* BH1750 Configuration------------------------------------------------------*/
BH1750  lightMeter;
float   bh1750LightMeterLux = 0;
int     lampStatus = 0;
#define LUX_MIN 30

/* CCS811 Configuration------------------------------------------------------*/
Adafruit_CCS811 ccs811;
float ccs811temp = 0;
float ccs811CO2 = 0;
float ccs811Tvoc = 0;

/* Soil Humidity Configuration-----------------------------------------------*/
const int sensorPin = 34;     //PIN USED BY THE SENSOR
float valueRead;              //VARIABLE THAT STORE THE PERCENTAGE OF SOIL MOISTURE
float analogSoilDry = 1300;   //VALUE MEASURED WITH DRY SOIL (YOU CAN MAKE TESTS AND ADJUST THIS VALUE)
float analogSoilWet = 0;      //VALUE MEASURED WITH WET SOIL (YOU CAN DO TESTS AND ADJUST THIS VALUE)
float percSoilDry = 0;        //LOWER PERCENTAGE OF DRY SOIL (0% - DO NOT CHANGE)
float percSoilWet = 100;      //HIGHER PERCENTAGE OF WET SOIL (100% - DO NOT CHANGE)

#define SOIL_WET    80        // Percentage to turn of the pump
#define SOIL_DRY    20        // Percentage to turn on the pump
int   waterPumpStatus = 0;

/* Outputs Configuration-----------------------------------------------*/
const int waterPump = 33;       // Water Pump connected to the relay on pin 25
const int lamp = 25;            // Lamp connected to the relay on pin 33

/* NTP TIME ----------------------------------------------------------*/

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

  // Setting the BH1750
  lightMeter.begin();

  // Setting the CCS811
  if (!ccs811.begin()) {
    Serial.println("Falha no sensor CCS811!");
    while (1);
  }
  //calibrate temperature sensor
  while (!ccs811.available());
  float temp = ccs811.calculateTemperature();
  ccs811.setTempOffset(temp - 25.0);

  // Configuring the relays control
  // Normally Open configuration, send LOW signal to let current flow
  // (if you're usong Normally Closed configuration send HIGH signal)
  pinMode(waterPump, OUTPUT);
  pinMode(lamp, OUTPUT);
  digitalWrite(waterPump, HIGH);
  digitalWrite(lamp, HIGH);

  // Init the wifi connection
  initWifi();

  // Init and get Time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

/*
   @brief This function is the main operational loop
*/
void loop() {
  // put your main code here, to run repeatedly:

  //Getting temperature and Humidity data
  getDhtData();

  // Getting the light data and activate the lamp
  getBh1750Data();

  // Getting the Soil Humidity data and activate the pump
  getSoilSensorData();

  // Getting the Air Quality data
  getCcs811Data();

  // Print data into the Serial Port
  printData();

  // Checking if the WiFi is connected
  if (WiFi.status() != WL_CONNECTED) {
    reconnecWifi();
    return;
  }

  // Sendign Data
  sendDataToServer();

  delay(TIME_LOOP * ONE_SECOND);
}

/* Auxiliary function ----------------------------------------------------------*/

/* Time Functions --------------------------------------------------------------*/

/* Sensors function ------------------------------------------------------------*/
/*
   @brief Gets the Temp/Hum data
*/
void getDhtData(void) {
  // Getting DHT sensor data
  dhtTemperature = dht.readTemperature();
  dhtHumidity = dht.readHumidity();
}

/*
   @brief Gets the light data and activate the lamp.
*/
void getBh1750Data() {
  // Getting the BH1750 sensor data
  bh1750LightMeterLux = lightMeter.readLightLevel();

  // Checkin if it is dark and turn on the lamp
  if (bh1750LightMeterLux < LUX_MIN) {
    //digitalWrite(lamp, LOW);
    lampStatus = 1;
  }
  else {
    //digitalWrite(lamp, HIGH);
    lampStatus = 0;
  }
}

/*
   @brief Gets the Soil Humidity data and activate the pump
*/
void getSoilSensorData(void) {
  // Geting the Soil Moisture sensor data

  // Soil Wet: analogRead ~= 0
  // Soil Dry: analogRead ~=1300
  valueRead = analogRead(sensorPin);

  //EXECUTE THE "map" FUNCTION ACCORDING TO THE PAST PARAMETERS
  valueRead = map(valueRead, analogSoilWet, analogSoilDry, percSoilWet, percSoilDry);

  // Checking if it has to turn on the water pump
  if (valueRead > SOIL_WET) {
    digitalWrite(waterPump, HIGH);
    waterPumpStatus = 0;
  } else if (valueRead < SOIL_DRY) {
    digitalWrite(waterPump, LOW);
    waterPumpStatus = 1;
  }
}

/*
   @brief Getting the Air Quality data
*/
void getCcs811Data(void) {
  // Getting the CSS811 sensor data
  if (ccs811.available()) {
    if (!ccs811.readData()) {
      ccs811temp = ccs811.calculateTemperature();
      ccs811CO2 = ccs811.geteCO2();
      ccs811Tvoc = ccs811.getTVOC();
    }
  }
}

/*
   @brief Print data into the Serial Port
*/
void printData(void) {
  // Print the data into the Console Serial Communication

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
  } else {
    Serial.println(&timeinfo, "%d/%m/%YT%H:%M:%S");
  }

  Serial.print("temperatura: ");
  Serial.print(dhtTemperature);
  Serial.print("\r\n");

  Serial.print("umidade: ");
  Serial.print(dhtHumidity);
  Serial.print("\r\n");

  Serial.print("luminosidade: ");
  Serial.print(bh1750LightMeterLux);
  Serial.print(" Status Lampada: ");
  Serial.print(lampStatus);
  Serial.print("\r\n");

  Serial.print("Umidade do solo: ");
  Serial.print(valueRead);
  Serial.print("%");
  Serial.print(" Status Bomba d'agua: ");
  Serial.print(waterPumpStatus);
  Serial.print("\r\n");

  Serial.print("CO2: ");
  Serial.print(ccs811CO2);
  Serial.print("ppm, TVOC: ");
  Serial.print(ccs811Tvoc);
  Serial.print("ppb Temp:");
  Serial.println(ccs811temp);
}

/* Send Data To Server functions -----------------------------------------------*/

/*
   @brief Sending data to the Servers
*/
void sendDataToServer(void) {
  countToSend++;
  if (countToSend >= TIME_THINGSBOARD) {

    // Sending data to the Thingsboard
    sendDataThingsBoard();

    // Sending data to the Intescity
    sendDataInterscity();
    Serial.println("Dados Enviados!");
    // Clearing the counter and wait to the next loop
    countToSend = 0;
  }
}


/*
   @brief Sending data to the Thingsboard
*/
void sendDataThingsBoard(void) {

  // Checking if it is connected to the Thingsboard Server
  if (!thingsBoard.connected()) {
    thingsBoard.connect(THINGSBOARD_SERVER, TOKEN);
  }
  Serial.println("Conectando ao ThingsBoard \n");

  // Sending Sensor data to the Thingsboard Plataform

  thingsBoard.sendTelemetryFloat("Temperatura", dhtTemperature);
  thingsBoard.sendTelemetryFloat("Umidade", dhtHumidity);

  thingsBoard.sendTelemetryFloat("Luminosidade", bh1750LightMeterLux);
  //thingsBoard.sendTelemetryInt  ("Lampada", lampStatus);

  thingsBoard.sendTelemetryFloat("Solo", valueRead);
  thingsBoard.sendTelemetryInt  ("Bomba", waterPumpStatus);

  thingsBoard.sendTelemetryFloat("CO2", ccs811CO2);
  thingsBoard.sendTelemetryFloat("TVOC", ccs811Tvoc);
}

/*
  @brief Sending data to the Intescity
*/
void sendDataInterscity(void) {
  char dataFormata[30];
  char dado[2048];
  String json;
  struct tm timeinfo;

  Serial.println("Conectando ao Interscity \n");

  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  strftime(dataFormata, 30, "%d/%m/%YT%H:%M:%S", &timeinfo);

  sprintf(dado, "{\"urban_smart_farm_temp\":\"%.2f\",\"urban_smart_farm_umid\":\"%.2f\",\"urban_smart_farm_lux\":\"%.2f\",\"urban_smart_farm_soil\":\"%.2f\",\"urban_smart_farm_pump\":\"%d\",\"urban_smart_farm_co2\":\"%.2f\",\"urban_smart_farm_tvoc\":\"%.2f\",\"date\":\"%s\"}", dhtTemperature, dhtHumidity, bh1750LightMeterLux, valueRead, waterPumpStatus, ccs811CO2, ccs811Tvoc, dataFormata);

  json = dado;

  String dado_inicial = "{\"data\":{\"environment_monitoring\":[";
  String dado_final = json + "]}}";
  json = dado_inicial + dado_final;
  Serial.println(json);

  HTTPClient http;
  http.begin("https://api.playground.interscity.org/adaptor/resources/.../data");  //Specify destination for HTTP request
  http.addHeader("Content-Type", "application/json");             //Specify content-type header

  int httpResponseCode = http.POST(json);
  //Send the actual POST request

  if (httpResponseCode == 201) {
    Serial.print("Retorno"); Serial.print("\t");
    Serial.println(httpResponseCode);   //Print return code
  }

  else
  {
    Serial.print("Error on sending request: ");
    Serial.println(httpResponseCode);
    Serial.println("---------------------------------");
    delay(100);  //Send a request every 1 second
  }

  http.end();  //Free resources

  json = "";
}


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
  // Loop until we're reconnected
  Serial.println("Reconectando...");
  WiFi.begin(wifiSsid, wifiPass);
  delay(2000);
}

/**
 ******************************************************************************
 * @file        smart-farm.ino     
 * @author      Airton Toyofuku
 * @copyright   None
 * @date        11.07.2022
 * @version     1.0
 * @brief       Arduino Sketch referring to the biohealth-urban-smart-farming 
 *              project, developed for the IoT discipline of the PROFESSIONAL 
 *              MASTER'S IN APPLIED COMPUTING at IPT.
 ******************************************************************************
 */
/* Private includes ----------------------------------------------------------*/
#include "Arduino.h"
#include "DHT.h"
#include "WiFi.h"
#include "ThingsBoard.h"

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
DHT dht (13, DHT22); // The sensor is connect under the pin 13
float dhtTemperature = 0;
float dhtHumidity = 0;

/* Main function ------------------------------------------------------------*/
/*
 * @brief This function is used to initial setup to the project
 */
void setup() {
  // put your setup code here, to run once:

  // Setting the Serial Communication
  Serial.begin(serialDataRate);
  Serial.println("Teste de comunicacao do ESP32-Wroom");

  // Init the wifi connection
  initWifi();
}

/*
 * @brief This function is the main operational loop
 */
void loop() {
  // put your main code here, to run repeatedly:

  // Getting DHT sensor data
  dhtTemperature = dht.readTemperature();
  dhtHumidity = dht.readHumidity();

  Serial.print("temperatura: ");
  Serial.print(dhtTemperature);
  Serial.print(" - umidade: ");
  Serial.print(dhtHumidity);
  Serial.print("\n");

  // Checking if the WiFi is connected
  if(WiFi.status() != WL_CONNECTED){
    reconnecWifi();
    return;
  }

  // Checking if it is connected to the Thingsboard Server
  if(!thingsBoard.connected()){
    thingsBoard.connect(THINGSBOARD_SERVER, TOKEN);
  }
  Serial.print("Conectando ao ThingsBoard \n");

  // Sending Sensor data to the Thingsboard Plataform
  thingsBoard.sendTelemetryFloat("Temperatura",dhtTemperature);
  thingsBoard.sendTelemetryFloat("Umidade",dhtHumidity);
  Serial.print("Dados Enviados! \n");

  delay(5000);
}

/* Auxiliary function ----------------------------------------------------------*/

/* WiFi function ---------------------------------------------------------------*/
/*
 * @brief Initializes the wifi connection
 */
void initWifi(void){
  Serial.println("Conectando Wifi...");
  WiFi.begin(wifiSsid, wifiPass);
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi Conectado!");
  delay(2000);
}

/*
 * @brief Reinitialize the wifi connection
 */
void reconnecWifi(){
  Serial.println("Reconectando...");
  WiFi.begin(wifiSsid, wifiPass);
  delay(2000);
}

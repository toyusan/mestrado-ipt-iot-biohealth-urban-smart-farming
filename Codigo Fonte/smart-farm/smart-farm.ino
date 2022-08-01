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
#include <time.h>             // RTC
#include <sys/time.h>         // RTC

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
float analogSoilDry = 4000;   //VALUE MEASURED WITH DRY SOIL (YOU CAN MAKE TESTS AND ADJUST THIS VALUE)
float analogSoilWet = 1000;   //VALUE MEASURED WITH WET SOIL (YOU CAN DO TESTS AND ADJUST THIS VALUE)
float percSoilDry = 0;        //LOWER PERCENTAGE OF DRY SOIL (0% - DO NOT CHANGE)
float percSoilWet = 100;      //HIGHER PERCENTAGE OF WET SOIL (100% - DO NOT CHANGE)
int   waterPumpStatus = 0;

/* Outputs Configuration-----------------------------------------------*/
const int waterPump = 33;       // Water Pump connected to the relay on pin 25
const int lamp = 25;            // Lamp connected to the relay on pin 33

/* Interscity data-----------------------------------------------------*/
//Cria a estrutura que contem as informacoes da data.
struct  tm data;
char    dado[512], hora[15];
int     contador = 1, num_dados = 20; // Define o número de dados à enviar em cada pacote
String  dado_build, json;
char    data_formatada[64];

/* NTP TIME ----------------------------------------------------------*/
//IPAddress timeServer(129, 6, 15, 28);         // time.nist.gov NTP server
unsigned int  fuso = -3;                       //informe o fuso da sua região
unsigned int  localPort = 2390;                // local port to listen for UDP packets
IPAddress     timeServerIP;                    // time.nist.gov NTP server address
const char*   ntpServerName = "time.nist.gov";
const int     NTP_PACKET_SIZE = 48;            // NTP time stamp is in the first 48 bytes of the message
byte          packetBuffer[ NTP_PACKET_SIZE];  //buffer to hold incoming and outgoing packets
WiFiUDP udp;

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

  // Init NTP time
  //udp.begin(localPort);
  //ntp();
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

  // Sending data to the Thingsboard
  sendDataThingsBoard();

  // Sending data to the Intescity
  //sendDataInterscity();

  delay(TIME_LOOP * ONE_SECOND);
}

/* Auxiliary function ----------------------------------------------------------*/

/* Sensors function ---------------------------------------------------------------*/
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
    digitalWrite(lamp, LOW);
    lampStatus = 1;
  }
  else {
    digitalWrite(lamp, HIGH);
    lampStatus = 0;
  }
}

/*
   @brief Gets the Soil Humidity data and activate the pump
*/
void getSoilSensorData(void) {
  // Geting the Soil Moisture sensor data
  //KEEP valueRead WITHIN THE RANGE (BETWEEN analogSoilWet AND analogSoilDry)
  //valueRead = constrain(analogRead(sensorPin), analogSoilWet, analogSoilDry);

  //EXECUTE THE "map" FUNCTION ACCORDING TO THE PAST PARAMETERS
  //valueRead = map(valueRead, analogSoilWet, analogSoilDry, percSoilWet, percSoilDry);
  valueRead = analogRead(sensorPin);

  // Checking if it has to turn on the water pump
  if (valueRead >= analogSoilDry) {
    digitalWrite(waterPump, LOW);
  } else if (valueRead <= percSoilWet) {
    digitalWrite(waterPump, HIGH);
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

/* ThingsBoard function ---------------------------------------------------------*/
/*
   @brief Sending data to the Thingsboard
*/
void sendDataThingsBoard(void) {

  countToSend++;

  if (countToSend >= TIME_THINGSBOARD) {

    // Checking if it is connected to the Thingsboard Server
    if (!thingsBoard.connected()) {
      thingsBoard.connect(THINGSBOARD_SERVER, TOKEN);
    }
    Serial.println("Conectando ao ThingsBoard \n");

    // Sending Sensor data to the Thingsboard Plataform

    thingsBoard.sendTelemetryFloat("Temperatura", dhtTemperature);
    thingsBoard.sendTelemetryFloat("Umidade", dhtHumidity);

    thingsBoard.sendTelemetryFloat("Luminosidade", bh1750LightMeterLux);
    thingsBoard.sendTelemetryInt  ("Lampada", lampStatus);

    thingsBoard.sendTelemetryFloat("Solo", valueRead);
    thingsBoard.sendTelemetryInt  ("Bomba", waterPumpStatus);

    thingsBoard.sendTelemetryFloat("CO2", ccs811CO2);
    thingsBoard.sendTelemetryFloat("TVOC", ccs811Tvoc);

    Serial.println("Dados Enviados!");

    // Clearing the counter and wait to the next loop
    countToSend = 0;
  }
}

/*
   @brief Sending data to the Interscity
*/
void sendDataInterscity(void) {
  
  timestamp();
  
  sprintf(dado, "{\"temperatura_DHT22\":\"%.2f\", \"umidade_DHT22\":\"%.2f\",\"date\":\"%s\"}", dhtTemperature, dhtHumidity, data_formatada);
  
  json = dado;
  
  while (json != ""){
    enviar_dado();
  }
}

void enviar_dado() {
  String dado_inicial = "{\"data\":{\"environment_monitoring\":[";
  String dado_final = json + "]}}";
  json = dado_inicial + dado_final;
  Serial.println(json);

  HTTPClient http;
  http.begin("https://api.playground.interscity.org/adaptor/resources/ed53b76f-3ebc-4a15-b760-a5791495be3f/data");  //Specify destination for HTTP request
  http.addHeader("Content-Type", "application/json");             //Specify content-type header
  
  int httpResponseCode = http.POST(json);
  //Send the actual POST request

  if (httpResponseCode == 201){
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

void ntp() { // busca  Network Time Protocol (NTP) time server
  //get a random server from the pool
  
  int cb;
  do {
  WiFi.hostByName(ntpServerName, timeServerIP);
  sendNTPpacket(timeServerIP); // send an NTP packet to a time server
  delay(1000);
  cb = udp.parsePacket();
  Serial.println(cb);
  // wait to see if a reply is available
  } while(!cb);
  
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);

    // converte ntp em UNIX (epoch) usado como referencia
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    secsSince1900 += fuso * 60 * 60;// corrige a hora para o fuso desejado
    // now convert NTP time into everyday time:
    // Unix time starts on Jan 1 1970. Segundos desde 1970 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    unsigned long epoch = secsSince1900 - seventyYears;//  obtem valor em Unix time
    //Serial.print("Unix time = ");//imprime os segundos desde 1970 para obter o valor época
    //Serial.println(epoch);

    timeval tv;//Cria a estrutura temporaria para funcao abaixo.
    tv.tv_sec = epoch;//Atribui minha data atual. Voce pode usar o NTP para isso ou o site Unix Time Stamp
    settimeofday(&tv, NULL);//Configura o RTC para manter a data atribuida atualizada.

    /* imprime hora local
      char hora[30];// concatena a impressão da hora
      int  h, m, s;// hora, minuto, segundos
      h = (epoch  % 86400L) / 3600; // converte unix em hora (86400 equals secs per day)
      m = (epoch % 3600) / 60; // converte unix em minuto (3600 equals secs per minute)
      s = (epoch % 60); // converte unix em segundo
      sprintf( hora, "%02d:%02d:%02d", h, m, s);// concatena os valores h,m,s

      Serial.print("hora local:");
      Serial.println(hora);
    */
  // wait ten seconds before asking for the time again
  //delay(10000);
}

unsigned long sendNTPpacket(IPAddress& address) {

  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
  //*************************************************************
}

void timestamp() {
  //vTaskDelay(pdMS_TO_TICKS(1000));//Espera 1 seg

  time_t tt = time(NULL);//Obtem o tempo atual em segundos. Utilize isso sempre que precisar obter o tempo atual
  data = *gmtime(&tt);//Converte o tempo atual e atribui na estrutura
  //strftime(data_formatada, 64, "%Y-%m-%d %H:%M:%S", &data);//Cria uma String formatada da estrutura "data"
  strftime(data_formatada, 64, "%Y-%m-%dT%H:%M:%SZ", &data);//Cria uma String formatada da estrutura "data" 2021-05-06T00:01:00.000+00:00
  //Serial.println(int32_t(tt));//Mostra na Serial o Unix time
  //String ms = "000";
  //sprintf(hora, "%d%s", int32_t(tt), ms);
  //Serial.print("Data formatada: ");
  //Serial.println(data_formatada);//Mostra na Serial a data formatada
  //Serial.println(hora);
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

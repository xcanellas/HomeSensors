#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#include <Adafruit_BME280.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>

// ESP8266 Number Pins definition
const int Blue_Led = 2;
const int Red_Led = 0;
const int Buzzer = 14;
const int InDig1 = 13;
const int InDig2 = 16;
const int PIR = 12;
const int _SDA = 4;
const int _SCL = 5;

// BME280 SENSOR
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME280 bme; // I2C
float Temperature;
float Pressure;
float Humidity;
float Altitude;
const float TempCalib = -4.3;

// TSL2561 SENSOR
/*I2C Address
   ===========
   The address will be different depending on whether you leave
   the ADDR pin floating (addr 0x39), or tie it to ground or vcc. 
   The default addess is 0x39, which assumes the ADDR pin is floating
   (not connected to anything).  If you set the ADDR pin high
   or low, use TSL2561_ADDR_HIGH (0x49) or TSL2561_ADDR_LOW
   (0x29) respectively.
*/
   
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_LOW, 12345);
float LUX_value;

// Configurar WiFi generada
const char *ssid = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";

IPAddress ip(IP del displositivo); //Ejemplo 192,168,1,245
IPAddress gateway(IP del router);  //Ejemplo 192,168,1,1
IPAddress subnet(255,255,255,0); 

// Use WiFiClient class to create TCP connections
WiFiClient client;
const int httpPort = 80;
const char* host = "IP del host"; //Ejemplo "192.168.1.245"

// PROGRAM VARIABLES 
bool PIR_temp;
bool InDig1_temp;
bool InDig2_temp;
bool PIR_value;
bool InDig1_value;
bool InDig2_value;
bool Alarma;
bool RefrescaAlarma = false;
int SQLConnectionCounter;
const int SQLConnectionTime = 1800;  //in seconds

void setup() {
  
  // initialize digital pins.
  pinMode(Blue_Led, OUTPUT);
  pinMode(Red_Led, OUTPUT);
  pinMode(Buzzer, OUTPUT);
  pinMode(InDig1, INPUT);
  pinMode(InDig2, INPUT);
  pinMode(PIR, INPUT);

  //Clear Leds Pin
  digitalWrite(Red_Led, HIGH);
  digitalWrite(Blue_Led, HIGH);
  
  Alarma = 0;

  Serial.begin(9600); /* begin serial for debug */

  ConectarWifi();

  ArduinoOTA.setHostname("HomeSensor");
  ArduinoOTA.setPassword((const char *)"xxx"); //Set your OTA password
  ArduinoOTA.begin();

  /* BME280 init */
  if (!bme.begin()) {
    Serial.println("Could not connect to BME280 sensor!");
  }
  else{
    Serial.println("-- BME280 Ready --");
  }

  /* TSL2561 init */
  if(!tsl.begin())
  {
    Serial.print("Could not connect to TSL2561 sensor!");
    while(1);
  }
  else{
    Serial.println("-- TSL2561 Ready --");
  }
  
  /* Configuracion sensor TSL2561 */
  configuracion_TSL2561();
  
  Serial.println();

}


void loop() {

//Gestiona las conexiones vía OTA para programar via WIFI
  ArduinoOTA.handle();

//Comprobamos si hay conexion Wifi
  if(WiFi.status() == WL_DISCONNECTED){
    ConectarWifi();
  }
  
// Leer Sensores de seguridad PIR y puertas
  ReadSafetySensors();
  

// Leer sensores hambientales
  Read_BME280();
  Read_TSL2561();
  

  Serial.println("-------------------------------------------");


    //Conexion al servido SQL cada tiempo SQLConnectionTime
  if(SQLConnectionCounter >= SQLConnectionTime || RefrescaAlarma == true){
    EnviarDatosSQL();
    SQLConnectionCounter = 0;
    RefrescaAlarma = false;
    delay(10000);
  }else{
    SQLConnectionCounter = SQLConnectionCounter + 1;
  }
  
  delay(1000);
  
}



/**************************************************************************************/
/**************************************************************************/
/*Activacion de la alarma*/
/**************************************************************************/
void ActivarAlarma(){
  if(Alarma == false){
    RefrescaAlarma = true;
    Alarma = true;
    BIP_Sonido();               // 3 pitidos de advertencia
  }
  
  digitalWrite(Red_Led, LOW); // Led ROJO ON

}

/**************************************************************************/
/*Desactivacion de la alarma*/
/**************************************************************************/
void DesactivarAlarma(){
  if(Alarma == true){
    RefrescaAlarma = true;
    Alarma = false;
  }
  
  digitalWrite(Red_Led, HIGH); // Led ROJO OFF
  analogWrite(Buzzer, 0);   // Sonido Buzzer OFF  
}

/**************************************************************************/
/*Activación de los tres pitidos de alarma*/
/**************************************************************************/
void BIP_Sonido(){
    analogWrite(Buzzer, 500); delay(150);  // Sonido Buzzer ON
    analogWrite(Buzzer, 0); delay(150);  // Sonido Buzzer OFF
    analogWrite(Buzzer, 500); delay(150);  // Sonido Buzzer ON
    analogWrite(Buzzer, 0); delay(150);  // Sonido Buzzer OFF
    analogWrite(Buzzer, 500); delay(150); // Sonido Buzzer ON
    analogWrite(Buzzer, 0); delay(150);  // Sonido Buzzer OFF
}

/**************************************************************************/
/*Leemos los sensores de seguridad PRESENCIA, PUERTAS*/
/**************************************************************************/
void ReadSafetySensors(){
  // Leer sensores de seguridad
  PIR_value = !digitalRead(PIR);
  InDig1_value = digitalRead(InDig1);
  InDig2_value = digitalRead(InDig2);

    
  if (PIR_value == true|| InDig1_value == true || InDig2_value == true){
    ActivarAlarma();
  }
  else{
    DesactivarAlarma();
  }

  Serial.print("PIR = "); Serial.print(PIR_value);
  Serial.print("  InDig1 = "); Serial.print(InDig1_value);
  Serial.print("  InDig2 = "); Serial.println(InDig2_value);
  
}

/**************************************************************************/
/*Lectura de todas las variables del sensor BME280 PRESION HUMEDAD y TEMPERATURA*/
/*y las mostramos por el puerto serie*/
/**************************************************************************/
void Read_BME280() {

    bme.init();
    Temperature = bme.readTemperature() + TempCalib;
    Humidity = bme.readHumidity();
    Pressure = bme.readPressure() / 100.0F;
    Altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);
    
    Serial.print("Temperature = ");
    Serial.print(Temperature);
    Serial.println(" *C");

    Serial.print("Pressure = ");
    Serial.print(Pressure);
    Serial.println(" hPa");

    Serial.print("Approx. Altitude = ");
    Serial.print(Altitude);
    Serial.println(" m");

    Serial.print("Humidity = ");
    Serial.print(Humidity);
    Serial.println(" %");

}

/**************************************************************************/
/*Configures the gain and integration time for the TSL2561*/
/**************************************************************************/
void configuracion_TSL2561(void){
  /* You can also manually set the gain or enable auto-gain support */
  // tsl.setGain(TSL2561_GAIN_1X);      /* No gain ... use in bright light to avoid sensor saturation */
  // tsl.setGain(TSL2561_GAIN_16X);     /* 16x gain ... use in low light to boost sensitivity */
  tsl.enableAutoRange(true);            /* Auto-gain ... switches automatically between 1x and 16x */
  
  /* Changing the integration time gives you better sensor resolution (402ms = 16-bit data) */
  //tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);      /* fast but low resolution */
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  /* medium resolution and speed   */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);  /* 16-bit data but slowest conversions */

  /* Update these values depending on what you've set above! */  
//  Serial.println("------------------------------------");
//  Serial.print  ("Gain:         "); Serial.println("Auto");
//  Serial.print  ("Timing:       "); Serial.println("13 ms");
//  Serial.println("------------------------------------");
}

/**************************************************************************/
/*Lecutura del sensor TSL2561*/
/**************************************************************************/
void Read_TSL2561(){
  sensors_event_t event;
  tsl.getEvent(&event);
  LUX_value = event.light;
  /* Display the results (light is measured in lux) */

  Serial.print("Lux = "); Serial.println(LUX_value); 
}

/**************************************************************************/
/*Enviamos todos los datos a la base de datos mediante un PHP*/
/**************************************************************************/
void EnviarDatosSQL(){
  
   if (client.connect(host, httpPort)) {
     Serial.println("SQL Connection done");
     client.print("GET /HomeSensors.php?Temperatura=");
     client.print(Temperature);
     client.print("&");
     client.print("Humedad=");
     client.print(Humidity);
     client.print("&");
     client.print("Presion=");
     client.print(Pressure);
     client.print("&");
     client.print("Altitud=");
     client.print(Altitude);
     client.print("&");
     client.print("Lux=");
     client.print(LUX_value);
     client.print("&");
     client.print("Pir=");
     client.print(PIR_value);
     client.print("&");
     client.print("Door1=");
     client.print(InDig1_value);
     client.print("&");
     client.print("Door2=");
     client.print(InDig2_value);
     client.println(" HTTP/1.0");
     client.println();

     if (!client.connected()) {
      Serial.println();
      Serial.println("disconnecting.");
      client.stop();
    }
  
   }else {
     Serial.println("connection failed");
   }
}

/**************************************************************************/
/*Conexion al wifi*/
/**************************************************************************/
void ConectarWifi(){
  WiFi.mode(WIFI_STA);
  WiFi.config(ip, gateway, subnet);
  WiFi.begin(ssid, password);
  Serial.print("Conectando a:\t");
  Serial.println(ssid); 
 
  // Esperar a que nos conectemos
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(200); 
    Serial.print('.');
  }
 
  // Mostrar mensaje de exito y dirección IP asignada
  Serial.println("Conexión establecida");  
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());
}

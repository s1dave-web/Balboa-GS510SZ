  /*
 *    
 *    Main board: Wemos D1 mini - esp8266
 *  
 *    SPA display controller for Balboa system GS510SZ 
 *    
 */
    

#include <WiFiClient.h>
#ifdef ESP32
#include <WebServer.h>
#include <WiFi.h>
#else
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>  
#endif
#include <ElegantOTA.h>                  // https://github.com/ayushsharma82/ElegantOTA
#include <ArduinoHA.h>                   // https://github.com/dawidchyrzynski/arduino-home-assistant
#include <Balboa_GS_Interface.h>        // https://github.com/MagnusPer/Balboa-GS510SZ    

#ifdef ESP32
#define setClockPin 1 // CHANGE ME
#define setReadPin  1 // CHANGE ME
#define setWritePin 1 // CHANGE ME 
#else
#define setClockPin D1  
#define setReadPin  D2 
#define setWritePin D8  
#endif

//Constants
const char *wifi_ssid                    = "";          // WiFi SSID
const char *wifi_pwd                     = "";          // WiFi Password 
const char *wifi_hostname                = "SPA";
const char* mqtt_server                  = "";           // MQTT Boker IP, your home MQTT server eg Mosquitto on RPi, or some public MQTT
const int mqtt_port                      = 1883;        // MQTT Broker PORT, default is 1883 but can be anything.
const char *mqtt_user                    = "";          // MQTT Broker User Name
const char *mqtt_pwd                     = "";          // MQTT Broker Password 

//Globals 
bool debug                               = true;    // If true activate debug values to write to serial port

const unsigned long ReportTimerMillis    = 30000;   // Timer in milliseconds to report mqtt topics 
unsigned long ReportTimerPrevMillis      = 0;       // Store previous millis

byte mac[] = {0x00, 0x10, 0xFA, 0x6E, 0x32, 0x4A};  // Leave this value, unless you own multiple hot tubs


// MQTT Constants
const char* mqtt_Subscribe_write_topic      = "SPA/Write"; 
const char* mqtt_Subscribe_updateTemp_topic = "SPA/UpdateTemp";

//Initialize components
WiFiClient espClient;                                           // Setup WiFi client definition WiFi
HADevice device(mac, sizeof(mac));
HAMqtt mqtt(espClient, device, 30);
BalboaInterface Balboa(setClockPin, setReadPin, setWritePin);   // Setup Balboa interface 
#ifdef ESP32
WebServer server(80);
#else
ESP8266WebServer server(80);
#endif

HASensor display("Display");
HASensorNumber waterTemp("waterTemp", HANumber::PrecisionP1);
HASensorNumber setTemp("setTemp", HANumber::PrecisionP1);
HABinarySensor heater("Heater");
HABinarySensor pump1("Pump1");
HABinarySensor lights("Lights");
HAButton pump1Button("Pump1");
HAButton lightsButton("Lights");
HAButton tempUpButton("TempUp");
HAButton tempDownButton("TempDown");
HAButton modeButton("Mode");
HAButton resetButton("Reset");
HAButton stopButton("Stop");

HAHVAC hvac(
  "spa",
  HAHVAC::TargetTemperatureFeature
);

void onTargetTemperatureCommand(HANumeric temperature, HAHVAC* sender) {
    float temperatureFloat = temperature.toFloat();

    Serial.print("Target Temperature: ");
    Serial.println(temperatureFloat);

    Balboa.updateTemperature(temperatureFloat);

    sender->setTargetTemperature(temperature); // report target temperature back to the HA panel
}

 
/**************************************************************************/
/* Setup                                                                  */
/**************************************************************************/

void setup() {
  
  if (debug) { Serial.begin(115200); Serial.println("Welcome to SPA - Balboa system GS501SZ");}
  setup_wifi();
  setup_HA();
  Serial.begin(115200);
  Balboa.begin();

  server.on("/", []() {
    server.send(200, "text/plain", "Welcome to SPA - Balboa system GS501SZ");
  });
  
  ElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
  if (debug) { Serial.println("HTTP server started"); }

}


/**************************************************************************/
/* Setup WiFi connection                                                  */
/**************************************************************************/

void setup_wifi() {

    /*  WiFi status return values and meaning 
        WL_IDLE_STATUS      = 0,
        WL_NO_SSID_AVAIL    = 1,
        WL_SCAN_COMPLETED   = 2,
        WL_CONNECTED        = 3,
        WL_CONNECT_FAILED   = 4,
        WL_CONNECTION_LOST  = 5,
        WL_WRONG_PASSWORD   = 6,
        WL_DISCONNECTED     = 7 */
  
    if (debug){ Serial.print("WiFi.status(): "); Serial.println(WiFi.status()); }
    
    int WiFi_retry_counter = 0;
    WiFi.mode(WIFI_STA);
    WiFi.hostname(wifi_hostname);
    #ifdef ESP32
    WiFi.setSleep(false);
    #else
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    #endif
    WiFi.begin(wifi_ssid, wifi_pwd);
    
    // Loop until reconnected or max retry then restart
    while (WiFi.status() != WL_CONNECTED){
        WiFi_retry_counter ++;
        if (WiFi_retry_counter == 30) {ESP.restart();}  
        if (debug){ Serial.print("WiFi.status(): "); Serial.print(WiFi.status()); 
                    Serial.print("   WiFi retry: "); Serial.println(WiFi_retry_counter); } 
        delay(1000);
    }
    
    if (debug){ Serial.print("WiFi connected: ");Serial.println(WiFi.localIP());}

}


void setup_HA() {
    device.setName("StrongSpa G2");
    device.setSoftwareVersion("1.5");
    device.setManufacturer("Balboa");
    device.setModel("GS501SZ");

    waterTemp.setUnitOfMeasurement("°C");
    waterTemp.setDeviceClass("temperature");
    waterTemp.setName("Water Temperature");

    setTemp.setUnitOfMeasurement("°C");
    setTemp.setDeviceClass("temperature");
    setTemp.setIcon("mdi:temperature-celsius");
    setTemp.setName("Target Temperature");

    display.setName("Display");
    display.setIcon("mdi:hot-tub");

    heater.setName("Heater");
    heater.setIcon("mdi:heating-coil");

    pump1.setName("Pump");
    pump1.setIcon("mdi:pump");

    pump1Button.setName("Pump");
    pump1Button.setIcon("mdi:pump");
    pump1Button.onCommand(onButtonPress);

    lights.setName("Lights");
    lights.setIcon("mdi:lightbulb-group");

    lightsButton.setName("Lights");
    lightsButton.setIcon("mdi:lightbulb-group");
    lightsButton.onCommand(onButtonPress);

    tempUpButton.setName("Temperature Up");
    tempUpButton.setIcon("mdi:thermometer-chevron-up");
    tempUpButton.onCommand(onButtonPress);

    tempDownButton.setName("Temperature Down");
    tempDownButton.setIcon("mdi:thermometer-chevron-down");
    tempDownButton.onCommand(onButtonPress);

    modeButton.setName("Mode");
    modeButton.setIcon("mdi:auto-mode");
    modeButton.onCommand(onButtonPress);

    resetButton.setName("Reset ESP");
    resetButton.setIcon("mdi:restart");
    resetButton.onCommand(onButtonPress);

    stopButton.setName("Stop Command");
    stopButton.setIcon("mdi:close-octagon");
    stopButton.onCommand(onButtonPress);

    hvac.setName("Temperature Control");
    hvac.setMinTemp(26);
    hvac.setMaxTemp(40);
    hvac.setTempStep(0.5);
    hvac.setModes(HAHVAC::HeatMode);

    mqtt.begin(mqtt_server, mqtt_user, mqtt_pwd);

}

/**************************************************************************/
/* Main loop                                                              */
/**************************************************************************/

void loop() {

	Balboa.loop();
  mqtt.loop();
  server.handleClient();
  
  if (WiFi.status() != WL_CONNECTED){ setup_wifi(); }             // Check WiFi connnection reconnect otherwise 
 
    if(millis() - ReportTimerPrevMillis  > ReportTimerMillis) {
    
          ReportTimerPrevMillis = millis();
          
          display.setValue(Balboa.LCD_display.c_str());
          waterTemp.setValue(Balboa.waterTemperature);
          heater.setState(Balboa.displayHeater);
          pump1.setState(Balboa.displayPump1);
          lights.setState(Balboa.displayLight);
          setTemp.setValue(Balboa.setTemperature);
          hvac.setCurrentTemperature(Balboa.waterTemperature);
          hvac.setTargetTemperature(Balboa.setTemperature);
          hvac.onTargetTemperatureCommand(onTargetTemperatureCommand);
    } 
     
}

/**************************************************************************/
/* Subscribe to MQTT topic                                                */
/**************************************************************************/

void onButtonPress(HAButton* sender) {
  
    // Handling incoming messages

    Serial.println(sender->getName());

    String s_payload = sender->getName();
         
             if (s_payload == "Temperature Up") {
                  Balboa.writeDisplayData = true; 
                  Balboa.writeTempUp      = true;
             }
             else if (s_payload == "Temperature Down") {
                  Balboa.writeDisplayData = true;
                  Balboa.writeTempDown    = true;  
             }
             else if (s_payload == "Lights") {
                  Balboa.writeDisplayData = true;
                  Balboa.writeLight       = true; 
             }
             else if (s_payload == "Pump") {
                  Balboa.writeDisplayData = true;
                  Balboa.writePump1       = true;  
             }
             else if (s_payload == "Mode") {
                  Balboa.writeDisplayData = true;
                  Balboa.writeMode        = true;  
             }
             else if (s_payload == "Stop Command") {
                  Balboa.stop();
             }
             else if (s_payload == "Reset ESP") {
                  ESP.restart();
             }
      
}
      
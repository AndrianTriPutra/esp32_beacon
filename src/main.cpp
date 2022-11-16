//mosquitto_sub -h broker.emqx.io -p 1883 -t smarthome/beacon/# -v
//mosquitto_pub -h broker.emqx.io -p 1883 -t smarthome/beacon/483923A4AE30/setting -m 'reboot'
//mosquitto_pub -h broker.emqx.io -p 1883 -t smarthome/beacon/483923A4AE30/setting -m '[interval|4]'
//mosquitto_pub -h broker.emqx.io -p 1883 -t smarthome/beacon/483923A4AE30/setting -m 'status'

#include <Arduino.h>
#include <Int64String.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <BLEDevice.h>
#include <BLEBeacon.h>
#include "time.h"
#include <EEPROM.h>

String ID=int64String((uint64_t)ESP.getEfuseMac(), HEX);
String Firmware="ATP-09.11.22";

#define WATCHDOG_TIMEOUT_S 60
hw_timer_t* watchDogTimer = NULL;

WiFiClient wifiClient;
PubSubClient mqttclient(wifiClient);
String topicPub = "smarthome/beacon/"+ID+"/data";
String topicSub = "smarthome/beacon/"+ID+"/setting";
String topicState = "smarthome/beacon/"+ID+"/status";

BLEScan *pBLEScan;
#define MAX_STRING_LEN 100
char april[10][MAX_STRING_LEN];
char jinou[10][MAX_STRING_LEN];

uint8_t counter=0;
uint8_t interval;
uint32_t uptime;

void IRAM_ATTR watchDogInterrupt() {
  Serial.println("[ERROR] Interrupt reboot");
  ESP.restart();
}

void watchDogRefresh(){
  timerWrite(watchDogTimer, 0);                    //reset timer (feed watchdog)
}

String getTime(){
  struct tm timeinfo;
  String timestamp;
  if(!getLocalTime(&timeinfo)){
    Serial.println("[ERROR] getTime");
  }else{
    char datetime[30];
    strftime(datetime, 30, "%FT%T+07:00", &timeinfo);
    timestamp = String(datetime);
  }
  return timestamp;
}

void wifi(){
  WiFi.persistent(false);
  delay(1000);
  WiFi.disconnect();
  delay(1000);
  WiFi.mode(WIFI_OFF);
  delay(3000);
  WiFi.mode(WIFI_STA);  
  delay(3000);
  
  const char* ssid = "xxxxxxxxx"; 
  const char* password = "yyyyyyyy";
  uint8_t trying=0;
  bool found=false;
  while (!found){
    trying++;
    Serial.print("Scan WIFI start ... ");
    int n = WiFi.scanNetworks();
    if (n>0){
      Serial.print(n);
      Serial.println(" network(s) found");
      for (int i = 0; i < n; i++){
        if (WiFi.SSID(i)==ssid){      
          Serial.print(WiFi.SSID(i));
          Serial.print(" found !!!");
          Serial.print("\t");
          Serial.print(WiFi.RSSI(i));
          found=true;
        }
      }
    }else{
      Serial.println("[ERROR] No network found !!!");
    }   
    if (trying>=5){
      Serial.println("[ERROR] reboot cause wifi can't found");
      ESP.restart();
    }
    Serial.println();
  }
  //WiFi.scanDelete();  
  delay(1000);
 
  WiFi.begin(ssid, password);
  
  Serial.println();
  Serial.println("WIFI SETTING");
  WiFi.printDiag(Serial);
  Serial.println();
  

  Serial.println("Connecting to Wifi ");
  trying=0;
  while (WiFi.status() != WL_CONNECTED) { 
    trying++; 
    Serial.print(trying);
    Serial.print(" ");
    delay(1000);
    
    int wifi_state=WiFi.status();
    switch(wifi_state){
      case 0:
        Serial.print("WL_IDLE_STATUS");
        break;
      case 1:
        Serial.print("WL_NO_SSID_AVAIL");
        break;
      case 2:
        Serial.print("WL_SCAN_COMPLETED");
        break;
      case 3:
        Serial.print("WL_CONNECTED");
        break;
      case 4:
        Serial.print("WL_CONNECT_FAILED");
        break;
      case 5:
        Serial.print("WL_CONNECTION_LOST");
        break;
      case 6:
        Serial.print("WL_CONNECT_WRONG_PASSWORD");
        break;
      case 7:
        Serial.print("WL_DISCONNECTED");
        break;
    } 
    Serial.println();
        
    if (trying%10==0){
       Serial.println();
    }
    if (trying>=60){
      Serial.println("[ERROR] reboot cause wifi can't connect");
      ESP.restart();
    }
  }
  
  Serial.println("\n[INFO] Wifi Connected");  
  Serial.print("[INFO] IP:");
  Serial.println(WiFi.localIP().toString());
}

void callback(char *topic, byte *payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg+=String(char(payload[i]));
  }
  
  if (msg.indexOf("reboot")!=-1){
    Serial.println("[INFO] reboot");
    ESP.restart();
  }else if(msg.indexOf("status")!=-1){
    String timestamp = getTime();
    if(timestamp.length()<10){
      timestamp="ERROR";
    }
    String mqint = String(interval*30)+" second";
    uint32_t minute = uint32_t((uptime*30)/60);
    uint32_t fh = ESP.getFreeHeap();
    char cState[250];
    snprintf(cState, 250, "{\"id\":\"%s\",\"timestamp\":\"%s\",\"interval\":\"%s\",\"uptime\":%d, \"freeheap\":%d}", 
      ID.c_str(), timestamp.c_str(), mqint, minute, fh);
    mqttclient.publish(topicState.c_str(), cState);
  }else if(msg.indexOf("interval")!=-1){//[interval|4]
    String minter = msg.substring(msg.lastIndexOf("|")+1,msg.lastIndexOf("]"));
    EEPROM.write(0, minter[0]);
    EEPROM.commit();
    String sintval = String(char(EEPROM.read(0)));
    uint8_t uintval = uint8_t(sintval.toInt());
    interval=uintval;
  }
}

void mqtt() {
  const char* mqttServer = "broker.emqx.io";
  const uint16_t mqttPort = 1883;
  const char* mqttUser = "";
  const char* mqttPassword = "";

  Serial.println();
  mqttclient.setServer(mqttServer, mqttPort);
  mqttclient.setCallback(callback);
  uint8_t trying=0;
  while (!mqttclient.connected()) {
    Serial.println("[INFO] Attempting MQTT Connection...");
  
    String clientId =  ID+"/";
    clientId += String(random(0xffff), HEX)+"/"+String(random(0xffff), HEX);
    //clientId += String(timeClient.getEpochTime());
    if (mqttclient.connect(clientId.c_str(), mqttUser, mqttPassword)) {
      Serial.println("[INFO] MQTT Connected");
      mqttclient.setKeepAlive(15);       
      mqttclient.subscribe(topicSub.c_str());
    }else {
      Serial.print("[ERROR] Failed, RC=");
      Serial.println(mqttclient.state());
      switch(mqttclient.state()){
        case -4:
          Serial.println("MQTT_CONNECTION_TIMEOUT");
          break;
        case -3:
          Serial.println("MQTT_CONNECTION_LOST");
          break;
        case -2:
          Serial.println("MQTT_CONNECT_FAILED");
          break;
        case -1:
          Serial.println("MQTT_DISCONNECTED");
          break;
        case 0:
          Serial.println("MQTT_CONNECTED");
          break;
        case 1:
          Serial.println("MQTT_CONNECT_BAD_PROTOCOL");
          break;
        case 2:
          Serial.println("MQTT_CONNECT_BAD_CLIENT_ID");
          break;
        case 3:
          Serial.println("MQTT_CONNECT_UNAVAILABLE");
          break;
        case 4:
          Serial.println("MQTT_CONNECT_BAD_CREDENTIALS");
          break;
        case 5:
          Serial.println("MQTT_CONNECT_UNAUTHORIZED");
          break;
      }
      mqttclient.disconnect();
      delay(5000);
      mqttclient.connect(clientId.c_str(), mqttUser, mqttPassword);

      trying++;
      Serial.print(" ");
      Serial.print(trying);
      Serial.print(" ");
      if (trying>=5){
        Serial.println("[ERROR] reboot cause can't connect mqtt");
        ESP.restart();
      }
    }
  }
  Serial.println();
}

void taskBLE(void *pvParameters){
  for(;;){
    watchDogRefresh();
    // Serial.print("[INFO]");
    // Serial.print(getTime());
    // Serial.print(" ");
    // Serial.println("taskBLE");
    BLEScanResults foundDevices = pBLEScan->start(30);
    pBLEScan->clearResults(); // delete results fromBLEScan buffer to release memory
  }
}

void taskMQTT(void *pvParameters){
  for(;;){
    watchDogRefresh();
    String timestamp = getTime();
    // Serial.print("[INFO]");
    // Serial.print(timestamp);
    // Serial.print(" ");
    // Serial.println("taskMQTT");
    if(!mqttclient.connected()){
      mqtt();
    }
    counter++;
    uptime++;
    if (counter>=interval && timestamp.length()>0){
      uint32_t fh = ESP.getFreeHeap();
      if (fh<50000){
        ESP.restart();
      }

      char cSendValue[250];
      for(uint8_t i = 0; i <= 30; ++i){
        if (strlen(april[i])>30){
          snprintf(cSendValue, 250, "{\"id\":\"%s\",\"timestamp\":\"%s\", \"beacon\":%s}", 
            ID.c_str(), timestamp.c_str(), april[i]);
          //Serial.printf("[INFO] Sending april:%s\n",cSendValue);
          mqttclient.publish(topicPub.c_str(), cSendValue);
        }
        
        if (strlen(jinou[i])>30){
          snprintf(cSendValue, 250, "{\"id\":\"%s\",\"timestamp\":\"%s\", \"beacon\":%s}", 
            ID.c_str(), timestamp.c_str(), jinou[i]);
          //Serial.printf("[INFO] Sending jinou:%s\n",cSendValue);
          mqttclient.publish(topicPub.c_str(), cSendValue);
        }
      }
      
      memset(april, 0, sizeof(april));
      memset(jinou, 0, sizeof(jinou));
      counter=0;
    }
    vTaskDelay(30000);
  }
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks{
    void onResult(BLEAdvertisedDevice advertisedDevice){

      if (advertisedDevice.haveManufacturerData() == true){  
        std::string strManufacturerData  = advertisedDevice.getManufacturerData();
        uint8_t cManufacturerData[MAX_STRING_LEN];
        strManufacturerData.copy((char *)cManufacturerData, strManufacturerData.length(), 0);
        if (strManufacturerData.length() == 25 && cManufacturerData[0] == 0x4C && cManufacturerData[1] == 0x00){//april
          if (advertisedDevice.haveName()){
            int8_t suhu = cManufacturerData[23];
            suhu = cManufacturerData[23];
            if ((suhu & 0x80) != 0){
              suhu=(byte)(~(suhu - 0x01)) * -1;
            }
            char cSuhu[5];
            float fSuhu;
            fSuhu = suhu*1.0;
            dtostrf(fSuhu, 4, 1, cSuhu);  

            if(cManufacturerData[22] <= 100){
              char mergedata[1][MAX_STRING_LEN];
              String serial_number ;
              serial_number = advertisedDevice.getAddress().toString().c_str();
              serial_number.replace(":","");
              snprintf(mergedata[0], MAX_STRING_LEN, "{\"id\":\"%s\",\"temp\":%s,\"battery\":%d,\"rssi\":%d}",
                serial_number.c_str(), cSuhu, cManufacturerData[22], advertisedDevice.getRSSI());
              
              bool filter=true;
              for (uint8_t j=0;j<30;j++){
                if(strstr(april[j],serial_number.c_str())){
                  if (filter){
                      snprintf(april[j], MAX_STRING_LEN,"%s",mergedata[0]);
                      filter=false;
                  }
                }else{
                  if(strlen(april[j])==0){
                    if (filter){
                      snprintf(april[j], MAX_STRING_LEN,"%s",mergedata[0]);
                      filter=false;
                    }
                  }
                }
              }
            }
          }
        }else if (strManufacturerData.length() ==13 ) {//jinou
          if (advertisedDevice.haveName()){
            String serial_number = advertisedDevice.getAddress().toString().c_str();    
            String tanda="";
            if (cManufacturerData[0]!=0){
              tanda="-";
            }
            
            if(cManufacturerData[6] <= 100){
              char mergedata[1][MAX_STRING_LEN];
              snprintf(mergedata[0], MAX_STRING_LEN, "{\"id\":\"%s\",\"temp\":%s%d.%d,\"battery\":%d,\"rssi\":%d}",
                serial_number.c_str(), tanda,cManufacturerData[1],cManufacturerData[2], 
                cManufacturerData[6], advertisedDevice.getRSSI()); 
          
              bool filter=true;
              for (uint8_t j=0;j<30;j++){
                if(strstr(jinou[j],serial_number.c_str())){
                  if (filter){
                      snprintf(jinou[j], MAX_STRING_LEN,"%s",mergedata[0]);
                      filter=false;
                  }
                }else{
                  if(strlen(jinou[j])==0){
                    if (filter){
                      snprintf(jinou[j], MAX_STRING_LEN,"%s",mergedata[0]);
                      filter=false;
                    }
                  }
                }
              };
            }
          }            
        }
      }
      return;
    }
};

void setup() {
  Serial.begin(115200); 
  EEPROM.begin(512);
  delay(1000);

  String sintval = String(char(EEPROM.read(0)));
  uint8_t uintval = uint8_t(sintval.toInt());
  if(uintval==0){
    interval=2;
  }else{
    interval=uintval;
  }

  Serial.println();
  Serial.println("================== START ==================");
  Serial.print("Device ID:");
  Serial.println(ID);
  Serial.print("Firmware :");
  Serial.println(Firmware);
  Serial.print("Interval :");
  Serial.print(interval*30);
  Serial.println(" second");
  Serial.println();

  Serial.println("[INFO] Watchdog start"); 
  watchDogTimer = timerBegin(2, 80, true);
  timerAttachInterrupt(watchDogTimer, &watchDogInterrupt, true);
  timerAlarmWrite(watchDogTimer, WATCHDOG_TIMEOUT_S * 1000000, false);
  timerAlarmEnable(watchDogTimer);

  wifi();
  mqtt();

  const char* ntpServer = "0.id.pool.ntp.org";
  const long gmtOffset_sec = 25200;//utc+7 //3600*7
  const int daylightOffset_sec = 0;
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(3000);
  Serial.print("[START]");
  Serial.println(getTime());

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster 

  Serial.println();
  Serial.println("ESP INFORMATION:");
  Serial.printf("[INFO] Internal Total heap %d, internal Free Heap %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
  Serial.printf("[INFO] SPIRam Total heap %d, SPIRam Free Heap %d\n", ESP.getPsramSize(), ESP.getFreePsram()); 
  Serial.printf("[INFO] ChipRevision %d, Cpu Freq %d, SDK Version %s\n",ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getSdkVersion());
  Serial.printf("[INFO] Flash Size %d, Flash Speed %d\n",ESP.getFlashChipSize(), ESP.getFlashChipSpeed());

  Serial.println("===========================================");

  xTaskCreatePinnedToCore(taskBLE, "Task_BLE", 4096, NULL, 1, NULL, ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(taskMQTT, "Task_MQTT", 2048, NULL, 2, NULL, ARDUINO_RUNNING_CORE);
}

void loop() {
  mqttclient.loop();
}
/* MIT License

Copyright (c) 2023 darrylb123

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
// Wifi Functions choose between Station or SoftAP

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include "Config.h"
#include "SMA_Inverter.h"

#define MYFS LittleFS
#define FORMAT_LITTLEFS_IF_FAILED 


extern Config config;


int smartConfig = 1; 

IPAddress         apIP(8,8,4,4);

WebServer webServer(80);

void wifiStartup(){

  // Build Hostname
  char sapString[21];
 

  snprintf(sapString, 20, "SMA-%08X", ESP.getEfuseMac());

  // Attempt to connect to the AP stored on board, if not, start in SoftAP mode
  WiFi.mode(WIFI_STA);
  delay(2000);
  WiFi.hostname(sapString);
  WiFi.begin();
  delay(5000);

  if (config.mqttTopic == "") 
    config.mqttTopic = sapString;
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    // Launch smartconfig to reconfigure wifi
    Serial.print(">");
    if (i > 3)
      mySmartConfig();
    i++;
    delay(1000);
  }
  // Success connecting 
  smartConfig = 0; 
  String hostName = "Hostname: ";
  hostName = hostName + sapString;
  Serial.println(hostName);
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
   
  
  webServer.begin();
  webServer.on("/", formPage);
  webServer.on("/smartconfig", connectAP);
  webServer.on("/postform/",handleForm);

  Serial.print("Web Server Running: ");

}

// Configure wifi using ESP Smartconfig app on phone
void mySmartConfig() {
  // Wipe current credentials
  WiFi.disconnect(true); // deletes the wifi credentials
  
  WiFi.mode(WIFI_STA);
  delay(2000);
  WiFi.begin();
  WiFi.beginSmartConfig();

  //Wait for SmartConfig packet from mobile
  Serial.println("Waiting for SmartConfig.");
  while (!WiFi.smartConfigDone()) {
    delay(500);
    Serial.print(".");
  }



  Serial.println("");
  Serial.println("SmartConfig received.");

  //Wait for WiFi to connect to AP
  Serial.println("Waiting for WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  Serial.println("Restarting in 5 seconds");
  delay(5000);
  ESP.restart();
}

// Use ESP SmartConfig to connect to wifi
void connectAP(){
  webServer.send(200, "text/plain", "Open ESPTouch: Smartconfig App to connect to Wifi Network"); 
  delay(2000);
  mySmartConfig();
  
}


void wifiLoop(){
  webServer.handleClient();  
}

void formPage () {
  char tempstr[1024];
  char *responseHTML;
  extern InverterData *pInvData;
  extern DisplayData *pDispData;

  responseHTML = (char *)malloc(10000);
  DEBUG1_PRINT("Connect formpage\n");
  strcpy(responseHTML, "<!DOCTYPE html><html><head>\
                      <title>SMA Inverter</title></head><body>\
                      <style>\
                      table {\
  border-collapse: collapse;\
  width: 100%;\
  font-size: 30px;\
}\
\
table, th, td {\
  border: 1px solid black;\
}\
</style>\
<H1> SMA Bluetooth Configuration</H1>\
<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/postform/\">");


  strcat(responseHTML, "<TABLE><TR><TH>Configuration</TH><TH>Setting</TH></TR>\n");
  sprintf(tempstr, "<TR><TD>Inverter Bluetooth Address (Format AA:BB:CC:DD:EE:FF) : </TD><TD> <input type=\"text\" name=\"btaddress\" value=\"%s\"></TD><TR>\n\n",config.SmaBTAddress.c_str());
  strcat(responseHTML, tempstr);
  sprintf(tempstr, "<TR><TD>MQTT Inverter Password :</TD><TD> <input type=\"text\" name=\"smapw\" value=\"%s\"></TD><TR>\n\n",config.SmaInvPass.c_str());
  strcat(responseHTML, tempstr);
  sprintf(tempstr, "<TR><TD>MQTT Broker Hostname or IP Address :</TD><TD> <input type=\"text\" name=\"mqttBroker\" value=\"%s\"></TD><TR>\n\n",config.mqttBroker.c_str());
  strcat(responseHTML, tempstr);
  sprintf(tempstr, "<TR><TD>MQTT Broker Port : </TD><TD><input type=\"text\" name=\"mqttPort\" value=\"%d\"></TD><TR>\n\n",config.mqttPort);
  strcat(responseHTML, tempstr);
  sprintf(tempstr, "<TR><TD>MQTT Broker User :</TD><TD> <input type=\"text\" name=\"mqttUser\" value=\"%s\"></TD><TR>\n\n",config.mqttUser.c_str());
  strcat(responseHTML, tempstr);
  sprintf(tempstr, "<TR><TD>MQTT Broker Password :</TD><TD> <input type=\"text\" name=\"mqttPasswd\" value=\"%s\"></TD><TR>\n\n",config.mqttPasswd.c_str());
  strcat(responseHTML, tempstr);
  sprintf(tempstr, "<TR><TD>MQTT Topic :</TD><TD> <input type=\"text\" name=\"mqttTopic\" value=\"%s\"></TD><TR>\n\n",config.mqttTopic.c_str());
  strcat(responseHTML, tempstr);


  strcat(responseHTML, "</TABLE>");
  strcat(responseHTML, "<input type=\"submit\" value=\"Submit\"></form><BR> <A href=\"/smartconfig\">Enable ESP Touch App smart config</A><BR>");


  strcat(responseHTML, "<TABLE><TR><TH>Last Scan</TH><TH>Data</TH>\n");
  snprintf(tempstr, sizeof(tempstr),
"<tr><td>Serial Number</td><td>%d %</td></tr>\n\
 <tr><td>BT Signal Strength</td><td>%4.1f %</td></tr>\n\
  <tr><td>Uac</td><td>%1.1f V</td></tr>\n\
 <tr><td>Iac</td><td>%1.3f A</td></tr>\n\
 <tr><td>Pac</td><td>%1.3f kW</td></tr>\n\
 <tr><td>Udc</td><td>String 1: %15.2f V, String 2: %15.2f V</td></tr>\n\
 <tr><td>Idc</td><td>String 1: %15.2f A, String 2: %15.2f A</td></tr>\n\
 <tr><td>Wdc</td><td>String 1: %15.2f kW, String 2: %15.2f kW</td></tr>\n"
 , pInvData->Serial
 , pDispData->BTSigStrength
 , pDispData->Uac
 , pDispData->Iac
 , pDispData->Pac
 , pDispData->Udc[0], pInvData->Udc[1]
 , pDispData->Idc[0], pInvData->Idc[1]
 , pDispData->Udc[0] * pDispData->Idc[0] , pDispData->Udc[1] * pDispData->Idc[1]);

  strcat(responseHTML, tempstr);

  snprintf(tempstr, sizeof(tempstr),
"<tr><td>E-Today</td><td>%1.3f kWh</td></tr>\n\
 <tr><td>E-Total</td><td>%1.3f kWh</td></tr>\n"
 , tokWh(pInvData->EToday)
 , tokWh(pInvData->ETotal));
  strcat(responseHTML, tempstr);
  // if (DEBUG)
  //  Serial.print(responseHTML);
  strcat(responseHTML,"</TABLE></body></html>\n");
  delay(100);// Serial.print(responseHTML);
  webServer.send(200, "text/html", responseHTML);

}

// Function to extract the configuration
void handleForm() {
  DEBUG1_PRINT("Connect handleForm\n");
  if (webServer.method() != HTTP_POST) {
    webServer.send(405, "text/plain", "Method Not Allowed");
  } else {
    String message = "POST form was:\n";
    for (uint8_t i = 0; i < webServer.args(); i++) {
      String name = webServer.argName(i);
      if (name == "mqttBroker") {
        config.mqttBroker = webServer.arg(i);
        config.mqttBroker.trim();
      } else if (name == "mqttPort") {
        String val = webServer.arg(i);
        config.mqttPort = val.toInt();   
      } else if (name == "mqttUser") {
        config.mqttUser = webServer.arg(i);
        config.mqttUser.trim();
      } else if (name == "mqttPasswd") {
        config.mqttPasswd = webServer.arg(i);
        config.mqttPasswd.trim();
      } else if (name == "mqttTopic") {
        config.mqttTopic = webServer.arg(i);
        config.mqttTopic.trim();
      } else if (name == "btaddress") {
        config.SmaBTAddress = webServer.arg(i);
        config.SmaBTAddress.trim();
      } else if (name == "smapw") {
        config.SmaInvPass = webServer.arg(i);
        config.SmaInvPass.trim();
      } 

    }
    saveConfiguration(confFile,config);
    printFile(confFile);
    delay(5000);
    ESP.restart();
  }
}
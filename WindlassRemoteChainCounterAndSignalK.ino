/*
  This code is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This code is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

// Anchor Chain Remote Control / Chain Counter with WLAN.
// original version by AK-Homberger
// SignalK feed, external control, freefall, LED status added by LukeS and JeremyP
// Version 1.0  20/12/2020

#include<ESP8266WiFi.h>
#include<ESP8266WebServer.h>
#include<EEPROM.h>
#include <WiFiUdp.h>


#include "index_html.h"              // Webserver script for Gauge/Buttons on phone/tablet

#define ENABLE_DEMO 0                // Set to 1 to enable Demo Mode with up/down counter
#define SAFETY_STOP 2                // Defines safety stop for chain up. Stops defined number of revolutions before reaching zero
#define MAX_CHAIN_LENGTH 40          // Define maximum chain length. Relay turns off after the value is reached

// Wifi: Select AP or Client

#define WiFiMode_AP_STA 1               // Defines WiFi Mode 0 -> AP (with IP:192.168.4.1 stand alone and  1 -> Station (client with IP: via DHCP)
                                        // To use both the windlass control and feed SignalK you have to set to 1 as a Station 
const char *ssid = "Your SSID";          // Set WLAN name for station connection
const char *password = "Your Password";  // Set password for station connection
IPAddress ip1(10, 10, 10, 1);           // IP Address to be used for SiganlK so that udp knows where to send messages
unsigned int outPort = 4210;            // and which port to send to


ESP8266WebServer server(80);                // Web Server at port 80 for general connection to central boat wifi


// Chain Counter
 
#define Chain_Calibration_Value 0.33 // Translates counter pulses to metres per revolution - change this to suit your windlass
#define Chain_Counter_Pin D1         // Counter impulse is measured as interrupt on pin D1, GPIO pin 05
unsigned long Last_int_time = 0;     // Time of last interrupt
unsigned long Last_event_time = 0;   // Time of last event for engine watchdog
int ChainCounter = 0;                // Counter for chain events
int LastSavedCounter = 0;            // Stores last ChainCounter value to allow storage to nonvolatile storage in case of value changes

// Relay

#define Chain_Up_Pin D7              // GPIO pin 13 for Chain Up Relay       (If using a board other than Wemos D1 mini then check all pins)
#define Chain_Down_Pin D5            // GPIO pin 14 for Chain Down Relay      
#define Chain_Up_Override_Pin D2     // GPIO pin 04 for Chain Up Override     
#define Chain_Down_Override_Pin D6   // GPIO pin 12 for Chain Down Override   
int UpDown = 1;                      // 1 =  Chain down / count up, -1 = Chain up / count backwards
int OnOff = 0;                       // Relay On/Off - Off = 0, On = 1
unsigned long Watchdog_Timer = 0;    // Watchdog timer to stop relay after 1 second of inactivity e.g. connection loss to client


// Chain Event Interrupt
// Enters on falling edge

void IRAM_ATTR handleInterrupt() {    //This section debounces the chain sensor to avoid multipe counts from one switch event
  noInterrupts();

  if (millis() > Last_int_time + 10) {  // Debouncing. No new events for 10 milliseconds

    ChainCounter += UpDown;             // Chain event: - add or subtract from previous count according to sign of UpDown

    if ( ( (ChainCounter <= SAFETY_STOP) && (UpDown == -1) && (OnOff == 1) ) ||     // Safety stop counter reached while chain is going up
         ( (UpDown == 1) && (abs(ChainCounter) * Chain_Calibration_Value >= MAX_CHAIN_LENGTH) ) ) {  // Maximum chain length reached

      digitalWrite(Chain_Up_Pin, LOW );
      digitalWrite(Chain_Down_Pin, LOW );
      OnOff = 0;
    }
    Last_event_time = millis();         // Store last event time to detect blocking chain
  }
  Last_int_time = millis();             // Store last interrupt time for debouncing
  interrupts();
}

 WiFiUDP Udp;                         // Configure UDP - not sure if these lines are necessary as we are transmitting rather than receiving via UDP
unsigned int localUdpPort = 4210;  // local port to listen on
char incomingPacket[255];  // buffer for incoming packets
char  replyPacket[] = "Hi there! Got the message :-)";  // a reply string to send back


void setup() {
  
  // LED indicator
  pinMode(LED_BUILTIN,OUTPUT); //Define LED pin as output
  digitalWrite(LED_BUILTIN,HIGH); //and turn it off
  
  int wifi_retry = 0;

  // Relay output
  pinMode(Chain_Up_Pin, OUTPUT);            // Sets pin as output
  pinMode(Chain_Down_Pin, OUTPUT);          // Sets pin as output
  digitalWrite(Chain_Up_Pin, LOW );         // and relay off
  digitalWrite(Chain_Down_Pin, LOW );       // and relay off
  
  //Over Ride input
  pinMode(Chain_Down_Override_Pin, INPUT_PULLUP);  // Sets pin as input pulled high 
  pinMode(Chain_Up_Override_Pin, INPUT_PULLUP);    // Sets pin as input pulled high

  // Init Chain Count measure with interrupt
  pinMode(Chain_Counter_Pin, INPUT_PULLUP); // Sets pin as input pulled high
  attachInterrupt(digitalPinToInterrupt(Chain_Counter_Pin), handleInterrupt, FALLING); // Attaches pin to interrupt on falling edge

  // Init serial
  Serial.begin(115200);
  Serial.print("");
  Serial.println("Start");

  EEPROM.begin(16);
  ChainCounter = EEPROM.read(0);
  LastSavedCounter = ChainCounter;                  // Initialise last counter value so that after power off it restarts with the same length

  // Init WLAN AP
  if (WiFiMode_AP_STA == 0) {

    WiFi.mode(WIFI_AP);                              // WiFi Mode Access Point
    delay (100);
    WiFi.softAP(ssid, password); // AP name and password
    Serial.println("Start WLAN AP");
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());
    
  Udp.begin(localUdpPort);                          // Not sure if this is needed as again it is listening rather than sending
  Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), localUdpPort);

  } else {

    Serial.println("Start WLAN Client DHCP");         // WiFi Mode Client with DHCP
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {           // Check connection
      wifi_retry++;
      delay(500);
      Serial.print(".");
      if (wifi_retry > 10) {
        Serial.println("\nReboot");                   // Reboot after 10 connection tries
        ESP.restart();
      }
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Udp.begin(localUdpPort);
  Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), localUdpPort);
  }

 


  // Handle HTTP request events from phone/tablet

  server.on("/", Event_Index);
  server.on("/gauge.min.js", Event_js);
  server.on("/ADC.txt", Event_ChainCount);
  server.on("/up", Event_Up);
  server.on("/down", Event_Down);
  server.on("/stop", Event_Stop);
  server.on("/reset", Event_Reset);

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP Server started");
}


void Event_Up() {                          // Handle UP request
//  if ((OnOff == 0 && digitalRead(Chain_Up_Override_Pin) == HIGH && digitalRead(Chain_Down_Override_Pin) == HIGH) || (OnOff ==1)) {// Execute only if the app is already on, or if there isn't a manual override present
  server.send(200, "text/plain", "-1000"); // Send response "-1000" means no  chainlength
  Serial.println("Up");
  digitalWrite(Chain_Up_Pin, HIGH );
  digitalWrite(Chain_Down_Pin, LOW );
  Last_event_time = millis();
  UpDown = -1;
  OnOff = 1;
 }
}

void Event_Down() {                         // Handle Down request
//  if ((OnOff == 0 && Chain_Up_Override_Pin == HIGH && Chain_Down_Override_Pin == HIGH) || (OnOff ==1)) {// Execute only if the app is already on, or if there isn't a manual override present
// JCHP commented this out to get relays working
  server.send(200, "text/plain", "-1000");  // Send response "-1000" means no  chainlength
  Serial.println("Down");
  digitalWrite(Chain_Up_Pin, LOW );
  digitalWrite(Chain_Down_Pin, HIGH );
  Last_event_time = millis();
  UpDown = 1;
  OnOff = 1;
//}}
}

void Event_Stop() {                         // Handle Stop request
//  if ((OnOff == 0 && Chain_Up_Override_Pin == HIGH && Chain_Down_Override_Pin == HIGH) || (OnOff ==1)) {// Execute only if the app is already on, or if there isn't a manual override present
// JCHP commented this out to get relays working
  server.send(200, "text/plain", "-1000");  // Send response "-1000" means no  chainlength
  Serial.println("Stop");
  digitalWrite(Chain_Up_Pin, LOW );
  digitalWrite(Chain_Down_Pin, LOW );
  OnOff = 0;
//}}
}

void Event_Reset() {                        // Handle reset request to reset counter to 0
  ChainCounter = 0;
  server.send(200, "text/plain", "-1000");  // Send response "-1000" means no  chainlenght
  Serial.println("Reset");
}


void Event_Index() {                         // If "http://<ip address>/" requested
  server.send(200, "text/html", indexHTML);  // Send Index Website
}


void Event_js() {                            // If "http://<ip address>/gauge.min.js" requested
  server.send(200, "text/html", gauge);      // Then send gauge.min.js
}


void Event_ChainCount() {                    // If  "http://<ip address>/ADC.txt" requested
  
  float temp = (ChainCounter * Chain_Calibration_Value); // Chain in meters
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "text/plain", String (temp));

  Watchdog_Timer = millis();                 // Watchdog timer is set to current uptime

#if ENABLE_DEMO == 1                         // Demo Mode - Counts automatically UP/Down every 500 ms

  if (OnOff == 1) ChainCounter += UpDown;

  if ( ( (ChainCounter <= SAFETY_STOP) && (UpDown == -1) && (OnOff == 1) ) ||     // Safety stop counter reached while chain is going up
       ( (UpDown == 1) && (abs(ChainCounter) * Chain_Calibration_Value >= MAX_CHAIN_LENGTH) ) ) {  // Maximum chain lenght reached

    digitalWrite(Chain_Up_Pin, LOW );
    digitalWrite(Chain_Down_Pin, LOW );
    OnOff = 0;
  }
  Last_event_time = millis();
#endif

}


void handleNotFound() {                                           // Unknown request. Send error 404
  server.send(404, "text/plain", "File Not Found\n\n");
}


void loop() {
  int wifi_retry = 0;

  server.handleClient();                                           // Handle HTTP requests

  if ( ( millis() > Watchdog_Timer + 1000 ) ||                     // Check HTTP connnection
       ( (OnOff == 1) && (millis() > Last_event_time + 1000)) )  { // Check events if engine is on

      digitalWrite(Chain_Up_Pin, LOW );                              // Relay off after 1 second inactivity  COMMENT OUT THESE THREE LINES TO BENCH TEST
      digitalWrite(Chain_Down_Pin, LOW );                            
      OnOff = 0;                                                     
  }
  
  if ((OnOff == 1 && (digitalRead(Chain_Up_Override_Pin) == LOW))||(OnOff == 1&& (digitalRead(Chain_Down_Override_Pin)) == LOW)){  //If the app is on and one of the relays is operated
    digitalWrite(BUILTIN_LED,LOW);                                                      // then turn on the LED
  } else {                                                                              // otherwise
    digitalWrite(BUILTIN_LED,HIGH);                                                     // turn it off 
  }
  
  if (OnOff == 0) {                                     // If App is off  ... 
  if (digitalRead(Chain_Up_Override_Pin) == LOW) {      // and there is a manual chain up over-ride 
  UpDown = -1;                                          // set counter to reduce chain count
  } else {
  UpDown = 1;                                           // otherwise if App is off then set to increase counter ie free fall or powered down..
  }}
    

  if (ChainCounter != LastSavedCounter) {                          // If ChainCounter has changed then store new value to nonvolatile storage (if changed)
    EEPROM.write(0, ChainCounter);
    EEPROM.commit();
    LastSavedCounter = ChainCounter;

    
  float temp = (ChainCounter * Chain_Calibration_Value);            // Caluclate new length to send to SignalK

  char udpmessage[1024];      // This and next few lines send the new length to SignalK.  You can change the path or source if you wish.
  sprintf(udpmessage, "{\"updates\":[{\"$source\":\"ESP8266.Windlass.Control\",\"values\":[{\"path\":\"navigation.anchor.chainlength\",\"value\":%f}]}]}", temp);
  Udp.beginPacket(ip1, outPort);
  Udp.write(udpmessage);
  Udp.endPacket();

  Serial.println(udpmessage); // print udpmessage to serial monitor in Arduino IDE, so can check JSON structure of the message is correct
    
  }

  if (WiFiMode_AP_STA == 1) {                                      // Check connection if working as client
    while (WiFi.status() != WL_CONNECTED && wifi_retry < 5 ) {     // Connection lost, 5 tries to reconnect
      wifi_retry++;
      Serial.println("WiFi not connected. Try to reconnect");
      WiFi.disconnect();
      WiFi.mode(WIFI_OFF);
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);
      delay(100);
    }
    if (wifi_retry >= 5) {
      Serial.println("\nReboot");                                  // Did not work -> restart ESP8266
      ESP.restart();
    }
    

  
  
  
  }



  // Dummy to empty input buffer to avoid board to stuck with e.g. NMEA Reader
  if ( Serial.available() ) {
    Serial.read();
  }
}

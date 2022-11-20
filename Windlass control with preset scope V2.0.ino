// Anchor Chain Remote Control / Chain Counter with WLAN.
// and Preset Target Scope
// SignalK feed, external control, freefall, LED status
// Version 2.0  07/11/2022

#include<ESP8266WiFi.h>
#include<ESP8266WebServer.h>
#include<EEPROM.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>              // Used to enable web browser to find by URL rather than IP address


#include "index_html.h"              // Webserver script for Gauge/Buttons on phone/tablet

#define ENABLE_DEMO 1                // Set to 1 to enable Demo Mode with up/down counter
#define SAFETY_STOP 2                // Defines safety stop for chain up. Stops defined number of revolutions before reaching zero
#define MAX_CHAIN_LENGTH 39          // Define maximum chain lengthin metres. Relay turns off after the value is reached

// Wifi: Select AP or Client

#define WiFiMode_AP_STA 0             // Defines WiFi Mode 0 -> AP (with IP:192.168.4.1 stand alone and  1 -> Station (client with IP: via DHCP)
                                        // To use both the windlass control and feed SignalK you have to set to 1 as a Station 
const char *ssid = "Windlass";          // Set WLAN name for station connection
const char *password = "Windlass";  // Set password for station connection
IPAddress ip1(10, 10, 10, 1);           // IP Address to be used for SiganlK so that udp knows where to send messages - amend if yours is different
unsigned int outPort = 4210;            // and which port to send to


ESP8266WebServer server(80);                //Define Web Server at port 80 for general connection to central boat wifi


// Chain Counter
 
#define Chain_Calibration_Value 0.35 // Translates counter pulses to metres per revolution - change this to suit your windlass
#define Chain_Counter_Pin D1         // Counter impulse is measured as interrupt on pin D1, GPIO pin 05
unsigned long Last_int_time = 0;     // Time of last interrupt
unsigned long Last_event_time = 0;   // Time of last event for engine watchdog
unsigned long Last_stop_time = 0;    // Time of stop request - used to continue correct counting while windlass slows to a halt
unsigned long Sprint_timer = 0;       // Timer to control serial monitor printing
int ChainCounter = 0;                // Counter for chain events
int Scope = 0;                        // Preset Target Scope
int LastSavedCounter = 0;            // Stores last ChainCounter value to allow storage to nonvolatile storage in case of value changes

// Relay

#define Chain_Up_Pin D7              // GPIO pin 13 for Chain Up Relay       (If using a board other than Wemos D1 mini then check all pins)
#define Chain_Down_Pin D5            // GPIO pin 14 for Chain Down Relay      
#define Chain_Up_Override_Pin D2     // GPIO pin 04 for Chain Up Override     
#define Chain_Down_Override_Pin D6   // GPIO pin 12 for Chain Down Override   
int UpDown = 1;                      // 1 =  Chain down / count up, -1 = Chain up / count backwards
int OnOff = 0;                       // Relay On/Off - Off = 0, On = 1
int Preset = 0;                      // Flag to control logic in reaching preset scope
unsigned long Watchdog_Timer = 0;    // Watchdog timer to stop relay after 1 second of inactivity e.g. connection loss to client


// Chain Event Interrupt
// Enters on falling edge

void IRAM_ATTR handleInterrupt() {    //This section debounces the chain sensor to avoid multipe counts from one switch event
  noInterrupts();

  if (millis() > Last_int_time + 100) {  // Debouncing. No new events for 100 milliseconds - still a fraction of a turn of the windlass

    ChainCounter += UpDown;             //Increment/decrement chain counter based on sign of UpDown

  //This section handles testing against reaching a preset value

    if ((Preset == 1) && (UpDown == 1) && (Scope-(ChainCounter*Chain_Calibration_Value) <= 0 )) {       //Test to see if chain let out to preset scope - and if so....
    OnOff = 0;                                                                                        //Set status to off
    Last_stop_time = millis();                                                                        //Record time (used to allow for deceleration interval of windlass)      
    Preset = 0;                                                                                       //Flag end of preset operation
    digitalWrite(Chain_Up_Pin, LOW );                                                                 //Make sure both relays are off  
    digitalWrite(Chain_Down_Pin, LOW);
  }

  if ((Preset == 1) && (UpDown == -1) && ((ChainCounter*Chain_Calibration_Value)-Scope <= 0 )) {      //Test to see if chain is taken in to preset scope - and if so.....
    OnOff = 0;                                                                                        //Set status to off
    Last_stop_time = millis();                                                                        //Record time (used to allow for deceleration interval of windlass)
    Preset = 0;                                                                                       //Flag end of preset operation
    digitalWrite(Chain_Up_Pin, LOW );                                                                 //Make sure both relays are off
    digitalWrite(Chain_Down_Pin, LOW);
  }

   //This section handles reaching the upper safety stop limit     

  if ( ( (ChainCounter <= SAFETY_STOP) && (UpDown == -1) && (OnOff == 1) )) {    // If safety stop counter reached while chain is going up - 
    digitalWrite(Chain_Up_Pin, LOW );                                             // Turn off both relays
    digitalWrite(Chain_Down_Pin, LOW );
    OnOff = 0;                                                                    // Set the status to off
    Last_stop_time = millis();
    Serial.print("Up safety stop reached - Turning off Up relay and UpDown state is ");
    Serial.println(UpDown);
    }
  
  
  
  if  ( (UpDown == 1) && (abs(ChainCounter) * Chain_Calibration_Value >= MAX_CHAIN_LENGTH) )  {  // Maximum chain length reached - so turn off

    digitalWrite(Chain_Up_Pin, LOW );
    digitalWrite(Chain_Down_Pin, LOW );
    OnOff = 0;
    Serial.print("Maximum chain length reached - Turning off Down relay and UpDown state is ");
    Serial.println(UpDown);

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

  // Init serial monitor for debugging
  Serial.begin(38400);
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
  
      char udpmessage[1024];    //Next few lines send metadata to SignalK for chainlength - replace mmsi with your own "self" vessel details
      sprintf(udpmessage, "{\"context\": \"vessels.urn: mrn: imo: mmsi: 235083375\",\"updates\": [{\"meta\":[{\"path\": \"navigation.anchor.chainlength\",\"value\": {\"units\": \"m\",\"description\": \"deployed chain length\",\"displayName\": \"Chain Length\",\"shortName\": \"DCL\"}}]}]}");
      Udp.beginPacket(ip1, outPort);
      Udp.write(udpmessage);
      Udp.endPacket();
      Serial.printf("Metadata sent to SignalK");
      
}

if (!MDNS.begin("windlass")) {                                                // Start the mDNS responder for windlass.local
    Serial.println("Error setting up MDNS responder!");
  }
  Serial.println("mDNS responder started");


// Handle HTTP request events from phone/tablet

server.on("/", Event_Index);
server.on("/gauge.min.js", Event_js);
server.on("/ADC.txt", Event_ChainCount);
server.on("/up", Event_Up);
server.on("/down", Event_Down);
server.on("/stop", Event_Stop);
server.on("/reset", Event_Reset);
server.on("/preset", Event_Preset);
server.onNotFound(handleNotFound);

server.begin();
Serial.println("HTTP Server started");
}

//HANDLE UP REQUEST
void Event_Up() {    
  if (((OnOff == 0 && digitalRead(Chain_Up_Override_Pin) == HIGH && digitalRead(Chain_Down_Override_Pin) == HIGH)) || (OnOff == 1)) { // Execute only if the app is already on, or if there isn't a manual override present
    server.send(200, "text/plain", "-1000");                                                                                          // Send response "-1000" means no  chainlength
     UpDown = -1;																													// Set chain counter to reduce scope
    Serial.println("Up button pressed - setting UpDown to -1 and Turning on Up relay by setting Chain_Up_Pin to High");				//Debug message
      digitalWrite(Chain_Down_Pin, LOW );                                                                                             //  Make sure down relay is off
      digitalWrite(Chain_Up_Pin, HIGH ); 																							//Then turn up relay on																						
      OnOff = 1;																													//With app on
      Preset = 0;																													//And target preset scope off
       Last_event_time = millis();                                        //Record event time for checking if windlass is jammed
  }
}
//HANDLE DOWN REQUEST
void Event_Down() {  
  if (((OnOff == 0 && digitalRead(Chain_Up_Override_Pin) == HIGH && digitalRead(Chain_Down_Override_Pin) == HIGH)) || (OnOff == 1)) { // Execute only if the app is already on, or if there isn't a manual override present
    server.send(200, "text/plain", "-1000");  // Send response "-1000" means no  chainlength
    UpDown = 1;                                                                                                                       // Set chain counter to increase scope
    Serial.println("Down button pressed - setting UpDown to 1 and Turning on Down relay by setting Chain_Down_Pin to High");          // Debug message
    digitalWrite(Chain_Up_Pin, LOW );                                                                                                 // Make sure up relay is off
    digitalWrite(Chain_Down_Pin, HIGH );                                                                                              // And turn down relay on
    Last_event_time = millis();                                                                                                       // Record event time for checking if windlass is jammed
    OnOff = 1;                                                                                                                        // With app on
    Preset = 0;                                                                                                                       // and preset off 
  Last_stop_time = millis();                                                                                                          // Record event time for checking if windlass is jammed
  }
}
//HANDLE STOP REQUEST
void Event_Stop() {  
  server.send(200, "text/plain", "-1000");  // Send response "-1000" means no  chainlength
  Serial.print("Stop button pressed - setting both Chain Pins to Low, current UpDown state is ");                                     // Debug message
  Serial.println(UpDown);                                                                                                             // Debug message    
  Last_stop_time = millis();
  digitalWrite(Chain_Up_Pin, LOW );                                                       // Make sure both relays are off
  digitalWrite(Chain_Down_Pin, LOW );
  OnOff = 0;                                                                              // With app off
  Preset = 0;                                                                             // And Preset off
}


//HANDLE REQUEST TO RESET COUNTER TO ZERO
void Event_Reset() {        
    ChainCounter = 0;                                             //Reset counter to zero
  server.send(200, "text/plain", "-1000");  // Send response "-1000" means no  chainlength
  Serial.println("Reset gauge to zero");                          //Debug message
}


//HANDLE PRESET SCOPE LENGTH REQUEST
void Event_Preset() {               
  server.send(200, "text/html", indexHTML);                                                                                           // Refresh browser page (ideally don't but can't work out how not to)
  Scope = (server.arg(0)).toInt();                                                                                                    // Take preset scope from input and convert it to an integer     

  Serial.print("Scope has been set to ");                                                                                             // Send length to serial print for debugging
  Serial.println(Scope);

  Preset = 1;                                                                                                                         // Set Flag for preset scope logic
  if (Scope - (ChainCounter*Chain_Calibration_Value) > 0) {                                                                           // Determine if chain needs to go out to reach preset scope
  Serial.println ("Chain needs to go out by ");                                                                                       // Debug message 
  Serial.println (Scope - (ChainCounter*Chain_Calibration_Value));                                                                    // Debug message
  UpDown =1;                                                                                                                          // Set counter for down                                 
  digitalWrite(Chain_Up_Pin, LOW);                                                                                                    //Make sure up relay is off and 
  digitalWrite(Chain_Down_Pin, HIGH);                                                                                                // Turn on down relay
  OnOff = 1;                                                                                                                        // And flag that app is on

  } else {                                                                                                                             //Chain needs to come up to reach preset scope
  Serial.println ("Chain needs to go up by ");
  Serial.println ((ChainCounter*Chain_Calibration_Value)-Scope);
  UpDown = -1;                                                                                                                        //Set counter for up
  digitalWrite(Chain_Down_Pin, LOW);                                                                                                  //Make sure down relay is off and 
  digitalWrite(Chain_Up_Pin, HIGH);                                                                                                   // Turn on up relay
  OnOff = 1;                                                                                                                          // And flag that app is on
  }
  Last_event_time = millis(); 
  Watchdog_Timer = millis();     
}

void Event_Index() {                         // If "http://<ip address>/" requested
  server.send(200, "text/html", indexHTML);  // Send Index Website
}


void Event_js() {                            // If "http://<ip address>/gauge.min.js" requested
  server.send(200, "text/html", gauge);      // Then send gauge.min.js
}


void Event_ChainCount() {                    // If  "http://<ip address>/ADC.txt" requested

  float temp = (ChainCounter * Chain_Calibration_Value); // Chain in metres
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "text/plain", String (temp));

  Watchdog_Timer = millis();                 // Watchdog timer is set to current uptime

#if ENABLE_DEMO == 1                         // Demo Mode - Counts automatically UP/Down every 500 ms

  if (OnOff == 1) ChainCounter += UpDown;

  if ((Preset == 1) && (UpDown == 1) && (Scope-(ChainCounter*Chain_Calibration_Value) <= 0 )) {
    Serial.println("Distance to go = ");
    Serial.println(Scope-(ChainCounter*Chain_Calibration_Value));
    Serial.println("Executing preset chain down off");
    OnOff = 0;
    Last_stop_time = millis();
    Preset = 0;
    
    digitalWrite(Chain_Up_Pin, LOW );
    digitalWrite(Chain_Down_Pin, LOW);
  }
if ((Preset == 1) && (UpDown == -1) && ((ChainCounter*Chain_Calibration_Value)-Scope <= 0 )) {
    Serial.println("Distance to go = ");
    Serial.println((ChainCounter*Chain_Calibration_Value)-Scope);
    Serial.println("Executing preset chain up off");
    OnOff = 0;
    Last_stop_time = millis();
    Preset = 0;
   
    digitalWrite(Chain_Up_Pin, LOW );
    digitalWrite(Chain_Down_Pin, LOW);
}
  if ( ( (ChainCounter <= SAFETY_STOP) && (UpDown == -1) && (OnOff == 1) )) {    // Safety stop counter reached while chain is going up
    digitalWrite(Chain_Up_Pin, LOW );
    digitalWrite(Chain_Down_Pin, LOW );
    OnOff = 0;
    Last_stop_time = millis();
    Serial.println("Up safety stop reached");
    }
  
  
  if  ( (UpDown == 1) && (abs(ChainCounter) * Chain_Calibration_Value >= MAX_CHAIN_LENGTH) )  {  // Maximum chain length reached

    digitalWrite(Chain_Up_Pin, LOW );
    digitalWrite(Chain_Down_Pin, LOW );
    OnOff = 0;
    Serial.print("Maximum chain length reached - setting Chain_Down_Pin to Low and UpDown state is ");
    Serial.println(UpDown);
  }
  Last_event_time = millis();
#endif

}


void handleNotFound() {                                           // Unknown request. Send error 404
  server.send(404, "text/plain", "File Not Found\n\n");
}


void loop() {
  int wifi_retry = 0;
  MDNS.update();                                                    // Check to see if browser is looking for IP address
  server.handleClient();                                           // Handle HTTP requests

  if ( ( millis() > Watchdog_Timer + 1000 ) ||                     // Check HTTP connnection for activity in last 1 second (>1 second implies Wifi connection lost)
       ( (OnOff == 1) && (millis() > Last_event_time + 1500)) )  { // Check if engine is on but with no in last 1.5 seconds (>1.5 seconds implies windlass is not rotating/jammed)

    digitalWrite(Chain_Up_Pin, LOW );                              // If either show no activity then turn relays off   COMMENT OUT THESE THREE LINES TO BENCH TEST
    digitalWrite(Chain_Down_Pin, LOW );
    OnOff = 0;
    Serial.println("Turning off due to watchdog or event timer");
  }

  if ((OnOff == 1 && (digitalRead(Chain_Up_Override_Pin) == LOW)) || (OnOff == 1 && (digitalRead(Chain_Down_Override_Pin)) == LOW)) { //If the app is on and one of the relays is operated
    digitalWrite(BUILTIN_LED, LOW);                                                     // then turn on the LED
  } else {                                                                              // otherwise
    digitalWrite(BUILTIN_LED, HIGH);                                                    // turn it off
  }

  if (OnOff == 0) {                                     // If App is off  ...
    if (digitalRead(Chain_Up_Override_Pin) == LOW && millis() > Last_stop_time + 2500) {      // and there is a manual chain up over-ride
      UpDown = -1;                                          // set counter to reduce chain count
    } else if ( millis() > Last_stop_time + 2500) {
      UpDown = 1;                                           // otherwise if App is off then set to increase counter ie free fall or powered down..  Timer stops chain pulses as chain up slows to a stop being treated as down.
    }
  }
 if (millis() - Sprint_timer >500) {
Serial.print("Chain count is ");
Serial.print(ChainCounter * Chain_Calibration_Value); 
Serial.print("UpDown = ");
Serial.print(UpDown);
Serial.print(" Up Override = ");
Serial.print(digitalRead(Chain_Up_Override_Pin));
Serial.print(" Down Override = ");
Serial.println(digitalRead(Chain_Down_Override_Pin));
Sprint_timer = millis();
 }
  if (ChainCounter != LastSavedCounter) {                          // If ChainCounter has changed then store new value to nonvolatile storage (if changed)
    EEPROM.write(0, ChainCounter);
    EEPROM.commit();
    LastSavedCounter = ChainCounter;


    float temp = (ChainCounter * Chain_Calibration_Value);            // Caluclate new length to send to SignalK

    char udpmessage[1024];      // This and next few lines send the new length to SignalK.  You can change the  source if you wish.
   sprintf(udpmessage, "{\"updates\":[{\"$source\":\"ESP8266.Windlass.Control\",\"values\":[{\"path\":\"navigation.anchor.chainlength\",\"value\":%f}]}]}", temp);
    Udp.beginPacket(ip1, outPort);
    Udp.write(udpmessage);
    Udp.endPacket();


    // Serial.println(udpmessage); // print udpmessage to serial monitor in Arduino IDE, so can check JSON structure of the message is correct

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

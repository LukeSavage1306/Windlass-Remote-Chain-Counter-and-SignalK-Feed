# Windlass Remote Control with Chain Counter and Signalk Feed

Credit for the original version of this app goes to AK-Hombergers.  This version builds upon his original design with input from me and JeremyP.  As at December 18th I have yet to install and test on the boat but it is working "on the bench" - so use with caution and absolutley no warranty!  In its current format it:

* Is based on a Wemos D1 Mini (ESP8266) reading pulses from a conventional windlass chain sensor and WiFi access to effect control
* Provides a wireless remote windlass control from any phone/tablet with wifi capability giving up/down/stop/chain-length-counter/counter-reset functionality
* Provides chain-length to SignalK via a UDP connection
* Works seamlessly with 12 and 24 volt systems - no set up necessary 
* Will also track chain-length when the windlass is operated by existing controls or when allowed to "free fall"
* LED ON indicates that the board should be operating the windlass.  Off indicates board is in standb

To implement you need to
* Build and install the windlass control board - PCB design available from the link below
* Configure the code (in Arduino)
   - WLAN type setting WiFiMode_AP_STA to "0" (stand alone access point) or "1" (Client with DHCP for SignalK feed)
   - Configure your wifi SSID and password
   - Calibrate for your sensor (e.g. 0.33. meter per revolution of the gypsy) and the maximum chain length
   - Flash the code to the ESP8266

If working as a standalone Access Point, connect the phone/tablet to the defined network and start "192.168.4.1" in the browser.
If working as WLAN client, determine the DHCP IP address using the Arduino Serial Monitor (or other IDE) and start your browser with that IP address.

This should give you a screen as per below on your phone/tablet

![Picture1](https://github.com/LukeSavage1306/Windlass-Remote-Chain-Counter-and-SignalK-Feed/blob/main/IMG_1254.PNG)

To control the anchor chain relay just press:
- "Down" for anchor down
- "Up" for anchor up
- "Stop" for Stop
- "Reset" to reset the chain counter to zero

Features:
- Safety stop to stop "anchor up" two gypsy revolutions before reaching zero (can be changed in code with SAFETY_STOP).
- Safety stop if maximum chain length is reached (standard 40 meters, can be changed with MAX_CHAIN_LENGTH)
- Watchdog timer to stop power after 1 second inactivity of client (e.g. due to connection problems).
- Watchdog timer to detect blocked chain. Windlass stops if no events are detected within 1 second for up/down command.
- Current Chain Counter is stored in nonvolatile memory. ESP can be switched off after anchoring and counter is restored after powering back up.
- Demo mode to check functionality without having a windlass / chain counter connected to ESP (set ENABLE_DEMO to 1).
- Detects windlass operation from other existing controls and counts chain for up/down/free fall

![Picture2](https://github.com/LukeSavage1306/Windlass-Remote-Chain-Counter-and-SignalK-Feed/blob/main/24vWindlassRemoteAndChainCounter.JPG)

A [pcb layout](https://github.com/LukeSavage1306/Windlass-Remote-Chain-Counter-and-SignalK-Feed/blob/main/24vD1MiniWindlassControlAndChainCounter.kicad_pcb) is available in the main folder. 

You can order a PCB from Aisler.net:https://aisler.net/p/ESKPZAUS

![Board](https://github.com/LukeSavage1306/Windlass-Remote-Chain-Counter-and-SignalK-Feed/blob/main/WindlassPCB.JPG)

The current design should work for a Quick or Lofrans anchor chain relay and chain sensor (which looks like a simple reed relay triggerd from a magnet). Connection details  for a Quick windlass/counter can be found here: https://www.quickitaly.com/resources/downloads_qne-prod/1/CHC1203_IT-EN-FR_REV001A.pdf

To send data to SignalK you need to define a UDP connection.  Go to Server, Connections and define a new connection as per the picture below.  Match the port number to the number you used in the .ino code.  Once configured ChainLength should appear as a data item within the Data Browser.

![UDP connection](https://github.com/LukeSavage1306/Windlass-Remote-Chain-Counter-and-SignalK-Feed/blob/main/UDPconnectionconfiguration.png)

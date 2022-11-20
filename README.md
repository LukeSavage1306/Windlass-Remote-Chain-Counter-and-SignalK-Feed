# Windlass Remote Control with Chain Counter and Signalk Feed

NOTE: As at mid November 2022 the code is the latest version as described below, but I have yet to upload the latest images, circuit schematic, pcb link etc.  Should be done in next week or so.


Credit for the original version of this app goes to AK-Hombergers.

The current version:

* Is based on a Wemos D1 Mini (ESP8266) reading pulses from a conventional windlass chain sensor with WiFi access to effect control either directly in AP mode or via the boats main wifi in station mode.
* In AP mode uses mDNS for easy access via a named URL rather than remembering IP addresses
* Provides a wireless remote windlass control from any phone/tablet with wifi capability giving up/down/stop/chain-length-counter/counter-reset functionality and the ability to enter a preset scope to which the windlass will automatically raise/lower the chain.
* Provides chain-length to SignalK via a UDP connection
* Works with 12 volt systems - just requires a single transient suppressor diode to be changed for 24v 
* Will also track chain-length when the windlass is operated by existing controls or when allowed to "free fall"
* LED ON indicates that the board should be operating the windlass.  Off indicates board is in standby
* Stores current chain length in nonvolatile memory so that length is stored on switch off and restored after powering back up.
* Demo mode to check correct operation without need for a gypsy sensor present.
* Detects windlass operation from other existing controls and counts chain for up/down/free fall

Includes various safety features:

- Safety interlock so that Up/Down will not work if another (existing) control is already operating the windlass.
- Safety stop to stop "anchor up" a preset number of gypsy revolutions before reaching zero (can be changed in code with SAFETY_STOP).
- Safety stop if maximum chain length is reached (can be changed with MAX_CHAIN_LENGTH)
- Safety watchdog timer to stop power after predetermined period of inactivity of client (e.g. due to loss of wifi connection).
- Safety watchdog timer to detect blocked chain. Windlass stops if no events are detected within predetermined period of activity after up/down command.


To implement you need to
* Build and install the windlass control board - PCB design available from the link below
* Configure the code (in Arduino)
   - WLAN type setting WiFiMode_AP_STA to "0" (stand alone access point) or "1" (Client with DHCP for SignalK feed).  Note code assumes SignalK server is at 10.10.10.1.  Configure to suit your set up
   - Configure your wifi SSID and password
   - Calibrate for your sensor (e.g. 0.35 meter per revolution of the gypsy) and the maximum chain length
   - Flash the code to the ESP8266

If working as a standalone Access Point, connect the phone/tablet to the defined network and start "192.168.4.1" in the browser.
If working as WLAN client, determine the DHCP IP address using the Arduino Serial Monitor (or other IDE) and start your browser with that IP address.

This should give you a screen as per below on your phone/tablet.  A flashing red dot in the title bar indicates connection established with the Wemos.

![Picture1](https://github.com/LukeSavage1306/Windlass-Remote-Chain-Counter-and-SignalK-Feed/blob/main/IMG_1254.PNG)

To control the anchor chain relay just press:
- "Down" for anchor down
- "Up" for anchor up
- "Stop" for Stop
- "Reset" to reset the chain counter to zero
- Or enter the desired scope and hit "Target Scope" to pay out/pull in to the set length



![Picture2](https://github.com/LukeSavage1306/Windlass-Remote-Chain-Counter-and-SignalK-Feed/blob/main/24vWindlassRemoteAndChainCounter.JPG)

A [pcb layout](https://github.com/LukeSavage1306/Windlass-Remote-Chain-Counter-and-SignalK-Feed/blob/main/24vD1MiniWindlassControlAndChainCounter.kicad_pcb) is available in the main folder. 

You can order a PCB from Aisler.net:https://aisler.net/p/ESKPZAUS

![Board](https://github.com/LukeSavage1306/Windlass-Remote-Chain-Counter-and-SignalK-Feed/blob/main/WindlassPCB.JPG)

## Parts:

Parts list currently points to largely German suppliers but are generally widely available with the exception of the PCB. If you want to source these elsewhere I can provide the detailed KiCad files. 

- Board [Link](https://aisler.net/p/ESKPZAUS)
- U1 D1 Mini [Link](https://www.reichelt.de/de/en/d1-mini-esp8266-v3-0-d1-mini-p253978.html?&nbc=1)
- U3 K7805 1000R3L [Link](https://uk.rs-online.com/web/p/switching-regulators/1934015/)
- C1 10uF 50v can electrolytic (can be reduced to 25v if running on a 12V system)
- C2 22uF 15v can electrolytic 
- R1 Resistor 5 KOhm (if using 12v you can reduce this to 2.2 Kohm to ensure accurate counting at low battery voltage) [Link](https://www.reichelt.de/de/en/carbon-film-resistor-1-4-w-5-1-0-kilo-ohms-1-4w-1-0k-p1315.html?&trstct=pos_2&nbc=1)
- R2, R3 Resistor 270 Ohm (*2) [Link](https://www.reichelt.de/de/en/carbon-film-resistor-1-4-w-5-270-ohm-1-4w-270-p1390.html?&nbc=1)
- R4, R5 Resistor 10 KOhm (*2) [Link](https://www.reichelt.de/de/en/carbon-film-resistor-1-4w-5-10-kilo-ohms-1-4w-10k-p1338.html?&nbc=1)
- Q1, Q2, Q3, Q4 Transistor BC337 (*4) [Link](https://www.reichelt.de/de/en/transistor-to-92-bl-npn-45v-800ma-bc-337-25-dio-p219125.html?&nbc=1)
- D2, D3 Diode 1N4148 (*2) [Link](https://www.reichelt.de/schalt-diode-100-v-150-ma-do-35-1n-4148-p1730.html?search=1n4148)
- D1 Diode 1N4001 [Link](https://www.reichelt.de/de/en/rectifier-diode-do41-50-v-1-a-1n-4001-p1723.html?&nbc=1)
- U2 H11-L1 [Link](https://www.reichelt.de/optokoppler-1-mbit-s-dil-6-h11l1m-p219351.html?search=H11-l1)
- K1, K2 Relay (*2) [Link](https://www.reichelt.de/de/en/miniature-power-relay-g5q-1-no-5-v-dc-5-a-g5q-1a-eu-5dc-p258331.html?&nbc=1)
- J1, J3, J4 Connector 2-pin (*3) [Link](https://www.reichelt.de/de/en/2-pin-terminal-strip-spacing-5-08-akl-101-02-p36605.html?&nbc=1)

The current design should work for a Quick or Lofrans anchor chain relay and chain sensor (which looks like a simple reed relay triggerd from a magnet). Connection details  for a Quick windlass/counter can be found here: https://www.quickitaly.com/resources/downloads_qne-prod/1/CHC1203_IT-EN-FR_REV001A.pdf

The board requires connection to your windlass power supply (12v or 24v - no configuration necessary), connection to your windlass sensor (polarity of connection does not matter) and connection of the Up and Down relay feeds to the existing Up and Down terminals on the main windlass relay (note: the switching terminals that connect to any existing switch controls - NOT the motor output terminals).

To send data to SignalK you need to define a UDP connection.  Go to Server, Connections and define a new connection as per the picture below.  Match the port number to the number you used in the .ino code (default is 4210).  Once configured ChainLength should appear as a data item within the Data Browser.

![UDP connection](https://github.com/LukeSavage1306/Windlass-Remote-Chain-Counter-and-SignalK-Feed/blob/main/UDPconnectionconfiguration.png)

The gauge display can be changed to mirror your particular chain length in index_html.h   The range is set at lines 133/134 and the coloured zones at line 144

# Windlass Remote Control with Chain Counter and Signalk Feed

This code builds on the work of AK-Hombergers original design.  In its current format it:

* Is based on a Wemos D1 Mini (ESP8266) reading pulses from a conventional windlass chain sensor and WiFi access to effect control
* Provides a wireless remote windlass control from any phone/tablet with wifi capability giving up/down/stop/chain-length-counter/counter-reset functionality
* Provides chain-length to SignalK via a UDP connection
* Works with 12 and 24 volt systems
* Will also track chain-length when the windlass is operated by existing controls or when allowed to "free fall"

To implement you need to
* Build and install the windlass control board - PCB design available from the link below
* Configure the code (in Arduino) for: 
    WLAN type setting WiFiMode_AP_STA to "0" (stand alone access point) or "1" (Client with DHCP for SignalK feed)
    Configure your wifi SSID and password
    Calibrate for your sensor (e.g. 0.33. meter per revolution of the gypsy) and the maximum chain length
    Flash the code to the ESP8266

If working as a standalone Access Point, connect the phone/tablet to the defined network and start "192.168.4.1" in the browser.
If working as WLAN client, determine the DHCP IP address with Serial Monitor of IDE and start your browser with that IP address.

This should give you a screen as per below on your phone/tablet

![Picture1](https://github.com/AK-Homberger/ESP32_ChainCounter_WLAN/blob/master/IMG_1254.PNG)

To control the anchor chain relay just press:
- "Down" for anchor down
- "Up" for anchor up
- "Stop" for Stop
- "Reset" to reset the chain counter to zero

Features:
- Saftey stop to stop "anchor up" two gypsy revolutions before reaching zero (can be changed in code with SAFETY_STOP).
- Safety stop if maximum chain length is reached (standard 40 meters, can be changed with MAX_CHAIN_LENGTH)
- Watchdog timer to stop power after 1 second inactivity of client (e.g. due to connection problems).
- Watchdog timer to detect blocked chain. Windlass stops if no events are detected within 1 second for up/down command.
- Current Chain Counter is stored in nonvolatile memory. ESP can be switched off after anchoring and counter is restored after powering back up.
- Demo mode to check functionality without having a windlass / chain counter connected to ESP (set ENABLE_DEMO to 1).
- Detects windlass operation from other existing controls and counts chain for up/down/free fall

![Picture2](https://github.com/LukeSavage1306/Windlass-Remote-Chain-Counter-and-SignalK-Feed/blob/main/24vWindlassRemoteAndChainCounter.JPG)

A [pcb layout](https://github.com/AK-Homberger/ESP8266_AnchorChainContol_WLAN/blob/main/D1MiniChainCounterWLAN-Board.pdf) is available in the main folder: "D1MiniChainCounterWLAN.kicad_pcb".

You can order a PCB from Aisler.net: https://aisler.net/p/WJSHXVDM

![Board](https://github.com/AK-Homberger/ESP8266_AnchorChainContol_WLAN/blob/main/D1MiniChainCounterWLAN.png)

The current design should work for a Quick or Lofrans anchor chain relay and chain sensor (which looks like a simple reed relay triggerd from a magnet). Connection details  for a Quick windlass/counter can be found here: https://www.quickitaly.com/resources/downloads_qne-prod/1/CHC1203_IT-EN-FR_REV001A.pdf

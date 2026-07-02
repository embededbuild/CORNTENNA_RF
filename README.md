🌽 CORNTENNA_RF

[ LOCAL NETWORK RECON TERMINAL ]

"The network talks. We just listen — from a distance."

Built on two ESP32s and a pair of nRF24L01+ radios. Scan the network on one device, beam the results to another over RF — no cable, no Wi-Fi handoff, no laptop required.


What is CORNTENNA_RF?

CORNTENNA_RF is a two-unit network reconnaissance rig designed to map the digital neighborhood around you — and whisper what it finds to a second device over its own private radio channel.

Plug in the TX unit.

Connect to a network.

Watch the devices reveal themselves.

Routers.
Phones.
Printers.
Smart TVs.
Mystery boxes with suspiciously open ports.

Every device leaves a trail.

CORNTENNA_RF follows it — and hands the results off to the RX unit, sitting wherever you left it, listening on channel 118.


How It Talks to Itself

 [ TX UNIT ]                              [ RX UNIT ]
 joins the target Wi-Fi         RF24      sits elsewhere
 discovers devices      ───────────────▶  listens on channel 118
 scans open ports              2.4GHz     renders the results
 no cable required                        no Wi-Fi radio required

The TX unit never needs to talk to a screen. The RX unit never needs to touch the target network. Between them: one shared channel, one shared address, one shared key.


Features

📡 Wireless Recon


Scan nearby Wi-Fi networks
View signal strength
Identify security types
Interactive network selection


👁 Device Discovery


ARP-assisted device detection
Local subnet enumeration
MAC address collection
Live device inventory


🔍 Port Recon

Scans common services including:

21   FTP
22   SSH
23   Telnet
80   HTTP
443  HTTPS
445  SMB
554  RTSP
8080 HTTP-Alt
8443 HTTPS-Alt
9100 Printer

🧬 Device Fingerprinting

Recognizes hardware vendors including:


Apple
Samsung
Cisco
Google
Amazon
Ubiquiti
Netgear
TP-Link
Raspberry Pi
Espressif
Sony
Microsoft
Synology


⚡ Connection Intelligence


RSSI monitoring
Signal quality grading
Gateway information
Local IP information
Subnet details


🛰️ Silent Handoff


Every discovered device is packed and fired over nRF24L01+
A shared auth key gates every payload — anything unmatched is dropped, no questions asked
A "flush" packet closes out each scan so the RX unit knows the transmission is done
Onboard LEDs on both units report status without a screen in sight



Terminal Commands

(TX unit — serial console, 115200 baud)

WIFI         Scan and connect
STATUS       Display connection info
SCAN         Discover devices, transmit results to RX
LIST         Show local device inventory
DISCONNECT   Drop Wi-Fi connection
CLEAR        Clear discovered devices
PING         Test console response

(RX unit — no commands, just listens)

Blue LED   listening
Red LED    device received


Example Session

>> WIFI

[1] CoffeeShop_WiFi
[2] Home_Network
[3] Definitely_Not_FBI_Van

>> 2

>> Connected!

IP Address : 192.0.2.101

>> SCAN

>> Running TCP discovery...

>> Device: 192.0.2.10
>> Open ports: 80,443

>> Device: 198.51.100.22
>> Open ports: 22,80

>> Device: 203.0.113.7
>> Open ports: 9100

>> TCP discovery done
>> transmitting devices to the TX unit....
>> Transmission complete

>> LIST

  +===================+==================+=============================+===========+==========+
  || MAC              || IP              || OPEN PORTS                || Device    || Source  |
  +===================+==================+=============================+===========+==========+
  || B8:3E:59:XX:XX:X || 192.0.2.10      || 80,443                    || Apple     || TCP     |
  || DC:A6:32:XX:XX:X || 198.51.100.22   || 22,80                     || Raspberry || TCP     |
  || A0:B3:CC:XX:XX:X || 203.0.113.7     || 9100                      || Printer   || TCP     |
  +===================+==================+=============================+===========+==========+
   Total: 3

Meanwhile, on the RX unit, sitting quietly somewhere else entirely:

>> Incoming recon data from TX unit:

 +===================+==================+=================+===========+
 ||      MAC         ||       IP        ||   OPEN PORTS   || Vendor   |
 +===================+==================+=================+===========+
 || B8:3E:59:XX:XX:X || 192.0.2.10      || 80,443         || Apple    |
 || DC:A6:32:XX:XX:X || 198.51.100.22   || 22,80          || RPi Found.|
 || A0:B3:CC:XX:XX:X || 203.0.113.7     || 9100           || HP       |
 +===================+==================+=================+===========+
 Total received: 3


Hardware

2x ESP32 Development Board
2x nRF24L01+ Radio Module
USB Cables
Serial Terminal
Questionable Sleep Schedule

Wiring

SignalTX UnitRX UnitCEGPIO 22GPIO 4CSNGPIO 21GPIO 5SCKGPIO 18GPIO 18MOSIGPIO 23GPIO 23MISOGPIO 19GPIO 19LED BlueGPIO 26GPIO 16LED RedGPIO 27GPIO 17

Both units ship pre-paired — same channel, same address, same key. Change one, change them all.


Build

Requirements


Arduino IDE
ESP32 Board Package
RF24 Library


Libraries

WiFi.h
esp_wifi.h
WiFiUdp.h
lwip/etharp.h
lwip/netif.h
lwip/tcpip.h
SPI.h
RF24.h

Open CORNTENNA_TX.ino in the Arduino IDE. Flash it to unit one.

Open CORNTENNA_RX.ino in the Arduino IDE. Flash it to unit two.

Board: ESP32 Dev Module
Baud: 115200

Compile.

Upload.

Separate them.

Observe.


Philosophy

Networks are ecosystems.

Every device broadcasts a story.

Most people never notice.

CORNTENNA_RF exists for the people who do.

Not because we want to break things.

Because we want to understand them.


Legal

Only scan networks you own or have permission to analyze.

Don't be the reason your ISP sends weird emails.


WE ARE ALL JUST DEVICES ON SOMEBODY ELSE'S NETWORK.

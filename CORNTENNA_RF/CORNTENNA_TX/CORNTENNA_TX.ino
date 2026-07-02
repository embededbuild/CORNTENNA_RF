#include <WiFi.h>
#include <esp_wifi.h>
#include <SPI.h>
#include <RF24.h>
#include <WiFiUdp.h>
#include <Arduino.h>
#include <lwip/etharp.h>
#include <lwip/netif.h>
#include <lwip/tcpip.h>

#define MAX_DEVICES 128

//RF24 tx setup bitchass
#define PIN_CE  22
#define PIN_CSN 21
#define PIN_SCK 18
#define PIN_MOSI 23
#define PIN_MISO 19
#define PIN_LED_BLUE 26
#define PIN_LED_RED 27

#define AUTH_KEY 0x4E
#define RF_CHANNEL 118
const byte ADDRESS[6] = "C0RN1";

RF24 radio(PIN_CE, PIN_CSN);


struct DevicePayload {
  uint8_t ip[4];
  uint8_t mac[6];
  uint8_t portMask;
  uint8_t authKey;
};

struct DeviceInfo {
  IPAddress ip;
  char    mac[18];
  char    ports[32];
  char    vendor[48];
  char    source[12];
};

struct Blinker {
  uint8_t  pin;
  bool     active;
  int      blinksLeft;
  uint32_t lastToggle;
  uint16_t interval;
  bool     ledState;
};

Blinker blueBlink = {PIN_LED_BLUE, false, 0, 0, 150, false};
Blinker redBlink = {PIN_LED_RED, false, 0, 0, 100, false};

void startBlink(Blinker& b, int count, uint16_t intervalMs) {
  b.active   = true;
  b.blinksLeft = count * 2;
  b.interval = intervalMs;
  b.lastToggle = millis();
  b.ledState = true;
  digitalWrite(b.pin, HIGH);
}

void updateBlinker(Blinker& b) {
  if (!b.active) return;
  uint32_t now = millis();
  if (now - b.lastToggle >= b.interval) {
    b.lastToggle = now;
    b.blinksLeft--;
    if (b.blinksLeft <= 0) {
      b.active = false;
      digitalWrite(b.pin, LOW);
    } else {
      b.ledState = !b.ledState;
      digitalWrite(b.pin, b.ledState ? HIGH : LOW);
    }
  }
}


void listDevices();

DeviceInfo devices[MAX_DEVICES];
int   deviceCount = 0;

String inputBuffer = "";
bool wifiConnected = false;

//scan & print available nettwerk
void scanNetwork() {
  Serial.println("\n>> Scanning for networks....");
  int found = WiFi.scanNetworks();

  if (found == 0) {
    Serial.println("\n>> no networks found");
    return;
  }

  for (int i = 0; i < found; i++){
    char line[80];
    String ssid = WiFi.SSID(i);
    if (ssid.length() > 30) ssid = ssid.substring(0, 30) + "..";

    String sec;
    switch (WiFi.encryptionType(i)) {

      case WIFI_AUTH_OPEN:
      sec = "[Open]";
      break;

      case WIFI_AUTH_WEP:
      sec = "[WEP]";
      break;

      case WIFI_AUTH_WPA_PSK:
      sec = "[WPA]";
      break;

      case WIFI_AUTH_WPA_WPA2_PSK:
      sec = "[WPA2]";
      break;

      case WIFI_AUTH_WPA2_ENTERPRISE:
      sec = "[WPA2-ENTERPRISE]";
      break;

      case WIFI_AUTH_WPA3_PSK:
      sec = "[WPA3]";
      break;

      case WIFI_AUTH_WPA2_WPA3_PSK:
      sec = "[WPA2/WPA3]";
      break;

      case WIFI_AUTH_WAPI_PSK:
      sec = "[WAPI-china wlan]";
      break;

      default:
      sec = "[Unknown]";
      break;
    }
    snprintf(line, sizeof(line), " %-2d | %-30s | %4d | %s",
    i + 1,
    ssid.c_str(),
    WiFi.RSSI(i),
    sec.c_str()
    );
    Serial.println(line);
  }
  Serial.println();
  Serial.println(">> Type the NUMBER of the network to connect to:");
}

//conect to the chosen network like 1805 auction house
void connectToNetwork(const String& ssid, const String& password) {
  Serial.print("\n>> Connecting to: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  Serial.print(">> ");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempts++;
    if (attempts > 30) {
      Serial.println("\n>> ERROR: Connection Failed. wrong password?");
      Serial.println("\n>> Type WIFI to try again");
      return;
    }
  }

  wifiConnected = true;
  Serial.println("\n");
  Serial.println(">> Connected!");
  Serial.print(" IP Address : "); Serial.println(WiFi.localIP());
  Serial.print(" Subnet Mask: "); Serial.println(WiFi.subnetMask());
  Serial.print(" Gateway    : "); Serial.println(WiFi.gatewayIP());
  Serial.print(" Signal     : "); Serial.println(WiFi.RSSI()); Serial.println(" dBm");
  Serial.println();
  Serial.println(">> Ready. Commands: PING | STATUS | SCAN | LIST | DISCONNECT | CLEAR | WIFI");
}

//setup stat machine do you want to play a game 
// states: IDLE, AWAIT_NET_CHOICE, AWAIT_PASSWORD
enum SetupState { IDLE, AWAIT_NET_CHOICE, AWAIT_PASSWORD };
SetupState setupState = IDLE;

int chosenNetIndex = -1;
String chosenSSID  = "";
int networkCount   = 0;

void handleSetupInput(const String& input) {
  if (setupState == AWAIT_NET_CHOICE) {
    int choice = input.toInt();
    networkCount = WiFi.scanComplete();

    if (choice < 1 || choice > networkCount) {
      Serial.println(">> invalid choice, try again:");
      return;
    }

    chosenNetIndex = choice - 1;
    chosenSSID     = WiFi.SSID(chosenNetIndex);

    if (WiFi.encryptionType(chosenNetIndex) == WIFI_AUTH_OPEN) {
      //open network - no password needed
      connectToNetwork(chosenSSID, "");
      setupState = IDLE;
    } else {
      Serial.print(">> Enter password for \"");
      Serial.print(chosenSSID);
      Serial.println(" (typing is hidden for security)");
      setupState = AWAIT_PASSWORD;
    }

  } else if (setupState == AWAIT_PASSWORD) {
    connectToNetwork(chosenSSID, input);
    setupState = IDLE;
  }
}

void wifiDisconnect() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  wifiConnected = false;
  Serial.println("\n>> wifi is disconnect - type WIFI if you want to connect back or select a different one");
}

//help
void formatMac(const uint8_t* mac, char* out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

DeviceInfo* findDevice(IPAddress ip) {
  for (int i = 0; i < deviceCount; i++)
    if (devices[i].ip == ip) return &devices[i];
    return nullptr;
}

const uint32_t scanPorts[] = {21, 22, 23, 80, 443, 445, 554, 8080, 8443, 9100};
const int numScanPorts = 10;

void macStringToBytes(const char* macStr, uint8_t* out) {
  sscanf(macStr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &out[0], &out[1], &out[2], &out[3], &out[4], &out[5]);
}

uint8_t portsToBitmask(const char* portsStr) {
  uint8_t mask = 0;
  if (strcmp(portsStr, "none") == 0 || strcmp(portsStr, "Scanning") == 0) return mask;

  char buf[32];
  strncpy(buf, portsStr, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  char* token = strtok(buf, ",");
  while (token != nullptr) {
    int port = atoi(token);
    for (int i = 0; i < numScanPorts; i++) {
      if ((int)scanPorts[i] == port) {
        mask |= (1 << i);
        break;
      }
    }
    token = strtok(nullptr, ",");
  }
  return mask;
}

void scanDevicePorts(DeviceInfo* device) {
  char result[32] = "";

  for (int p = 0; p < numScanPorts; p++) {
    WiFiClient client;
    client.setTimeout(50);

    if (client.connect(device->ip, scanPorts[p])) {
      client.stop();

      //append port to result string
      char portStr[8];
      if (strlen(result) > 0) strncat(result, ",", sizeof(result) - strlen(result) - 1);
      snprintf(portStr, sizeof(portStr), "%d", scanPorts[p]);
      strncat(result, portStr, sizeof(result) - strlen(result) - 1);
    }
  }

  if (strlen(result) == 0) strncpy(result, "none", sizeof(result) - 1);
  strncpy(device->ports, result, sizeof(device->ports) - 1);
  device->ports[sizeof(device->ports) - 1] = '\0';
}

void transmitDevice(DeviceInfo* device) {
  DevicePayload payload;
  memset(&payload, 0, sizeof(payload));

  payload.ip[0] = device->ip[0];
  payload.ip[1] = device->ip[1];
  payload.ip[2] = device->ip[2];
  payload.ip[3] = device->ip[3];

  macStringToBytes(device->mac, payload.mac);
  payload.portMask = portsToBitmask(device->ports);
  payload.authKey = AUTH_KEY;

  radio.stopListening();
  bool ok = radio.write(&payload, sizeof(payload));
  radio.startListening();

  if (ok) {
    startBlink(redBlink, 1, 100);
  } 
}

void transmitFlush() {
  DevicePayload payload;
  memset(&payload, 0, sizeof(payload));
  payload.authKey = AUTH_KEY;

  radio.stopListening();
  radio.write(&payload, sizeof(payload));
  radio.startListening();
}

void addOrUpdateDevice(IPAddress ip, const uint8_t* mac, const char* source) {
  DeviceInfo* existing = findDevice(ip);

  if (existing) {
      // AARP shit i mean ARP entry - only fill in if we don't already have mac
      if (mac && strlen(existing->mac) == 0) {
        formatMac(mac, existing->mac);
      }
      return;
    }

  if  (deviceCount >= MAX_DEVICES) return;

  DeviceInfo& d = devices[deviceCount++];
  d.ip    = ip;
  if (mac) {
    formatMac(mac, d.mac);
  } else {
    strncpy(d.mac, "Unknown", sizeof(d.mac) -1);
    d.mac[sizeof(d.mac) - 1] = '\0';
  }
  strncpy(d.ports, "Scanning...", sizeof(d.ports) - 1);
  d.ports[sizeof(d.ports) - 1] = '\0';
  strncpy(d.vendor, "Unknown", sizeof(d.vendor) - 1);
  d.vendor[sizeof(d.vendor) - 1] = '\0';
  strncpy(d.source, source, sizeof(d.source) -1);
  d.source[sizeof(d.source) -1] = '\0';
}

/*getting furthur but still need to work on the device discovery so when doing a scan devices. That are connected
to the network will show up with the IP address / MAC address / Hostname / Device Type
maybe after food,weed and some chill the solution will cum to me.
*/

//see if this will make device appeare 
void tcpDiscovery() {
  IPAddress localIP = WiFi.localIP();
  IPAddress mask    = WiFi.subnetMask();

  uint32_t lip = (uint32_t)localIP[0] << 24 | (uint32_t)localIP[1] << 16 |
                 (uint32_t)localIP[2] << 8  | (uint32_t)localIP[3];
  uint32_t msk = (uint32_t)mask[0] << 24 | (uint32_t)mask[1] << 16 |
                 (uint32_t)mask[2] << 8  | (uint32_t)mask[3];

  uint32_t base = lip & msk;

  const uint32_t ranges[][2] = {
    {100, 199},
    {1,   99},
    {200, 254}
  };

  Serial.println(">> Running TCP discovery...");

  for (int r = 0; r < 3; r++) {
    for (uint32_t i = ranges[r][0]; i <= ranges[r][1]; i++) {
      uint32_t h = base + i;
      IPAddress target(
        (h >> 24) & 0xFF,
        (h >> 16) & 0xFF,
        (h >>  8) & 0xFF,
         h        & 0xFF
      );
      if (target == localIP) continue;

      WiFiClient client;
      client.setTimeout(50);

      bool deviceFound = false;
      if (client.connect(target, 80)) {
        Serial.print(">> Hit: ");
        Serial.print(target);
        Serial.println(" :80");
        client.stop();
        deviceFound = true;
      } else {
        delay(20);
      }

      // check ARP cache regardless of whether port 80 responded
      struct netif* nif = netif_default;
      ip4_addr_t ipaddr;
      IP4_ADDR(&ipaddr, target[0], target[1], target[2], target[3]);

      struct eth_addr* ethret = nullptr;
      const ip4_addr_t* ipret = nullptr;

      LOCK_TCPIP_CORE();
      int found = etharp_find_addr(nif, &ipaddr, &ethret, &ipret);
      UNLOCK_TCPIP_CORE();

      if (found >= 0 && ethret) {
        if (!deviceFound) {
          Serial.print(">> Device: ");
          Serial.println(target);
        }
        addOrUpdateDevice(target, ethret->addr, "TCP");

        //scan ports on newly found device
        DeviceInfo* d = findDevice(target);
        if (d) {
          Serial.print(">> Scanning ports on ");
          Serial.println(target);
          scanDevicePorts(d);
          Serial.print(">> Open ports: ");
          Serial.println(d->ports);
        }
      }
    }
  }

  Serial.println(">> TCP discovery done");
}


// full scan - passively listen to mDNS, then check the ARP table after we fuck your mom
void runArpScan() {
  if (!wifiConnected) {
    Serial.println("ERR not connected -- type WiFi first");
    return;
  }

  int prevCount = deviceCount;

  Serial.println("\n>> Starting passive scan...");
  digitalWrite(PIN_LED_BLUE, HIGH);
  tcpDiscovery();
  digitalWrite(PIN_LED_BLUE, LOW);

  int newFound = deviceCount - prevCount;
  Serial.print(">> Found "); Serial.print(newFound);
  Serial.print(" new | "); Serial.print(deviceCount);
  Serial.println(" total -- type LIST to see all devices");

  //transmot all devices over rf
  Serial.println(">> transmitting devices to the TX unit....");
  for (int i = 0; i < deviceCount; i++) {
    transmitDevice(&devices[i]);
    updateBlinker(redBlink);
    delay(50);
  }
  transmitFlush();
  Serial.println(">> Transmission complete");
}

//Function returns true if the signal is excellent, false otherwise
String isConnectionExcellent() {
  if (WiFi.status() != WL_CONNECTED) {
    return "Disconnected";
  }

  long rssi = WiFi.RSSI();
  
  if (rssi >= -50 && rssi < 0) return "Excellent";
  if (rssi >= -65)             return "Good";
  if (rssi >= -75)             return "Fair";
  return "Weak";
}

//command handle like a slave master
void processCommand(const String& raw) {
  // password input must not be uppercased - handle setup state first
  if (setupState != IDLE) {
    handleSetupInput(raw);
    return;
  }

  String cmd = raw;
  cmd.toUpperCase();
  cmd.trim();

  if (cmd == "PING") {
    Serial.println("Pong");
  } else if (cmd == "WIFI") {
    setupState = AWAIT_NET_CHOICE;
    scanNetwork();
  } else if (cmd == "STATUS") {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("Status wifi=");
      Serial.print(WiFi.SSID());
      Serial.print(" ip=");
      Serial.print(WiFi.localIP().toString());
      Serial.print(" rssi=");
      Serial.print(WiFi.RSSI());
      Serial.print("dBm");
      Serial.print("\n" "connection is: " + isConnectionExcellent());
    } else {
      Serial.println("Status wifi=disconnected -- type WiFi to connect");
    }

  } else if (cmd == "SCAN") {
    runArpScan();

  } else if (cmd == "LIST") {
    listDevices();

  } else if (cmd == "DISCONNECT") {
    wifiDisconnect();

  } else if (cmd == "CLEAR") {
    deviceCount = 0;
    Serial.println(">> Device list cleared");

  } else {
    Serial.print("ERR unknown command: ");
    Serial.print(cmd);
  }
}

struct OUIEntry {
  uint8_t oui[3];
  const char* vendor;
  const char* type;
};

const OUIEntry ouiTable[] = {
  //apple
  {{0xB8, 0x3E, 0x59}, "Apple",      "Apple Device"},
  {{0x40, 0xB0, 0x76}, "Apple",      "Apple Device"},
  {{0xA4, 0xC3, 0xF0}, "Apple",      "Apple Device"},
  {{0x8C, 0x85, 0x90}, "Apple",      "Apple Device"},
  {{0xF0, 0x18, 0x98}, "Apple",      "Apple Device"},
  {{0x00, 0x17, 0xF2}, "Apple",      "Apple Device"},
  {{0xAC, 0xDE, 0x48}, "Apple",      "Apple Device"},
  {{0x54, 0x26, 0x96}, "Apple",      "Apple Device"},
  //samsung
  {{0xD4, 0xE2, 0x2F}, "Samsung",    "Samsung Device"},
  {{0x8C, 0x77, 0x12}, "Samsung",    "Samsung Device"},
  {{0xA0, 0x07, 0x98}, "Samsung",    "Samsung Device"},
  {{0x30, 0x07, 0x4D}, "Samsung",    "Samsung Device"},
  {{0xCC, 0x07, 0xAB}, "Samsung",    "Samsung Device"},
  //TP-Link
  {{0xF0, 0x09, 0x0D}, "TP-Link",    "Router"},
  {{0x50, 0xC7, 0xBF}, "TP-Link",    "Router"},
  {{0xC4, 0xE9, 0x84}, "TP-Link",    "Router"},
  {{0x98, 0xDA, 0xC4}, "TP-Link",    "Router"},
  //Netgear
  {{0xA0, 0x04, 0x60}, "Netgear",    "Router"},
  {{0x6C, 0x40, 0x08}, "Netgear",    "Router"},
  {{0x9C, 0x3D, 0xCF}, "Netgear",    "Router"},
  //Cisco
  {{0x00, 0x1A, 0xA1}, "Cisco",      "Switch/Router"},
  {{0x00, 0x09, 0x97}, "Cisco",      "Switch/Router"},
  {{0xF8, 0x7B, 0x20}, "Cisco",      "Switch/Router"},
  //google
  {{0xF4, 0xF5, 0xDB}, "Google",     "Google Device"},
  {{0x3C, 0x28, 0x6D}, "Google",     "Google Device"},
  {{0xA4, 0x77, 0x33}, "Google",     "Chromecast"},
  //amazon
  {{0xFC, 0x65, 0xDE}, "Amazon",     "Echo/FireTV"},
  {{0x44, 0x65, 0x0D}, "Amazon",     "Echo/FireTV"},
  {{0xA0, 0x02, 0xDC}, "Amazon",     "Echo/FireTV"},
  //Raspberry Pi
  {{0xB8, 0x27, 0xEB}, "RPi Found.", "Raspberry Pi"},
  {{0xDC, 0xA6, 0x32}, "RPi Found.", "Raspberry Pi"},
  {{0xE4, 0x5F, 0x01}, "RPi Found.", "Raspberry Pi"},
  // Espressif (ESP32/ESP8266)
  {{0x24, 0x6F, 0x28}, "Espressif",  "ESP Device"},
  {{0x30, 0xAE, 0xA4}, "Espressif",  "ESP Device"},
  {{0xA0, 0x20, 0xA6}, "Espressif",  "ESP Device"},
  // Nvidia (Shield)
  {{0x00, 0x04, 0x4B}, "Nvidia",     "Shield/GPU"},
  // Sony
  {{0x28, 0xFD, 0xEB}, "Sony",       "Sony Device"},
  {{0xAC, 0x9B, 0x0A}, "Sony",       "PlayStation"},
  {{0x70, 0x9E, 0x29}, "Sony",       "PlayStation"},
  // Microsoft
  {{0x00, 0x50, 0xF2}, "Microsoft",  "Windows PC"},
  {{0x28, 0x18, 0x78}, "Microsoft",  "Xbox"},
  {{0x98, 0x5F, 0xD3}, "Microsoft",  "Xbox"},
  // Ubiquiti
  {{0x24, 0xA4, 0x3C}, "Ubiquiti",   "AP/Router"},
  {{0x78, 0x8A, 0x20}, "Ubiquiti",   "AP/Router"},
  // Synology
  {{0x00, 0x11, 0x32}, "Synology",   "NAS"},
  // Canon
  {{0x00, 0x1E, 0x8F}, "Canon",      "Printer"},
  {{0xA4, 0x83, 0xE7}, "Canon",      "Printer"},
  // HP
  {{0x3C, 0xD9, 0x2B}, "HP",         "Printer/PC"},
  {{0xA0, 0xB3, 0xCC}, "HP",         "Printer/PC"},
};
const int ouiTableSize = sizeof(ouiTable) / sizeof(ouiTable[0]);

const char* lookupDeviceType(const char* mac) {
  //check for randomized/locally administered Mac first
  //second hex digit being 2,6,A,E means loacally admin
  uint8_t firstByte = strtol(mac, nullptr, 16);
  if (firstByte & 0x02) {
    return "Random MAC";
  }

  uint8_t oui[3];
  sscanf(mac, "%hhx:%hhx:%hhx", &oui[0], &oui[1], &oui[2]);

  for (int i = 0; i < ouiTableSize; i++) {
    if (oui[0] == ouiTable[i].oui[0] &&
        oui[1] == ouiTable[i].oui[1] &&
        oui[2] == ouiTable[i].oui[2]) {
          return ouiTable[i].type;
        }
  }
  return "Unkown";
}

//work on changing Hostname to open ports 

void listDevices() {
  if (deviceCount == 0) {
    Serial.println(">> no Devices yet -- run the SCAN command pussy");
    return;
  }

  Serial.println();
  Serial.println("  +===================+==================+=============================+===========+==========+");
  Serial.println("  || MAC              || IP              || OPEN PORTS                || Device    || Source  |");
  Serial.println("  +===================+==================+=============================+===========+==========+");

  for (int i = 0; i < deviceCount; i++) {
    const char* devType = lookupDeviceType(devices[i].mac);

    char line[160];
    snprintf(line, sizeof(line),
    "  || %-16s || %-15s || %-23s || %-8s || %-7s |",
    devices[i].mac,
    devices[i].ip.toString().c_str(),
    devices[i].ports,
    devType,
    devices[i].source
    );
    Serial.println(line);
  }
   Serial.println("  +===================+==================+==========================+===========+==========+");
   Serial.print(" Total: ");
   Serial.println(deviceCount);
}

//so after being a monkey smacking a rock on a rock i finally got it to scan the devices on the network but
// the hostname is still not showing and lets make the RSSI in the list to actually be VPN
//to see if the device is using a VPN

void setup() { // set up the blinking crap
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("===================================================");
  Serial.println("                    CORNTENNA                   ");
  Serial.println("===================================================");

  WiFi.mode(WIFI_STA);
  setupState = AWAIT_NET_CHOICE;
  scanNetwork();

  pinMode(PIN_LED_BLUE, OUTPUT);
  pinMode(PIN_LED_RED,  OUTPUT);
  digitalWrite(PIN_LED_BLUE, LOW);
  digitalWrite(PIN_LED_RED, LOW);

  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CSN);
  if (!radio.begin()) {
    Serial.println(">> ERROR: nRF24L01+ not detected! check wiring.");
  } else { 
    radio.setPALevel(RF24_PA_LOW);
    radio.setDataRate(RF24_250KBPS);
    radio.setChannel(RF_CHANNEL);
    radio.setPayloadSize(sizeof(DevicePayload));
    radio.openWritingPipe(ADDRESS);
    radio.stopListening();
    Serial.println(">> nRF24L01+ OK");
  }
}

void loop() {
  updateBlinker(blueBlink);
  updateBlinker(redBlink);

  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        processCommand(inputBuffer);
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
    }
  }
}
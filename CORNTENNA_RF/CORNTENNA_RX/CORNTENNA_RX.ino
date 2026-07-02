//==CORNTENNA_RX===========
#include <SPI.h>
#include <RF24.h>

// ── Pin definitions ─────────────────────────────────
#define PIN_CE        4
#define PIN_CSN       5
#define PIN_SCK       18
#define PIN_MOSI      23
#define PIN_MISO      19
#define PIN_LED_BLUE  16
#define PIN_LED_RED   17

// ── Pairing / security settings ──────────────────────
// These three values must be IDENTICAL on the TX unit.
#define AUTH_KEY      0x4E
#define RF_CHANNEL    118
const byte ADDRESS[6] = "C0RN1";

// ── RF24 setup ──────────────────────────────────────
RF24 radio(PIN_CE, PIN_CSN);

// ── Payload struct 
struct DevicePayload {
  uint8_t  ip[4];
  uint8_t  mac[6];
  uint8_t  portMask;
  uint8_t  authKey;
};

// ── LED helpers ──────────────────────────────────────
struct Blinker {
  uint8_t  pin;
  bool     active;
  int      blinksLeft;
  int      totalBlinks;
  uint32_t lastToggle;
  uint16_t interval;
  bool     ledState;
};

// ── Known scan ports, in the same order as the TX scanner ───
const uint16_t PORT_LIST[] = {21, 22, 23, 80, 443, 445, 554, 8080, 8443, 9100};
const int NUM_PORTS = 10;

// ── Vendor 
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

const char* lookupVendor(const uint8_t* mac) {
  if (mac[0] & 0x02) return "Random";

  for (int i = 0; i < ouiTableSize; i++) {
    if (mac[0] == ouiTable[i].oui[0] &&
        mac[1] == ouiTable[i].oui[1] &&
        mac[2] == ouiTable[i].oui[2]) {
      return ouiTable[i].vendor;
    }
  }
  return "Unknown";
}

// ── State ────────────────────────────────────────────
int      totalReceived  = 0;
int      droppedPackets = 0;
bool     headerPrinted  = false;


Blinker blueBlink = {PIN_LED_BLUE, false, 0, 0, 0, 50, false};
Blinker redBlink  = {PIN_LED_RED,  false, 0, 0, 0, 40, false};

void startBlink(Blinker& b, int count, uint16_t intervalMs) {
  b.active      = true;
  b.blinksLeft  = count * 1;   // each blink = on + off
  b.totalBlinks = count * 1;
  b.interval    = intervalMs;
  b.lastToggle  = millis();
  b.ledState    = true;
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

// ── Formatting helpers ───────────────────────────────
void formatIP(const uint8_t* ip, char* out) {
  snprintf(out, 16, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}

void formatMAC(const uint8_t* mac, char* out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Unpack the port bitmask back into a readable CSV string e.g. "80,443,9100"
void formatPorts(uint8_t mask, char* out, size_t outSize) {
  out[0] = '\0';
  if (mask == 0) {
    strncpy(out, "none", outSize - 1);
    out[outSize - 1] = '\0';
    return;
  }
  bool first = true;
  for (int i = 0; i < NUM_PORTS; i++) {
    if (mask & (1 << i)) {
      char portStr[8];
      snprintf(portStr, sizeof(portStr), "%d", PORT_LIST[i]);
      if (!first) strncat(out, ",", outSize - strlen(out) - 1);
      strncat(out, portStr, outSize - strlen(out) - 1);
      first = false;
    }
  }
}

// ── Print table header ───────────────────────────────
void printHeader() {
  Serial.println();
  Serial.println(" +===================+==================+=================+===========+");
  Serial.println(" ||      MAC         ||       IP        ||   OPEN PORTS   || Vendor   |");
  Serial.println(" +===================+==================+=================+===========+");
  headerPrinted = true;
}

// ── Print one device row ─────────────────────────────
void printDevice(const DevicePayload& p) {
  char ipStr[16];
  char macStr[18];
  char portsStr[32];

  formatIP(p.ip, ipStr);
  formatMAC(p.mac, macStr);
  formatPorts(p.portMask, portsStr, sizeof(portsStr));

  const char* vendor = lookupVendor(p.mac);

  char line[140];
  snprintf(line, sizeof(line),
           " || %-16s || %-15s || %-15s || %-8s |", macStr, ipStr, portsStr, vendor);
  Serial.println(line);
}

// ── Print footer ───────────────────
void printFooter() {
  Serial.println(" +===================+==================+=================+===========+");
  Serial.print  (" Total received: ");
  Serial.println(totalReceived);
  Serial.println();
}

// ── Handle incoming payload ───────
void handlePayload(const DevicePayload& p) {
  if (p.authKey != AUTH_KEY) {
    droppedPackets++;
    return;
  }

  bool isFlush = (p.ip[0] == 0 && p.ip[1] == 0 && p.ip[2] == 0 && p.ip[3] == 0);

  if (isFlush) {
    if (headerPrinted) {
      printFooter();
      headerPrinted = false;
    }
    Serial.println(">> TX scan complete. Waiting for next transmission...");
    Serial.println(">> Blue = listening  |  Red = device received");
    return;
  }

  totalReceived++;

  if (!headerPrinted) {
    Serial.println("\n>> Incoming recon data from TX unit:");
    printHeader();
  }

  printDevice(p);

  
  startBlink(redBlink, 2, 25);

  
  startBlink(blueBlink, 1, 10);
}

// ── Setup ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(PIN_LED_BLUE, OUTPUT);
  pinMode(PIN_LED_RED,  OUTPUT);
  digitalWrite(PIN_LED_BLUE, LOW);
  digitalWrite(PIN_LED_RED,  LOW);

  Serial.println();
  Serial.println("===================================================");
  Serial.println("  CORNTENNA — RX UNIT");
  Serial.println("  Network Recon Receiver (paired)");
  Serial.println("===================================================");

  
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CSN);

  if (!radio.begin()) {
    Serial.println(">> ERROR: nRF24L01+ not detected! Check wiring.");
    while (true) {
      digitalWrite(PIN_LED_RED, HIGH); delay(200);
      digitalWrite(PIN_LED_RED, LOW);  delay(200);
    }
  }

  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(RF_CHANNEL);
  radio.setPayloadSize(sizeof(DevicePayload));
  radio.openReadingPipe(1, ADDRESS);
  radio.startListening();

  Serial.println(">> nRF24L01+ OK");
  Serial.print  (">> Payload size: "); Serial.print(sizeof(DevicePayload)); Serial.println(" bytes");
  Serial.print  (">> Channel: "); Serial.println(RF_CHANNEL);
  Serial.println(">> Listening for paired TX unit...");
  Serial.println(">> Blue = listening  |  Red = device received");
  Serial.println();

  
  startBlink(blueBlink, 1, 10);
}

// ── Loop ──────────────────────────────────────────────
void loop() {
  // Update non-blocking LED blinkers
  updateBlinker(blueBlink);
  updateBlinker(redBlink);

  static uint32_t lastHeartbeat = 0;
  if (!blueBlink.active && !redBlink.active) {
    uint32_t now = millis();
    if (now - lastHeartbeat >= 3000) {
      lastHeartbeat = now;
      startBlink(blueBlink, 1, 10);   // single short pulse = still listening
    }
  }

  // Check for incoming RF data
  if (radio.available()) {
    DevicePayload payload;
    radio.read(&payload, sizeof(DevicePayload));
    handlePayload(payload);
  }
}
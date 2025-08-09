#include <Arduino.h>
#include "WiFi.h"
#include "esp_now.h"

#define ESPNOW_WIFI_CHANNEL 0
#define MAX_ESP_NOW_MAC_LEN 6
#define BAUD 115200
#define MAX_CHARACTERS_NUMBER 20
#define NO_PMK_KEY false

typedef uint8_t XIAO;
typedef int status;

// Sender MAC address
static uint8_t XIAOS3_Sender_MAC_Address[MAX_ESP_NOW_MAC_LEN] = {0xEC, 0x64, 0xC9, 0x6E, 0xCA, 0xC0};
esp_now_peer_info_t peerInfo_sender;

typedef struct {
  char Reveiver_device[MAX_CHARACTERS_NUMBER];
  char Reveiver_Trag[MAX_CHARACTERS_NUMBER];
} receiver_meesage_types;

typedef struct {
  char message[32];
} message_types;

receiver_meesage_types XIAOC6_RECEIVER_INFORATION;
message_types XIAOS3_SENDER_INFORATION;

// ✅ Relay Pin Assignments (Tested & Working GPIOs)
#define RELAY_BULB   0   // GPIO0
#define RELAY_FAN    1   // GPIO1
#define RELAY_BELL   2   // GPIO2
#define RELAY_LIGHT 18   // GPIO18
#define RELAY_LAMP  19   // GPIO19
#define RELAY_BUZZ  21   // GPIO21

void Receiver_MACAddress_requir();
void espnow_init();
void espnow_deinit();
void ReceiverXIAOC6_Recive_Data_cb(const esp_now_recv_info *info, const uint8_t *incomingData, int len);
void ReceiverXIAOC6_Send_Data_cb(const wifi_tx_info_t *info, esp_now_send_status_t status);
void Association_SenderXIAOS3_peer();

void setup() {
  Serial.begin(BAUD);
  while (!Serial);

  Receiver_MACAddress_requir();
  espnow_init();

  esp_now_register_recv_cb(ReceiverXIAOC6_Recive_Data_cb);
  esp_now_register_send_cb(ReceiverXIAOC6_Send_Data_cb);
  Association_SenderXIAOS3_peer();

  // ✅ Set all relay pins as OUTPUT and initially OFF
  pinMode(RELAY_BULB, OUTPUT);
  pinMode(RELAY_FAN, OUTPUT);
  pinMode(RELAY_BELL, OUTPUT);
  pinMode(RELAY_LIGHT, OUTPUT);
  pinMode(RELAY_LAMP, OUTPUT);
  pinMode(RELAY_BUZZ, OUTPUT);

  digitalWrite(RELAY_BULB, LOW);
  digitalWrite(RELAY_FAN, LOW);
  digitalWrite(RELAY_BELL, LOW);
  digitalWrite(RELAY_LIGHT, LOW);
  digitalWrite(RELAY_LAMP, LOW);
  digitalWrite(RELAY_BUZZ, LOW);

  // ✅ Test each relay once
  Serial.println("Relay pin test...");
  digitalWrite(RELAY_BULB, HIGH);  delay(200); digitalWrite(RELAY_BULB, LOW);
  digitalWrite(RELAY_FAN, HIGH);   delay(200); digitalWrite(RELAY_FAN, LOW);
  digitalWrite(RELAY_BELL, HIGH);  delay(200); digitalWrite(RELAY_BELL, LOW);
  digitalWrite(RELAY_LIGHT, HIGH); delay(200); digitalWrite(RELAY_LIGHT, LOW);
  digitalWrite(RELAY_LAMP, HIGH);  delay(200); digitalWrite(RELAY_LAMP, LOW);
  digitalWrite(RELAY_BUZZ, HIGH);  delay(200); digitalWrite(RELAY_BUZZ, LOW);
}

void loop() {
  // Nothing to do here
}

void espnow_init() {
  status espnow_sign = esp_now_init();
  Serial.println(espnow_sign == ESP_OK ? "ESP-NOW initialized successfully!" : "ESP-NOW initialization failed!");
}

void espnow_deinit() {
  status espnow_sign = esp_now_deinit();
  Serial.println(espnow_sign == ESP_OK ? "ESP-NOW deinitialized successfully!" : "ESP-NOW deinitialization failed!");
}

void Receiver_MACAddress_requir() {
  WiFi.mode(WIFI_STA);
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);

  XIAO mac[MAX_ESP_NOW_MAC_LEN];
  while (!WiFi.STA.started()) {
    Serial.print(".");
    delay(100);
  }

  WiFi.macAddress(mac);
  Serial.println();
  Serial.printf("const uint8_t mac_self[6] = {0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x};\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void ReceiverXIAOC6_Recive_Data_cb(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  memcpy(&XIAOS3_SENDER_INFORATION, incomingData, sizeof(XIAOS3_SENDER_INFORATION));
  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.print("Received message: ");
  Serial.println(XIAOS3_SENDER_INFORATION.message);

  String command = String(XIAOS3_SENDER_INFORATION.message);
  command.trim();
  Serial.print("Executing command: ");
  Serial.println(command);

  // ✅ Relay logic
  if (command == "BULB:ON")        digitalWrite(RELAY_BULB, HIGH);
  else if (command == "BULB:OFF")  digitalWrite(RELAY_BULB, LOW);
  else if (command == "FAN:ON")    digitalWrite(RELAY_FAN, HIGH);
  else if (command == "FAN:OFF")   digitalWrite(RELAY_FAN, LOW);
  else if (command == "BELL:ON")   digitalWrite(RELAY_BELL, HIGH);
  else if (command == "BELL:OFF")  digitalWrite(RELAY_BELL, LOW);
  else if (command == "LIGHT:ON")  digitalWrite(RELAY_LIGHT, HIGH);
  else if (command == "LIGHT:OFF") digitalWrite(RELAY_LIGHT, LOW);
  else if (command == "LAMP:ON")   digitalWrite(RELAY_LAMP, HIGH);
  else if (command == "LAMP:OFF")  digitalWrite(RELAY_LAMP, LOW);
  else if (command == "BUZZ:ON")   digitalWrite(RELAY_BUZZ, HIGH);
  else if (command == "BUZZ:OFF")  digitalWrite(RELAY_BUZZ, LOW);
}

void ReceiverXIAOC6_Send_Data_cb(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           info->des_addr[0], info->des_addr[1], info->des_addr[2],
           info->des_addr[3], info->des_addr[4], info->des_addr[5]);

  Serial.print("Packet to: ");
  Serial.println(macStr);
  delay(500);
  Serial.print("Send status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  Serial.println();
}

void Association_SenderXIAOS3_peer() {
  Serial.println("Attempting to associate peer for XIAOC6...");
  peerInfo_sender.channel = ESPNOW_WIFI_CHANNEL;
  peerInfo_sender.encrypt = NO_PMK_KEY;

  memcpy(peerInfo_sender.peer_addr, XIAOS3_Sender_MAC_Address, MAX_ESP_NOW_MAC_LEN);
  esp_err_t addPressStatus = esp_now_add_peer(&peerInfo_sender);

  if (addPressStatus != ESP_OK) {
    Serial.print("Failed to add peer: ");
    Serial.println(addPressStatus);
  } else {
    Serial.println("Peer added successfully");
  }
}

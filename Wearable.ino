 #include <esp_now.h>
#include <WiFi.h>
#include <AccelAndGyro.h>
#include <LightProximityAndGesture.h>
#include <Wire.h>
#include <oled.h>
#include "BluetoothSerial.h"

BluetoothSerial BT;
AccelAndGyro Ag;
LightProximityAndGesture Lpg;
oLed display(SCREEN_WIDTH, SCREEN_HEIGHT);

/* Sensor Data */
float AngleRoll = 0, AnglePitch = 0;
float RefAngleRoll = 0, RefAnglePitch = 0;
float filteredRoll = 0, filteredPitch = 0;

/* Device control */
int currentDevice = 0;
bool deviceSelected = false;
unsigned long lastSwitchTime = 0;
const int SWITCH_DEBOUNCE = 500;  
unsigned long lastMovement = 0;  

/* Thresholds */
const float ANGLE_THRESHOLD = 30.0;
const float TWIST_THRESHOLD = 25.0;
const float HYSTERESIS = 5.0;
const float NEUTRAL_ZONE = 15.0;
const float DEADBAND = 5.0;  
const unsigned long MIN_TWIST_TIME = 300; // Minimum time to register a twist
const int STABLE_COUNT_REQUIRED = 5;
int stableCount = 0;

/* Twist State Machine */
enum TwistState { NEUTRAL, TWISTING_LEFT, TWISTING_RIGHT };
TwistState currentTwistState = NEUTRAL;
unsigned long twistStartTime = 0;
bool twistRegistered = false;

/* Feedback Pins */
const int BUZZER_PIN = 5;
const int LED_PIN = 2;

/* Device names */
const char* deviceNames[] = { "BULB", "BELL", "FAN", "LAMP", "TV", "CALL" };
const int NUM_DEVICES = sizeof(deviceNames) / sizeof(deviceNames[0]);
bool deviceOn[NUM_DEVICES] = {false};

/* Contacts */
struct Contact {
  const char* name;
  const char* number;
};

Contact contacts[] = {
  {"MOM", "9876543210"},
  {"DAD", "9123456780"},
  {"FRIEND", "9988776655"}
};
const int NUM_CONTACTS = sizeof(contacts) / sizeof(contacts[0]);
int currentContact = 0;
bool callMode = false;

/* Gesture types */
enum Gesture {
  NONE,
  SWIPE_UP,
  SWIPE_DOWN,
  SWIPE_LEFT,
  SWIPE_RIGHT
};

/* ESP-NOW Communication */
typedef struct struct_message {
  char message[32];
} struct_message;

struct_message outgoingMessage;
uint8_t receiverMAC[] = {0xE4, 0xB3, 0x23, 0xB5, 0x6F, 0x30};

// Function prototypes
void OnDataSent(const wifi_tx_info_t *wifi_info, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void sendBluetoothMessage(const char* message) {
  if (BT.hasClient()) {
    BT.println(message);
    Serial.print(" BT Sent: ");
    Serial.println(message);
  } else {
    Serial.println(" No Bluetooth client connected");
  }
}

void sendCommandESPNow(const char* command) {
  strncpy(outgoingMessage.message, command, sizeof(outgoingMessage.message));
  esp_err_t result = esp_now_send(receiverMAC, (uint8_t *) &outgoingMessage, sizeof(outgoingMessage));

  if (result == ESP_OK) {
    Serial.print("Command sent: ");
    Serial.println(command);
  } else {
    Serial.println("Send Error");
  }
}

void calibrateReferenceAngles();
void readAngles();
void checkGestures();
void giveFeedback();
void updateOLED(String message);
Gesture readGesture();

void autoRecalibrate() {
  if (abs(AngleRoll - RefAngleRoll) < 5.0 && abs(AnglePitch - RefAnglePitch) < 5.0) {
    if (millis() - lastMovement > 2000) { // 2 seconds of stillness
      RefAngleRoll = AngleRoll;
      RefAnglePitch = AnglePitch;
    }
  } else {
    lastMovement = millis();
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  // Initialize OLED
  if (!display.begin()) {
    Serial.println(" OLED init failed");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(" OLED Ready");
    display.display();
    delay(1000);
  }

  // Initialize Gyro
  while (!Ag.begin()) {
    Serial.println(" MPU6050 not found...");
    updateOLED("MPU6050 not found");
    delay(500);
  }
  Serial.println(" MPU6050 connected!");
  updateOLED("MPU6050 connected");
  delay(1000);

  // Initialize Gesture Sensor
  while (!Lpg.begin()) {
    Serial.println(" APDS9960 not found...");
    updateOLED("APDS9960 not found");
    delay(500);
  }
  Serial.println(" APDS9960 connected!");
  if (Lpg.enableGestureSensor(DISABLE)) {
    Serial.println("Gesture sensor running");
  } else {
    Serial.println("Gesture sensor init failed!");
  }

  // Bluetooth Initialization
  BT.begin("Wearable-Controller");
  Serial.println(" Bluetooth Started - Pair with your phone");
  updateOLED("Bluetooth Ready\nPair Phone");
  delay(2000);
  
  Serial.println(" Calibrating...");
  updateOLED("Hold Still\nCalibrating...");
  delay(3000);
  calibrateReferenceAngles();
  Serial.println(" Calibration complete!");
  updateOLED("Calibration Done\nTwist to Select\nSwipe Left to Exit");

  // ESP-NOW Initialization
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

void calibrateReferenceAngles() {
  float rollSum = 0, pitchSum = 0;
  const int samples = 50;

  for (int i = 0; i < samples; i++) {
    readAngles();
    rollSum += AngleRoll;
    pitchSum += AnglePitch;
    delay(20);
  }

  RefAngleRoll = rollSum / samples;
  RefAnglePitch = pitchSum / samples;
}

void readAngles() {
  if (Ag.ping()) {
    float accX = Ag.getAccelX(false);
    float accY = Ag.getAccelY(false);
    float accZ = Ag.getAccelZ(false);

    // Apply complementary filter for better stability
    static float lastFilteredRoll = 0, lastFilteredPitch = 0;
    const float ALPHA = 0.2;  // Smoothing factor
    
    float rawRoll = atan2(accY, sqrt(accX * accX + accZ * accZ)) * 180.0 / PI;
    float rawPitch = -atan2(accX, sqrt(accY * accY + accZ * accZ)) * 180.0 / PI;
    
    // Apply low-pass filter
    filteredRoll = ALPHA * rawRoll + (1 - ALPHA) * lastFilteredRoll;
    filteredPitch = ALPHA * rawPitch + (1 - ALPHA) * lastFilteredPitch;
    
    lastFilteredRoll = filteredRoll;
    lastFilteredPitch = filteredPitch;
    
    AngleRoll = filteredRoll;
    AnglePitch = filteredPitch;
  }
}
Gesture readGesture() {
  if (Lpg.ping()) {
    String gestureStr = String(Lpg.getGesture());
    
    if (gestureStr == "UP") return SWIPE_UP;
    if (gestureStr == "DOWN") return SWIPE_DOWN;
    if (gestureStr == "LEFT") return SWIPE_LEFT;
    if (gestureStr == "RIGHT") return SWIPE_RIGHT;
  }
  return NONE;
}

void checkGestures() {
  float rollDiff = AngleRoll - RefAngleRoll;
  float pitchDiff = AnglePitch - RefAnglePitch;
  unsigned long now = millis();

  // Improved twist detection with state machine
  switch (currentTwistState) {
    case NEUTRAL:
      if (rollDiff > TWIST_THRESHOLD + HYSTERESIS) {
        currentTwistState = TWISTING_RIGHT;
        twistStartTime = now;
        twistRegistered = false;
      } else if (rollDiff < -TWIST_THRESHOLD - HYSTERESIS) {
        currentTwistState = TWISTING_LEFT;
        twistStartTime = now;
        twistRegistered = false;
      }
      break;
      
    case TWISTING_RIGHT:
      if (rollDiff < TWIST_THRESHOLD - HYSTERESIS) {
        currentTwistState = NEUTRAL;
      } else if (!twistRegistered && (now - twistStartTime >= MIN_TWIST_TIME)) {
        // Register right twist
        if (!deviceSelected && (now - lastSwitchTime > SWITCH_DEBOUNCE)) {
          currentDevice = (currentDevice + 1) % NUM_DEVICES;
          Serial.print("  Twist Right → ");
          Serial.print("Option Selected: ");
          Serial.println(deviceNames[currentDevice]);
          updateOLED(String("Selected: ") + deviceNames[currentDevice]);
          deviceSelected = true;
          giveFeedback();
          lastSwitchTime = now;
        }
        twistRegistered = true;
      }
      break;
      
    case TWISTING_LEFT:
      if (rollDiff > -TWIST_THRESHOLD + HYSTERESIS) {
        currentTwistState = NEUTRAL;
      } else if (!twistRegistered && (now - twistStartTime >= MIN_TWIST_TIME)) {
        // Register left twist
        if (!deviceSelected && (now - lastSwitchTime > SWITCH_DEBOUNCE)) {
          currentDevice = (currentDevice - 1 + NUM_DEVICES) % NUM_DEVICES;
          Serial.print(" Twist Left → ");
          Serial.print("Option Selected: ");
          Serial.println(deviceNames[currentDevice]);
          updateOLED(String("Selected: ") + deviceNames[currentDevice]);
          deviceSelected = true;
          giveFeedback();
          lastSwitchTime = now;
        }
        twistRegistered = true;
      }
      break;
  }


  // Gesture sensor handling when device is selected
  if (deviceSelected) {
    Gesture gesture = readGesture();

    // Call mode handling
    if (strcmp(deviceNames[currentDevice], "CALL") == 0) {
      callMode = true;
      if (gesture != NONE) {
        String callCmd;  // Moved declaration to avoid cross initialization
        
        switch (gesture) {
          case SWIPE_UP:
            currentContact = (currentContact - 1 + NUM_CONTACTS) % NUM_CONTACTS;
            Serial.print("Swipe Up → Contact: ");
            Serial.println(contacts[currentContact].name);
            updateOLED(String(contacts[currentContact].name) + "\n" + contacts[currentContact].number);
            giveFeedback();
            break;

          case SWIPE_DOWN:
            currentContact = (currentContact + 1) % NUM_CONTACTS;
            Serial.print(" Swipe Down → Contact: ");
            Serial.println(contacts[currentContact].name);
            updateOLED(String(contacts[currentContact].name) + "\n" + contacts[currentContact].number);
            giveFeedback();
            break;

          case SWIPE_LEFT:
            Serial.print(" Calling → ");
            Serial.println(contacts[currentContact].name);
            updateOLED("Calling:\n" + String(contacts[currentContact].name));
            callCmd = String("CALL:") + contacts[currentContact].number;
            sendBluetoothMessage((String("CALL:") + contacts[currentContact].number).c_str());

            giveFeedback();
            break;

          case SWIPE_RIGHT:
            callMode = false;
            deviceSelected = false;
            Serial.println("⬅️ Exit Call Mode");
            updateOLED("Exited Call Mode");
            giveFeedback();
            break;

          case NONE:
            break;
        }
        delay(300); // Gesture debounce
      }
      return;
    }

    // Normal device control mode
    switch (gesture) {
      case SWIPE_DOWN:
        deviceOn[currentDevice] = true;
        Serial.print("🔼 Swipe Up → ");
        Serial.print(deviceNames[currentDevice]);
        Serial.println(" ON");
        updateOLED(String(deviceNames[currentDevice]) + "\nON");
        giveFeedback();
        break;

      case SWIPE_UP:
        deviceOn[currentDevice] = false;
        Serial.print("🔽 Swipe Down → ");
        Serial.print(deviceNames[currentDevice]);
        Serial.println(" OFF");
        updateOLED(String(deviceNames[currentDevice]) + "\nOFF");
        giveFeedback();
        break;

      case SWIPE_RIGHT:
        deviceSelected = false;
        Serial.println("⬅️ Swipe Left → Exit");
        updateOLED("Exited to Home");
        giveFeedback();
        break;

      case SWIPE_LEFT:
        Serial.print("📤 Swipe Right → ");
        Serial.print(deviceNames[currentDevice]);
        Serial.println(" Command");
        updateOLED(String("Sent to ") + deviceNames[currentDevice]);
        giveFeedback();

        if (deviceOn[currentDevice]) {
          String cmd = String(deviceNames[currentDevice]) + ":ON";
          sendCommandESPNow(cmd.c_str());
        } else {
          String cmd = String(deviceNames[currentDevice]) + ":OFF";
          sendCommandESPNow(cmd.c_str());
        }
        break;

      case NONE:
        break;
    }

    if (gesture != NONE) {
      delay(300);
    }
  }
}



void giveFeedback() {
  digitalWrite(BUZZER_PIN, HIGH);
  digitalWrite(LED_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
}

void updateOLED(String message) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  
  // Add twist state indicator
  display.setTextSize(1);
  display.setCursor(90, 0);
  switch(currentTwistState) {
    case NEUTRAL: display.print("|"); break;
    case TWISTING_LEFT: display.print("←"); break;
    case TWISTING_RIGHT: display.print("→"); break;
  }
  
  display.setTextSize(2);
  display.setCursor(0, 10);
  display.println(message);
  display.display();
}

void loop() {
  readAngles();
  checkGestures();
  autoRecalibrate();
  delay(20);
}

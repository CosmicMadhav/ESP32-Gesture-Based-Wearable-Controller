
# Gesture Powered Signage

A wearable control system that empowers patients, elderly individuals, and people with limited mobility or speech to control their surroundings and call for help — using only hand gestures.

## 📌 Features

* **Gesture Control:** Twist, swipe, and tilt gestures to select and control devices.
* **Offline Operation:** Works without internet, mobile apps, or voice commands.
* **Emergency Calling:** Make global calls to predefined contacts using gestures alone — no extra hardware required.
* **Multi-Device Control:** Integrated with Seeed Studio’s 6-Channel Relay Board for switching lights, fans, bells, and more.
* **Fast & Reliable:** Uses ESP-NOW protocol for instant communication between wearable and receiver.

## 🛠 Hardware Used

1. **MYOSA ESP32 Wroom Board** – Wearable gesture detection
2. **Seeed Studio XIAO ESP32-C6** – Relay controller
3. **Seeed Studio 6-Channel Relay Board** – Multi-appliance switching
4. **APDS9960 Gesture Sensor** – Gesture recognition (twist, swipe, tilt)
5. **OLED Display** – Device selection feedback
6. Relays, wiring, and supporting components

## 🔌 How It Works

1. Wearable detects gestures via APDS9960 and MPU6050 sensors.
2. Commands are transmitted via **ESP-NOW** to the receiver node.
3. Receiver activates the corresponding relay channel to control appliances.
4. Emergency gesture triggers a **Tasker-based phone call** without unlocking the phone.


## 🚀 Getting Started

### Prerequisites
* Arduino IDE or PlatformIO installed
* ESP32 board package installed in Arduino IDE
* Basic knowledge of ESP-NOW communication
* Basic Use Of Tasker Application
### Installation

1. Clone the repository:

```bash
git clone https://github.com/CosmicMadhav/ESP32-Gesture-Based-Wearable-Controller.git
```

2. Open `Wearable_code.ino` in Arduino IDE, select **ESP32 Wroom board**, and upload.
3. Open `Receiver_relay_code.ino`, select **XIAO ESP32-C6**, and upload.

### Connections
* Connect gesture sensor to ESP32 as per `wearable_code` pin definitions.
* Connect appliances to the relay board as per wiring diagram.


## 📜 License
This project is open-source and available under the MIT License.

## 🙌 Acknowledgements
* Seeed Studio for hardware
* MYOSA Kit team for development platform

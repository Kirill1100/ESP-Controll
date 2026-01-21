# ESP-Controll 

**ESP-Controll** is an embedded project based on the **ESP32-C3**, featuring an **OLED display** and **4 physical buttons** for navigation and control.  
Designed for learning, experimentation, and portable embedded interfaces.

## 📦 Hardware Requirements

To build this project you will need:

- **ESP32-C3**
- **OLED Display 128×64 (I²C)**
- **4 Push Buttons**
- **3.3V Battery** *(optional)*
- **Power Switch** *(optional)*

##  Pin Configuration

### OLED Display → ESP32-C3

| OLED Pin | ESP32-C3 Pin |
|--------|--------------|
| GND    | GND          |
| VCC    | VCC (3.3V)   |
| SCL    | GPIO 4       |
| SDA    | GPIO 3       |

### Buttons → ESP32-C3

| Button     | ESP32-C3 Pin |
|-----------|--------------|
| BTN_UP    | GPIO 6       |
| BTN_DOWN | GPIO 5       |
| BTN_OK   | GPIO 7       |
| BTN_BACK | GPIO 8       |

## ⚙️ Features

- OLED menu interface
- Button-based navigation
- Compact and portable design
- Battery-powered (optional)
- Ideal for custom controllers and small UIs

##  Setup & Installation

1. Connect all components following the pin table above.
2. Install the **ESP32 board package** in the Arduino IDE.
3. Install required libraries:
   - `Adafruit_GFX`
   - `Adafruit_SSD1306`
4. Upload the code to your ESP32-C3.
5. Power the board via USB or battery.

##  Preview

*(Add photos or screenshots of the project here)*

##  Future Improvements

- Add more menu pages
- Power management (deep sleep)
- Bluetooth / Wi-Fi control
- Custom icons and animations

##  Author

**KyrexX**  
ESP32-C3 Embedded Project

## 📄 License

This project is open-source.  
You are free to use, modify, and distribute it.

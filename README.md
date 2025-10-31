# ☕ Jura ESPHome Components

**Control and monitor your Jura Coffee Machine and CoolControl milk cooler directly from ESPHome and Home Assistant.**

> ⚠️ *Use at your own risk.*  
> This project interfaces with hardware not meant for third-party control. While it’s been tested successfully on several Jura models (notably the **E8**), you are responsible for any damage or warranty issues.

---

## 🌟 Overview

This project builds on the fantastic work by [Ryan Alden’s original Jura component](https://github.com/ryanalden/esphome-jura-component), modernizing it for **ESPHome’s new external component architecture** and ensuring **cross-platform compatibility** (ESP8266 & ESP32).

### 🧰 Key Improvements

- ✅ Migrated from `custom_component` → ESPHome’s **`external_component`** system  
- ✅ Replaced Arduino `String` with C++ `std::string` → **works on ESP32 IDF**  
- ✅ Fully self-contained component (no external sensors required)  
- ✅ Updated **bit-flags** for the Jura E8 (your model may differ)  
- ✅ Added **Jura CoolControl** support (milk cooler integration)  
- ✅ Clear debug logging for bit-flag discovery and machine diagnostics  

---

## ⚡ Hardware & Wiring

> **⚠️ Warning:** Incorrect wiring can permanently damage your Jura machine or your ESP device.  
> ESP pins are **not 5 V tolerant**. Always use a level-shifter if unsure.
<img width="468" height="324" alt="image" src="https://github.com/user-attachments/assets/6f2bb48f-e853-409c-b768-2b08b87c70d2" />


| Pin | Description | Notes |
|-----|--------------|-------|
| TX  | ESP → Jura   | Send commands |
| RX  | Jura → ESP   | Receive data |
| GND | Common Ground | Shared reference |

### Example connection (ESP32-C3)

```yaml
uart:
  id: uart_bus
  tx_pin: GPIO21
  rx_pin: GPIO20
  baud_rate: 9600
```

<details>
<summary>⚙️ Practical note about voltage tolerance</summary>

Officially, ESP devices are **not 5 V tolerant**.  
Unofficially—many of us have connected 5 V UARTs to ESP boards without immediate issues.  
Proceed at your own risk: your luck, your device, your coffee. ☕😅  
</details>

---

## 🧩 ESPHome Configuration

Example YAML:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/tiaanv/jura
    components: [ jura, jura_coolcontrol ] #You only need one!

uart:
  id: uart_bus
  tx_pin: GPIO21
  rx_pin: GPIO20
  baud_rate: 9600

#Only define one! One ESP. One Machine
#Use this one for coffee machines
jura:
  id: jura_main
  uart_id: uart_bus

#Use this one for the Milk Cooler!
jura_coolcontrol:
  id: jura_cool
  uart_id: uart_bus
```

---

## ☕ Jura Component

The **Jura component** polls the machine every **2 seconds**, sending two serial commands and decoding the responses.

### Home Assistant Entities

| Sensor | Description |
|---------|--------------|
| `Single Espresso Made` | Total count |
| `Double Espresso Made` | Total count |
| `Coffee Made` | Total count |
| `Double Coffee Made` | Total count |
| `Cleanings Performed` | Total cycles |
| `Brews Performed` | Total count |
| `Grounds Capacity Remaining` | Remaining ground cycles |
| `Tray Status` | “Present” / “Missing” |
| `Water Tank Status` | “OK” / “Fill Tank” |
| `Machine Status` | Ready, Busy, Tray Missing, etc. |

### Example Dashboard

<img width="330" alt="Jura Dashboard" src="https://github.com/user-attachments/assets/8fde2d3c-cc85-4a5d-ab0a-e84f5641cd6e" />

---

## 🥶 Jura CoolControl Component

Monitors the official Jura **CoolControl milk cooler**, which continuously broadcasts its data.

### Exposed Sensors

| Sensor | Unit | Range | Description |
|--------|-------|--------|-------------|
| Level | % | 0–100 | Milk level |
| Temperature | °C | 0–50 | Cooler temperature |

<img width="325" alt="CoolControl Entities" src="https://github.com/user-attachments/assets/f9654b9d-b26e-46c5-b7aa-83a001afc28c" />

---

## 🧠 Advanced Info

- Communication uses a **custom 2-bit serial protocol** over UART.  
- Each message is sent as encoded bytes with timing delays (`8 ms` between chars).  
- The machine echoes responses ending with `\r\n`.  
- Bit positions differ between models — use the debug logs to identify your own.

To debug:
```bash
ESP_LOGD("jura", "Raw IC result: %s", result.c_str());
```

---

## 🔧 Development Notes

- Tested on **ESP8266**, **ESP32-C1**, and **ESP32-S3**
- Compatible with **ESPHome ≥ 2024.4**
- Compiles cleanly on both **Arduino** and **ESP-IDF**

---

## ⚠️ Final Thoughts

> ☕ “Just because you *can*, doesn’t mean you *should*.”  
> This project is purely for educational tinkering.  
> Interfacing directly with commercial appliances **can be dangerous**.  
> Be cautious, monitor your device, and never leave it unattended.

---

## 💡 Contributions Welcome

Pull requests, improvements, or new flag maps for other Jura models are very welcome!  
Let’s make our coffee smarter — responsibly.

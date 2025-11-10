# Jura UART Register Map

This document provides an overview of the known **registers (counters)** returned by the `RT:0000` command
for various Jura coffee machine models.  
Each register corresponds to a 4-character hexadecimal field in the response payload (starting from offset `0x03`).

The goal of this table is to consolidate knowledge from community efforts
and to make it easy to correlate specific machine counters with their real-world meanings.

---

## 📊 Register / Counter Mapping

| Register | Offset | Description (if known) | F6 | F7 | E8 | ENA8 | UNKNOWN / Legacy |
|-----------|:------:|------------------------|:--:|:--:|:--:|:--:|:----------------:|
| counter_1 | 0x03 | *Shots / Single Espresso* | `Single Espresso Made` | `Single Espresso Made` | `Single Espresso Made` | – | – |
| counter_2 | 0x07 | *Double Espresso / Lungo* | `Double Espresso Made` | `Double Espresso Made` | `Double Espresso Made` | – | – |
| counter_3 | 0x0B | *Coffee / Americano* | `Coffee Made` | `Coffee Made` | `Coffee Made` | – | – |
| counter_4 | 0x0F | *Normally Double Coffee* | `Double Coffee Made` | `Double Coffee Made` | `Flat White Made` | – | – |
| counter_5 | 0x13 | *Ristretto/Cappuccino* | – | `Ristretto Made` | `Cappuccino Made` | – | – |
| counter_6 | 0x17 | *Cappuccino* | – | `Cappuccino Made` | – | – | – |
| counter_7 | 0x1B | *Double Ristretto* | – | `Double Ristretto Made` | – | – | – |
| counter_8 | 0x1F | *Brew Group Cycles/Rinses* | - | `Brew Movements Performed` | `Rinses Performed` | – | – |
| counter_9 | 0x23 | *Cleanings Performed* | `Cleanings Performed` | `Cleanings Performed` | `Cleanings Performed` | – | – |
| counter_10 | 0x27 | *Descalings Performed* | – | `Descalings Performed` | `Descalings Performed` | – | – |
| counter_11 | 0x2B | *Brew Group Cycles* | – | – | `Brew Movements Performed` | – | – |
| counter_12 | 0x2F | *Maybe something to do with Milk* | – | – | `*Milk Made*` | – | – |
| counter_13 | 0x33 | *Unknown* | – | – | – | – | – |
| counter_14 | 0x37 | *Brews since Cleaning* | – | – | `Brews Since Cleaning Performed` | – | – |
| counter_15 | 0x3B | *Grounds Bin Count* | `Grounds Level` | `Grounds Level` | `Grounds Level` | – | – |
| counter_16 | 0x3F | *Unknown / Reserved* | – | – | – | – | – |

> 💡 **Notes**
> - Offsets shown are relative to the ASCII payload starting at byte `3` of `RT:0000`.
> - Each field is a 4-digit hexadecimal value (`0000`–`FFFF`) representing an unsigned integer.


---

## 🧩 Other Registers (Non-Counter Commands)

| Command | Description | Notes |
|----------|--------------|-------|
| `IC:` | Input/Status Control Flags | Returns 2 bytes (`A` and `B`) with individual bit flags for tray, tank, and brew states. |
| `TT:` | Temperature Test (varies) | Unverified — possible thermal sensor readout. |
| `RU:` | Runtime Stats | May return usage hours or cycles (unknown structure). |

---

## 🔍 Future Work

- [ ] Identify mappings for `counter_5`–`counter_7`, `counter_10`–`counter_14`.
- [ ] Cross-reference with EEPROM layouts of legacy machines (Impressa, ENA Micro).
- [ ] Add derived metrics (e.g., total drinks made, lifetime brew count).
- [ ] Confirm meaning of non-counter registers for models like **ENA8**, **S8**, **Z10**.

---

## 🗂️ Index

- [Introduction](#jura-uart-register-map)
- [Register / Counter Mapping](#-register--counter-mapping)
- [Other Registers](#-other-registers-non-counter-commands)
- [Future Work](#-future-work)

---

> ⚙️ *This file is part of the [Jura ESPHome Integration](https://github.com/your-repo-here) project.*
> Contributions and verified offsets are welcome — please open a PR or issue to update the mapping.

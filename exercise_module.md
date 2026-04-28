# Exercise Tracking Module (Wearable + Teensy Integration)

## Overview

This module is a **battery-powered wearable device** that tracks exercises using motion data and user input, then sends results to a central **Teensy 4.1 hub** over Bluetooth.

It supports:
- Rep-based exercises (pushups, curls, squats)
- Time-based exercises (plank, dead hang)
- Manual exercise selection via rotary encoder
- Visual feedback via OLED display
- Audio feedback via buzzer

---

# System Architecture

[ Wearable Module ]
   ├─ IMU (motion sensing)
   ├─ Rotary Encoder (input)
   ├─ OLED Display (UI)
   ├─ Buzzer (feedback)
   ├─ BLE MCU (processing + transmission)
   └─ LiPo Battery

           ↓ Bluetooth (BLE)

[ Teensy 4.1 Hub ]
   ├─ BLE Receiver Module
   ├─ Display (main UI)
   └─ Data logging (SD card)

---

# Hardware Components

## 1. Microcontroller (Wearable Core)

- Seeed XIAO nRF52840
- Built-in BLE, low power, Arduino-compatible

## 2. IMU (Motion Sensor)

- BMI270 (recommended) or MPU6050 (starter)
- 3-axis accel + 3-axis gyro

## 3. Rotary Encoder

- EC11 encoder with push button
- Used for selecting exercise and confirming input

## 4. OLED Display

- 0.96" SSD1306 I2C OLED
- Displays UI, timers, reps

## 5. Buzzer

- Active buzzer (3.3V compatible)
- Used for countdown + completion alerts

## 6. Battery

- 3.7V LiPo (200–500 mAh)
- ~6–10 hours runtime

## 7. Teensy BLE Receiver

- HM-10 BLE module
- UART connection to Teensy

---

# Wiring Overview

## Wearable

I2C:
- SDA → OLED + IMU
- SCL → OLED + IMU

Encoder:
- A → GPIO
- B → GPIO
- Button → GPIO

Buzzer:
- Signal → GPIO

Power:
- LiPo → XIAO

## Teensy

- HM-10 TX → Teensy RX
- HM-10 RX → Teensy TX
- GND → GND
- VCC → 3.3V

---

# Software Design

## State Machine

SELECT_EXERCISE → SELECT_DURATION → READY_COUNTDOWN → ACTIVE_TIMER / REP_TRACKING → DONE

---

# User Flow

1. Rotate → select exercise
2. Click → confirm
3. (If timed) rotate → select duration
4. Click → confirm
5. 5-second countdown
6. Exercise runs (rep count or timer)
7. Completion → buzzer + display

---

# Bluetooth Data

Examples:

Rep-based:
pushups: 12 reps

Time-based:
plank: 45 sec

Recommended format:
exercise_complete: pushups,12

---

# Rep Detection

- Read IMU
- Smooth signal
- Detect peaks
- Count cycles

---

# Timer Logic

Use millis():

if (millis() - startTime >= duration) {
  // done
}

---

# Power Usage

~20–40 mA total

---

# Integration with Teensy

Teensy:
- Receives BLE data
- Displays info
- Logs to SD card

---

# Design Philosophy

User selects exercise → system tracks accurately

---

# Future Ideas

- Posture detection
- AI classification
- Multiple sensors

---

# Summary

- Accurate reps
- Timer support
- Simple UI
- Expandable system

# Routine Warden

A touchscreen-based daily routine, productivity, and lifestyle tracker built on a Teensy 4.1 with a 4" capacitive display, an HX711 load cell, a DS3231 RTC, and SD-card persistence.

---

## Table of Contents

1. [Overview](#overview)
2. [Features](#features)
3. [Parts List](#parts-list)
4. [Hardware Setup](#hardware-setup)
5. [Software Setup](#software-setup)
6. [Application Functions](#application-functions)
7. [Pages & Functionality](#pages)
8. [Data & Persistence](#data--persistence)
9. [Future Improvements](#future-improvements)

---

## Overview

The Routine Warden is a custom-built embedded system designed to help track:

- Daily routines (morning / afternoon / night)
- Meals (with optional weighed portion sizes)
- Exercise sessions
- Study / Homework timers
- General productivity habits

It runs as a single Arduino sketch (`routine_tracker.ino`) on a Teensy 4.1, with all UI, persistence, and sensor logic in one file.

---

## Features

- Touch-based UI on a 320x480 ILI9488 display
- Three-stage routine tracking (morning / afternoon / night) with auto-reset after 3 AM each day
- Tri-segment progress donut on the home screen showing per-routine completion and overall daily %
- Combined Homework / Study timer page with circular progress indicator
- Meal tracker with food selection, portion size (S/M/L), and protein flag
- Built-in scale flow: weighs full plate, waits for the meal to be eaten, weighs the empty plate, derives food weight automatically
- Manual weight-entry page (slider) when no scale is connected
- Exercise tracker (template A/B/C/Rest, minutes, effort, hunger, bodyweight, notes)
- Live clock tile and full-screen clock page driven by the DS3231 RTC
- Dark / light theme toggle, persisted across reboots
- SD-card persistence for routines, meals, exercise, and theme
- USB serial command (`GET <filename>`) to dump SD-card files to a host computer
- 5-minute idle timeout
- 3D-printable enclosure (see `3D_Printing_Files/`)

---

## Parts List

- [Teensy 4.1](https://www.amazon.com/PJRC-Teensy-4-1-with-Pins/dp/B08CTM3279)
- [Display (ILI9488 + FT6336U touch)](https://www.amazon.com/Hosyond-320x480-Capacitive-Display-Mega2560/dp/B0CRGQN58D?th=1)
- [Command Strips](https://www.amazon.com/dp/B0751S46TS?th=1)
- [DS3231 RTC Module](https://www.amazon.com/dp/B09LLMYBM1?ref=ppx_yo2ov_dt_b_fed_asin_title&th=1)
- [Bolts](https://www.amazon.com/dp/B0FG2LRFZ2)
- [Magnets](https://www.amazon.com/dp/B096LYVGPS)
- [Protoboard](https://www.amazon.com/Treedix-Solderable-BreadBoard-Universal-Prototyping/dp/B0896YPD8F)
- HX711 load cell amplifier + load cell (for the meal-weighing flow)
- microSD card (uses the Teensy 4.1 built-in SD slot)

---

## Hardware Setup

Pin assignments used by the sketch:

| Function          | Pin            |
| ----------------- | -------------- |
| TFT CS            | 9              |
| TFT DC            | 7              |
| TFT RST           | 8              |
| TFT LED (backlight) | 6            |
| Touch (FT6336U)   | I²C @ 0x38     |
| RTC (DS3231)      | I²C            |
| HX711 DT          | 4              |
| HX711 SCK         | 5              |
| SD card           | Teensy built-in slot |

---

## Software Setup

**Install Arduino IDE 1.8.19**
Download: [Arduino IDE](https://www.arduino.cc/en/software)
Make sure to install the 1.8.19 version, not Arduino 2.x

**Install Teensyduino**
Download: [TeensyDuino](https://www.pjrc.com/teensy/td_download.html)
Follow the instructions and install TeensyDuino

**Install libraries via Arduino IDE:**
1. Go to Sketch in the ribbon
2. Select Include Library
3. Select Manage Libraries
4. Search for and install the latest versions of:
   - `ArduinoJson` (v6.x)
   - `RTClib` (Adafruit)
   - `HX711` (by Bogdan Necula / Rob Tillaart)
   - `ILI9488_t3`

**Install libraries via zip import:**
1. Go to GitHub repo: [RAK14014 touch library](https://github.com/RAKWireless/RAK14014-FT6336U)
2. Download ZIP from GitHub
3. Go back to Arduino IDE
4. Select Sketch in the ribbon
5. Select Include Library
6. Select Add .ZIP library
7. Select downloaded ZIP file

**Select Board:**
1. Go to Tools in the ribbon
2. Select Board
3. Select Teensy 4.1

---

## Application Functions

The sketch is structured around a single `loop()` that dispatches to per-page handlers based on `currentPage`. The major subsystems:

### Hardware integration
- **Display / Touch** — ILI9488 TFT driven via `ILI9488_t3` with frame buffering, plus FT6336U capacitive touch over I²C.
- **RTC** — DS3231 supplies date/time, drives stage detection (morning/afternoon/night) and the daily routine reset.
- **Scale** — HX711 load cell amplifier with stable-weight detection used by the meal-weighing flow.
- **Storage** — SD card holds routines, meal log, exercise log, and theme preference.

### Routines
- Three independent task lists (`morningTasks`, `afternoonTasks`, `nightTasks`) tap to toggle done/undone.
- Tasks persist across reboots in `routines.txt` (`saveRoutines` / `loadRoutines`).
- `checkRoutineReset` clears all tasks once per day after 3 AM and bumps `lastResetDate`.
- Each task is assigned a shuffled color from a 19-color palette so the list looks fresh each day.

### Home screen
- 3x2 grid of tiles (current routine, clock, school timer, meal tracker, exercise tracker, settings).
- Live clock tile updates every minute.
- Tri-segment progress donut splits the ring into morning / afternoon / night thirds and shows the combined daily percentage in the center.

### Timers
- Combined timer page hosts both modes:
  - **Homework timer** — 10-minute countdown.
  - **Study timer** — 15-minute study / 10-minute break cycle.
- Circular progress indicator updates live; pause/resume supported.

### Meal tracking
- Per-day record for breakfast, lunch, dinner, and snack: size (S/M/L), protein flag, and (optionally) weighed grams.
- Food selection page lets the user pick a specific food per meal category.
- **Weigh flow**: wait for plate+food to settle → 3-second countdown → wait for the user to eat → wait for empty plate to settle → 3-second countdown → derive food weight (full − empty).
- Manual weight-entry page (slider-based) for when no scale is attached.
- Entries are appended to a CSV on the SD card and reloaded on boot.

### Exercise tracker
- Template (A / B / C / Rest), minutes, effort (0–10), hunger (1–5), bodyweight, and free-form notes.
- Bodyweight pre-fills from the most recent CSV entry.
- Saves to a CSV on the SD card.

### Settings
- Dark / light theme toggle (`applyTheme` recolors background, text, and box colors).
- Theme persists to the SD card (`saveThemeToSD` / `loadThemeFromSD`).

### Serial / debug
- `handleSerialCommands` accepts `GET <filename>` over USB serial and streams the file back wrapped in `START_FILE` / `END_FILE` markers — useful for pulling logs off the device without removing the SD card.

---

## Pages

### Home
Navigation hub showing the current routine tile, live clock, timer, meal, exercise, and settings tiles, plus the daily progress donut.

### Routine Pages (Morning / Afternoon / Night)
Tap tasks to mark them done; the home tile turns green when the active routine is complete.

### Clock
Full-screen clock view.

### Timer
Combined Homework + Study/Break modes with circular progress.

### Meal Tracker
Per-meal entries with food selection, portion size, and protein flag. Optional weighed grams via the scale flow or manual slider.

### Exercise Tracker
Template, effort, energy, hunger, bodyweight, and notes.

### Settings
Toggle dark / light mode.

---

## Data & Persistence

Files written to the SD card:

| File              | Contents                                                  |
| ----------------- | --------------------------------------------------------- |
| `routines.txt`    | Current stage, last reset date, and done/not-done flags for each task |
| Meal CSV          | One row per meal entry (food, weight in lbs, time)        |
| Exercise CSV      | One row per exercise day (template, minutes, effort, hunger, weight, notes) |
| Theme file        | Persisted dark/light preference                           |

Use the serial `GET <filename>` command to dump any of these to a connected host.

---

## Future Improvements

- Keyboard input for notes and custom foods
- On-device analytics / trend charts
- Wi-Fi sync or cloud backup of the CSV logs
- Battery / power-management support
- Configurable routine task lists from the UI (currently hard-coded in the sketch)

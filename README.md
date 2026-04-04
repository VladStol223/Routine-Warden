# Routine Warden

A touchscreen-based daily routine, productivity, and lifestyle tracker built on a Teensy 4.1 with a 4" capacitive display.

---

## Table of Contents

1. Overview  
2. Features  
3. Parts List  
4. Hardware Setup  
5. Software Setup  
6. Pages & Functionality  
7. File Structure  
8. Future Improvements  

---

## Overview

Routine Warden is a custom-built embedded system designed to help track:

- Daily routines (morning/night)
- Meals
- Exercise
- Study/Homework timers
- General productivity habits

---

## Features

- Touch-based UI  
- Routine tracking with progress visualization  
- Meal tracking (S/M/L)  
- Exercise tracking  
- Combined study/homework timer  
- Dark mode toggle  
- 3D printable enclosure  

---

## Parts List

- Teensy 4.1  
- Display: https://www.amazon.com/dp/B0751S46TS  
- Command Strips: https://www.amazon.com/dp/B09LLMYBM1  
- RTC Module: https://www.amazon.com/dp/B0FG2LRFZ2  
- Bolts: https://www.amazon.com/dp/B0FG2LRFZ2  
- Magnets: https://www.amazon.com/dp/B096LYVGPS  
- Protoboard: https://www.amazon.com/Treedix-Solderable-BreadBoard-Universal-Prototyping/dp/B0896YPD8F  

---

## Software Setup

Install Arduino IDE 1.8.19  
https://www.arduino.cc/en/software  

Install Teensyduino  
https://www.pjrc.com/teensy/td_download.html  

Install libraries via Arduino IDE:
- ArduinoJson (v6.x)
- RAK14014 FT6336U (GitHub ZIP install)

Select Board: Teensy 4.1  

---

## Pages

### Home
Navigation hub for all apps.

### Routine Pages
Tap tasks, track completion, view progress.

### Timer
Homework + Study modes with circular progress.

### Meal Tracker
Tap-based meal logging (S/M/L).

### Exercise Tracker
Tracks template, effort, energy, hunger, weight.

### Settings
Toggle dark/light mode.

---

## File Structure

- routine_tracker.ino  
- 3d_printing_files/  

---

## Future Improvements

- SD card saving  
- RTC integration  
- Keyboard input  
- Analytics  

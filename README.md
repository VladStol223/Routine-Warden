# Routine Warden

A touchscreen-based daily routine, productivity, and lifestyle tracker built on a Teensy 4.1 with a 4" capacitive display.

---

## Table of Contents

1. [Overview](#overview)  
2. [Features](#features)
3. [Parts List](#parts-list)  
4. [Hardware Setup](#parts-list)
5. [Software Setup](#software-setup)
6. [Pages & Functionality](#pages)  
7. [Future Improvements](#future-improvements)  

---

## Overview

The Routine Warden is a custom-built embedded system designed to help track:

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

- [Teensy 4.1](https://www.amazon.com/PJRC-Teensy-4-1-with-Pins/dp/B08CTM3279)
- [Display](https://www.amazon.com/Hosyond-320x480-Capacitive-Display-Mega2560/dp/B0CRGQN58D?th=1)  
- [Command Strips](https://www.amazon.com/dp/B0751S46TS?th=1)  
- [RTC Module](https://www.amazon.com/dp/B09LLMYBM1?ref=ppx_yo2ov_dt_b_fed_asin_title&th=1)  
- [Bolts](https://www.amazon.com/dp/B0FG2LRFZ2)  
- [Magnets](https://www.amazon.com/dp/B096LYVGPS)  
- [Protoboard](https://www.amazon.com/Treedix-Solderable-BreadBoard-Universal-Prototyping/dp/B0896YPD8F)  

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
2. select include library
3. select manage libraries
4. search ArduinoJson
5. install latest v6.x

**Install libraries via zip import:**
1. go to github repo: [RAK14014 touch library](https://github.com/RAKWireless/RAK14014-FT6336U)
2. Download ZIP from GitHub
3. Go back to Arduino IDE
4. Select Sketch in the ribbon
5. Select include library
6. Select Add .ZIP library
7. select downloaded ZIP file

**Select Board:** 
1. Go to Tools in the ribbon
2. Select Board
3. Seelct Teensy 4.1  

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

## Future Improvements

- SD card saving  
- RTC integration  
- Keyboard input  
- Analytics  

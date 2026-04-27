//Routine Warden
#include <ILI9488_t3.h>
#include <Wire.h>
#include "RAK14014_FT6336U.h"
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>

struct Task;

// ---------------- DISPLAY ----------------
#define TFT_CS   9
#define TFT_DC   7
#define TFT_RST  8
#define TFT_LED  6

ILI9488_t3 tft(TFT_CS, TFT_DC, TFT_RST);
FT6336U touchController;

// ---------------- TIME ----------------
RTC_DS3231 rtc;
int lastMinute = -1;

elapsedMillis runtime;
elapsedMillis idleTimer;
const unsigned long IDLE_TIMEOUT = 300000;


String getDateString(){
  DateTime now = rtc.now();
  char buf[20];
  sprintf(buf,"%02d/%02d/%04d", now.month(), now.day(), now.year());
  return String(buf);
}

// ---------------- TIMER ----------------
const unsigned long TIMER_DURATION = 600000; // 10 min

unsigned long timerRemaining = TIMER_DURATION;
unsigned long lastUpdate = 0;

bool timerRunning = false;
bool timerPaused = false;

bool isStudyMode = false; // false = homework, true = study

// ---------------- STUDY TIMER ----------------
const unsigned long STUDY_DURATION = 900000; // 15 min = 900000
const unsigned long BREAK_DURATION = 600000; // 10 min = 600000

unsigned long studyRemaining = STUDY_DURATION;

bool studyMode = true; // true = study, false = break
bool studyRunning = false;
bool studyPaused = false;

unsigned long studyLastUpdate = 0;

// ---------------- theme system ----------------

bool darkMode = false;

uint16_t BG_COLOR = 0xFFFF;
uint16_t TEXT_COLOR = 0x0000;
uint16_t BOX_COLOR = 0x2104;

void applyTheme(){
  if(darkMode){
    BG_COLOR = 0x0000;
    TEXT_COLOR = 0xFFFF;
    BOX_COLOR = 0x4208;
  } else {
    BG_COLOR = 0xFFFF;
    TEXT_COLOR = 0x0000;
    BOX_COLOR = 0xC618;
  }
}

// ---------------- MEAL TRACKER ----------------
struct MealEntry {
  int size;     // 0 none, 1 S, 2 M, 3 L
  bool protein; // true = yes, false = no
  bool proteinSet; // to track if user selected it
};

struct MealDay {
  MealEntry breakfast;
  MealEntry lunch;
  MealEntry dinner;
  MealEntry snack;
};

MealDay todayMeals;

// ---------------- SCALE ----------------
#include "HX711.h"

#define HX_DT  4
#define HX_SCK 5

HX711 scale;

float CAL_FACTOR = 123000.0;

bool scaleConnected = false;

unsigned long lastReadTime = 0;
unsigned long lastPrintTime = 0;

unsigned long FAST_THRESHOLD = 80;

// ---------------- WEIGH STATE ----------------
float currentWeight = 0;

float totalWeight = 0;
float foodWeight = 0;

bool totalCaptured = false;
bool plateRemoved = false;

unsigned long stableStart = 0;
float lastStableWeight = 0;

// ---------------- EXERCISE TRACKER ----------------
struct ExerciseDay {
  int templateType = 0; // 0 none, 1 A, 2 B, 3 C, 4 Rest

  int minutes = 30;

  int effort = 0;   // 0–10
  int hunger = 0;   // 1–5

  float weight = 202.0;

  String notes = "";
};

ExerciseDay todayExercise;

// ---------------- UI STATE ----------------
enum Page {
  HOME_PAGE,
  CLOCK_PAGE,
  MORNING_PAGE,
  AFTERNOON_PAGE,
  NIGHT_PAGE,
  MEAL_PAGE,
  WEIGH_PAGE,
  FOOD_PAGE,           // NEW - for food selection
  MANUAL_WEIGHT_PAGE,  // NEW - for manual weight entry with slider
  EXERCISE_PAGE,
  TIMER_PAGE,
  SETTINGS_PAGE
};

Page currentPage = HOME_PAGE;
Page lastPage = HOME_PAGE;

// ---------------- WEIGH STATES ----------------
enum WeighStep {
  WEIGH_WAIT_PLATE,      // waiting for plate+food to settle
  WEIGH_COUNTDOWN_FULL,  // 3-sec countdown after plate+food settled
  WEIGH_WAIT_EMPTY,      // waiting for user to finish eating (idle)
  WEIGH_WAIT_RETURN,     // waiting for empty plate to settle
  WEIGH_COUNTDOWN_EMPTY, // 3-sec countdown after empty plate settled
  WEIGH_DONE             // done, auto-advance
};

// ---------------- FOOD SELECTION ----------------
enum FoodCategory { CAT_NONE = -1, CAT_BREAKFAST = 0, CAT_LUNCH, CAT_DINNER, CAT_SNACK };

// Forward declarations for food helper functions
const char** getFoodList(FoodCategory cat);
int getFoodCount(FoodCategory cat);
const char* getCatName(FoodCategory cat);

// ---------------- TASK STRUCT ----------------
struct Task {
  const char* label;
  bool done;
  uint16_t color;
};

// ---------------- TASKS ----------------
uint16_t MORNING_COLOR   = 0xFEA0; // golden yellow
uint16_t AFTERNOON_COLOR = 0x4B7F; // soft navy blue (not too dark)
uint16_t NIGHT_COLOR     = 0x801F; // royal purple

Task morningTasks[] = {
  {"Go Piss", false, 0},
  {"Brush Teeth", false, 0},
  {"Sunscreen", false, 0},
  {"Log weight", false, 0},
  {"Breakfast", false, 0},
  {"Make Bed", false, 0}
};

Task afternoonTasks[] = {
  {"Workout", false, 0},
  {"Shower", false, 0},
  {"Clean Dishes", false, 0},
  {"Log Meals", false, 0},
  {"Log workout", false, 0}
};

Task nightTasks[] = {
  {"Floss", false, 0},
  {"Brush Teeth", false, 0},
  {"Wash Face", false, 0},
  {"Moisturise", false, 0},
  {"Clean Counters", false, 0}
};

const int MORNING_COUNT = sizeof(morningTasks)/sizeof(Task);
const int NIGHT_COUNT   = sizeof(nightTasks)/sizeof(Task);
const int AFTERNOON_COUNT = sizeof(afternoonTasks)/sizeof(Task);

// ---------------- FLAGS ----------------
bool needsRedraw = true;
int scrollOffset = 0;

// ---------------- COLORS ----------------
uint16_t colors[] = {
  0xF800,0xFFE0,0xF81F,0x07FF,0xFD20,0xFA20,0xFFE5,0x7BEF,
  0xFBE0,0x07E0,0x9FC0,0xAFE5,0xE7FF,0xF7BB,0xFC9F,
  0xFC0F,0xD7BF,0x7DDE,0xAD6B
};

const int NUM_COLORS = sizeof(colors)/sizeof(colors[0]);

void shuffleColors() {
  for (int i = NUM_COLORS - 1; i > 0; i--) {
    int j = random(i + 1);
    uint16_t tmp = colors[i];
    colors[i] = colors[j];
    colors[j] = tmp;
  }
}


void assignColors(Task tasks[], int count) {
  shuffleColors();
  for (int i = 0; i < count; i++) {
    tasks[i].color = colors[i % NUM_COLORS];
  }
}

// ================= ROUTINE STATE =================

enum RoutineStage {
  MORNING_STAGE,
  AFTERNOON_STAGE,
  NIGHT_STAGE
};

RoutineStage currentStage = MORNING_STAGE;
String lastResetDate = "";

bool isRoutineComplete(Task tasks[], int count){
  for(int i=0;i<count;i++){
    if(!tasks[i].done) return false;
  }
  return true;
}

void saveRoutines(){

  String stageStr = "morning";
  if(currentStage == AFTERNOON_STAGE) stageStr = "afternoon";
  if(currentStage == NIGHT_STAGE) stageStr = "night";

  String output = stageStr + "\n";
  output += lastResetDate + "\n";

  // MORNING
  for(int i=0;i<MORNING_COUNT;i++){
    output += (morningTasks[i].done ? "1":"0");
    if(i < MORNING_COUNT-1) output += ",";
  }
  output += "\n";

  // AFTERNOON
  for(int i=0;i<AFTERNOON_COUNT;i++){
    output += (afternoonTasks[i].done ? "1":"0");
    if(i < AFTERNOON_COUNT-1) output += ",";
  }
  output += "\n";

  // NIGHT
  for(int i=0;i<NIGHT_COUNT;i++){
    output += (nightTasks[i].done ? "1":"0");
    if(i < NIGHT_COUNT-1) output += ",";
  }

  SD.remove("routines.txt");

  File f = SD.open("routines.txt", FILE_WRITE);
  if(f){
    f.print(output);
    f.close();
  }
}

void loadRoutines(){

  File f = SD.open("routines.txt", FILE_READ);

  if(!f){
    Serial.println("No routines.txt, creating default");
    lastResetDate = "";
    currentStage = MORNING_STAGE;
    saveRoutines();
    return;
  }

  String stageLine = f.readStringUntil('\n');
  stageLine.trim();
  
  if(stageLine.length() == 0){
    Serial.println("Corrupt routines.txt (stage)");
    f.close();
    saveRoutines();
    return;
  }
  
  if(stageLine == "morning") currentStage = MORNING_STAGE;
  else if(stageLine == "afternoon") currentStage = AFTERNOON_STAGE;
  else currentStage = NIGHT_STAGE;
  
  // NEW: read last reset date
  lastResetDate = f.readStringUntil('\n');
  lastResetDate.trim();
  
  // If date line is invalid, assume fresh start
  if(lastResetDate.indexOf('/') == -1){
    Serial.println("Invalid date format, resetting routines");
  
    lastResetDate = "";
    currentStage = MORNING_STAGE;
  
    for(int i=0;i<MORNING_COUNT;i++) morningTasks[i].done = false;
    for(int i=0;i<AFTERNOON_COUNT;i++) afternoonTasks[i].done = false;
    for(int i=0;i<NIGHT_COUNT;i++) nightTasks[i].done = false;
  
    f.close();
    saveRoutines();
    return;
  }

  // helper
  String line;
  
  // MORNING
  line = f.readStringUntil('\n');
  int start = 0;
  for(int i=0;i<MORNING_COUNT;i++){
    int comma = line.indexOf(',', start);
    if(comma == -1) comma = line.length();
  
    String val = line.substring(start, comma);
    morningTasks[i].done = (val == "1");
  
    start = comma + 1;
  }
  
  // AFTERNOON
  line = f.readStringUntil('\n');
  start = 0;
  for(int i=0;i<AFTERNOON_COUNT;i++){
    int comma = line.indexOf(',', start);
    if(comma == -1) comma = line.length();
  
    String val = line.substring(start, comma);
    afternoonTasks[i].done = (val == "1");
  
    start = comma + 1;
  }
  
  // NIGHT
  line = f.readStringUntil('\n');
  start = 0;
  for(int i=0;i<NIGHT_COUNT;i++){
    int comma = line.indexOf(',', start);
    if(comma == -1) comma = line.length();
  
    String val = line.substring(start, comma);
    nightTasks[i].done = (val == "1");
  
    start = comma + 1;
  }

  f.close();
}

// RESET LOGIC
void checkRoutineReset(){

  String today = getDateString();

  DateTime now = rtc.now();
  int h = now.hour();

  // Only reset once per day AFTER 3AM
  if(today != lastResetDate && h >= 3){

    // reset all routines
    for(int i=0;i<MORNING_COUNT;i++) morningTasks[i].done = false;
    for(int i=0;i<AFTERNOON_COUNT;i++) afternoonTasks[i].done = false;
    for(int i=0;i<NIGHT_COUNT;i++) nightTasks[i].done = false;

    currentStage = MORNING_STAGE;

    lastResetDate = today;

    saveRoutines();
  }
}

// ============================================================
// DONUT
// ============================================================
float getRoutineProgress(Task tasks[], int count){
  int done = 0;
  for(int i=0;i<count;i++){
    if(tasks[i].done) done++;
  }
  return (float)done / count;
}

void drawDonut(int cx, int cy, int rOuter, int rInner) {

  // Background ring
  tft.fillCircle(cx, cy, rOuter, 0xC618);
  tft.fillCircle(cx, cy, rInner, BG_COLOR);

  int segments = 120;

  // Get progress for each routine
  float pMorning   = getRoutineProgress(morningTasks, MORNING_COUNT);
  float pAfternoon = getRoutineProgress(afternoonTasks, AFTERNOON_COUNT);
  float pNight     = getRoutineProgress(nightTasks, NIGHT_COUNT);

  // Split ring into thirds
  int segPerBlock = segments / 3;

  for (int i = 0; i < segments; i++) {

    float a1 = -PI/2 + (2*PI*i/segments);
    float a2 = -PI/2 + (2*PI*(i+1)/segments);

    int x1 = cx + cos(a1) * rOuter;
    int y1 = cy + sin(a1) * rOuter;
    int x2 = cx + cos(a2) * rOuter;
    int y2 = cy + sin(a2) * rOuter;

    int x3 = cx + cos(a1) * rInner;
    int y3 = cy + sin(a1) * rInner;
    int x4 = cx + cos(a2) * rInner;
    int y4 = cy + sin(a2) * rInner;

    uint16_t color = 0xC618; // default gray

    // -------- MORNING (first third) --------
    if(i < segPerBlock){
      int filled = pMorning * segPerBlock;
      if(i < filled) color = MORNING_COLOR;
    }

    // -------- AFTERNOON (second third) --------
    else if(i < 2*segPerBlock){
      int idx = i - segPerBlock;
      int filled = pAfternoon * segPerBlock;
      if(idx < filled) color = AFTERNOON_COLOR;
    }

    // -------- NIGHT (last third) --------
    else{
      int idx = i - 2*segPerBlock;
      int filled = pNight * segPerBlock;
      if(idx < filled) color = NIGHT_COLOR;
    }

    tft.fillTriangle(x1,y1,x2,y2,x3,y3,color);
    tft.fillTriangle(x2,y2,x3,y3,x4,y4,color);
  }

  // -------- CENTER TEXT (TOTAL %) --------
  float totalProgress =
    (getRoutineProgress(morningTasks, MORNING_COUNT) +
     getRoutineProgress(afternoonTasks, AFTERNOON_COUNT) +
     getRoutineProgress(nightTasks, NIGHT_COUNT)) / 3.0;

  int pct = round(totalProgress * 100);

  char buf[6];
  sprintf(buf,"%d%%",pct);

  tft.setTextSize(3);
  tft.setTextColor(TEXT_COLOR);

  int16_t x1,y1;
  uint16_t w,h;
  tft.getTextBounds(buf,0,0,&x1,&y1,&w,&h);

  tft.setCursor(cx-w/2,cy-h/2);
  tft.print(buf);
}

// ============================================================
// BACK BUTTON
// ============================================================
void drawBackButton() {
  int size = 40;
  int x = tft.width() - size - 10;
  int y = 10;
  tft.fillRect(x,y,size,size,0xF800);
}

bool backPressed(uint16_t x,uint16_t y){
  int size = 40;
  int bx = tft.width() - size - 10;
  int by = 10;
  return (x>=bx && x<=bx+size && y>=by && y<=by+size);
}

// ============================================================
// HOME
// ============================================================
void drawHome() {

  tft.fillScreen(BG_COLOR);

  const char* labels[6];
  
  if(currentStage == MORNING_STAGE){
    labels[0] = "Morning Routine";
  }
  else if(currentStage == AFTERNOON_STAGE){
    labels[0] = "Afternoon Routine";
  }
  else{
    labels[0] = "Night Routine";
  }
  
  labels[1] = "Clock";
  labels[2] = "School Timer";
  labels[3] = "Meal Tracker";
  labels[4] = "Exercise Tracker";
  labels[5] = "Settings";

  int cols=3,rows=2,pad=20;
  int boxW=(tft.width()-(cols+1)*pad)/cols;
  int boxH=(tft.height()-(rows+1)*pad)/rows;

  for(int r=0;r<rows;r++){
    for(int c=0;c<cols;c++){

      int i=r*cols+c;
      int x=pad+c*(boxW+pad);
      int y=pad+r*(boxH+pad);

      tft.fillRoundRect(x,y,boxW,boxH,12,BG_COLOR);

      for(int k=0;k<5;k++){
        tft.drawRoundRect(x-k,y-k,boxW+2*k,boxH+2*k,12,BOX_COLOR);
      }

      tft.setTextSize(2);
      bool complete = false;
      
      if(i == 0){
        if(currentStage == MORNING_STAGE)
          complete = isRoutineComplete(morningTasks, MORNING_COUNT);
        else if(currentStage == AFTERNOON_STAGE)
          complete = isRoutineComplete(afternoonTasks, AFTERNOON_COUNT);
        else
          complete = isRoutineComplete(nightTasks, NIGHT_COUNT);
      }
      
      tft.setTextColor(complete ? 0x07E0 : TEXT_COLOR);
      
      // Split into two words
      if(i == 1){
        // ===== CLOCK TILE =====
        DateTime now = rtc.now();
      
        // TIME
        char timeBuf[10];
        int hour = now.hour();
        bool isPM = hour >= 12;
        
        hour = hour % 12;
        if(hour == 0) hour = 12;
        
        sprintf(timeBuf,"%d:%02d%s", hour, now.minute(), isPM ? "PM" : "AM");
      
        tft.setTextSize(3);
      
        int16_t x1,y1;
        uint16_t w,h;
        tft.getTextBounds(timeBuf,0,0,&x1,&y1,&w,&h);
      
        tft.setCursor(x + (boxW - w)/2, y + boxH/2 - 20);
        tft.print(timeBuf);
      
        // DATE
        char dateBuf[20];
        sprintf(dateBuf,"%02d/%02d",now.month(),now.day());
      
        tft.setTextSize(2);
      
        tft.getTextBounds(dateBuf,0,0,&x1,&y1,&w,&h);
      
        tft.setCursor(x + (boxW - w)/2, y + boxH/2 + 10);
        tft.print(dateBuf);
      
        tft.setTextSize(2); // reset
      }
      else{
        // ===== NORMAL TILE =====
        String full = String(labels[i]);
        int spaceIndex = full.indexOf(' ');
      
        String line1 = full;
        String line2 = "";
      
        if(spaceIndex != -1){
          line1 = full.substring(0, spaceIndex);
          line2 = full.substring(spaceIndex + 1);
        }
      
        int16_t x1,y1;
        uint16_t w1,h1,w2,h2;
      
        tft.getTextBounds(line1.c_str(),0,0,&x1,&y1,&w1,&h1);
        tft.getTextBounds(line2.c_str(),0,0,&x1,&y1,&w2,&h2);
      
        int totalHeight = h1 + h2 + 6;
        int startY = y + (boxH - totalHeight)/2;
      
        tft.setCursor(x + (boxW - w1)/2, startY);
        tft.print(line1);
      
        if(line2 != ""){
          tft.setCursor(x + (boxW - w2)/2, startY + h1 + 6);
          tft.print(line2);
        }
      }
    }
  }

  tft.updateScreen();
}

void handleHomeTouch(){
  if(!touchController.read_td_status()) return;

  uint16_t x=touchController.read_touch1_x();
  uint16_t y=touchController.read_touch1_y();

  uint16_t newX = tft.width() - map(y,0,480,0,tft.width());
  uint16_t newY = map(x,0,320,0,tft.height());

  int cols=3,rows=2,pad=20;
  int boxW=(tft.width()-(cols+1)*pad)/cols;
  int boxH=(tft.height()-(rows+1)*pad)/rows;

  for(int r=0;r<rows;r++){
    for(int c=0;c<cols;c++){

      int i=r*cols+c;
      int x0=pad+c*(boxW+pad);
      int y0=pad+r*(boxH+pad);

      if(newX>=x0 && newX<=x0+boxW && newY>=y0 && newY<=y0+boxH){

        if(i==0){
          if(currentStage == MORNING_STAGE) currentPage = MORNING_PAGE;
          else if(currentStage == AFTERNOON_STAGE) currentPage = AFTERNOON_PAGE;
          else currentPage = NIGHT_PAGE;
        }
        
        if(i==1){
          currentPage = CLOCK_PAGE;
        }
        if(i==2) currentPage=TIMER_PAGE;
        if(i==3) currentPage=MEAL_PAGE;
        if(i==4) currentPage=EXERCISE_PAGE;
        if(i==5) currentPage=SETTINGS_PAGE;

        needsRedraw=true;
        return;
      }
    }
  }
}

// ============================================================
// TASK PAGE
// ============================================================
float getDailyProgress(){

  int totalTasks = MORNING_COUNT + AFTERNOON_COUNT + NIGHT_COUNT;

  int totalDone = 0;

  for(int i=0;i<MORNING_COUNT;i++){
    if(morningTasks[i].done) totalDone++;
  }

  for(int i=0;i<AFTERNOON_COUNT;i++){
    if(afternoonTasks[i].done) totalDone++;
  }

  for(int i=0;i<NIGHT_COUNT;i++){
    if(nightTasks[i].done) totalDone++;
  }

  return (float)totalDone / totalTasks;
}

void drawTasks(Task tasks[],int count,const char* title){

  tft.fillScreen(BG_COLOR);
  drawBackButton();

  int taskW = tft.width()*0.55;

  int done = 0;
  for(int i=0;i<count;i++){
    if(tasks[i].done) done++;
  }
  
  int y=50-scrollOffset;
  
  if(done < count){
    tft.setCursor(10,y);
    tft.print("Incomplete:");
    y+=25;
  }

  tft.setTextColor(0x0000);

  for(int i=0;i<count;i++){
    if(!tasks[i].done){
      tft.fillRoundRect(10,y,taskW-20,32,6,tasks[i].color);
      tft.setCursor(15,y+8);
      tft.print(tasks[i].label);
      y+=40;
    }
  }

  tft.setTextColor(TEXT_COLOR);

  if(done>0){
    tft.setCursor(10,y);
    tft.print("Complete:");
    y+=25;
  }

  tft.setTextColor(0x0000);

  for(int i=0;i<count;i++){
    if(tasks[i].done){
      tft.fillRoundRect(10,y,taskW-20,32,6,0xC618);
      tft.setCursor(15,y+8);
      tft.print(tasks[i].label);
      y+=40;
    }
  }

  tft.setTextColor(TEXT_COLOR);

  drawDonut(taskW+(tft.width()-taskW)/2,tft.height()/2-40,70,50);


  int cx = taskW + (tft.width()-taskW)/2;
  int cy = tft.height()/2 - 40;
  
  drawArrowButton(cx-60, cy+110, true);
  drawArrowButton(cx+15, cy+110, false);

  // ---------- HEADER OVERLAY ----------
  tft.fillRect(0, 0, tft.width(), 36, BG_COLOR);  // white bar
  
  // redraw back button (it gets covered)
  drawBackButton();
  
  // ---------- TITLE ----------
  tft.setTextSize(3);
  tft.setTextColor(TEXT_COLOR);
  
  int16_t x1,y1;
  uint16_t w,h;
  tft.getTextBounds(title,0,0,&x1,&y1,&w,&h);
  
  tft.setCursor(10, 10);
  tft.print(title);
  
  // reset font size
  tft.setTextSize(2);

  tft.updateScreen();
}

void drawArrowButton(int x, int y, bool up) {
  int w = 45;
  int h = 45;

  tft.fillRoundRect(x, y, w, h, 8, 0xC618);

  int cx = x + w/2;
  int cy = y + h/2;
  int size = 10;

  if (up)
    tft.fillTriangle(cx-size, cy+size/2, cx+size, cy+size/2, cx, cy-size, 0x0000);
  else
    tft.fillTriangle(cx-size, cy-size/2, cx+size, cy-size/2, cx, cy+size, 0x0000);
}

void handleTasks(Task tasks[], int count){

  if(!touchController.read_td_status()) return;

  uint16_t x=touchController.read_touch1_x();
  uint16_t y=touchController.read_touch1_y();

  uint16_t newX = tft.width() - map(y,0,480,0,tft.width());
  uint16_t newY = map(x,0,320,0,tft.height());

  // BACK
  if(backPressed(newX,newY)){
  
    if(tasks == morningTasks && isRoutineComplete(morningTasks, MORNING_COUNT)){
      currentStage = AFTERNOON_STAGE;
    }
    else if(tasks == afternoonTasks && isRoutineComplete(afternoonTasks, AFTERNOON_COUNT)){
      currentStage = NIGHT_STAGE;
    }
  
    saveRoutines();
  
    currentPage = HOME_PAGE;
    needsRedraw = true;
    return;
  }

  int taskW = tft.width()*0.55;

  int cx = taskW + (tft.width()-taskW)/2;
  int cy = tft.height()/2 - 40;
  
  // UP
  if(newX >= cx-60 && newX <= cx-60+45 &&
     newY >= cy+110 && newY <= cy+110+45){
    scrollOffset -= 40;
    if(scrollOffset < 0) scrollOffset = 0;
    needsRedraw = true;
    return;
  }
  
  // DOWN
  if(newX >= cx+15 && newX <= cx+15+45 &&
     newY >= cy+110 && newY <= cy+110+45){
    scrollOffset += 40;
    needsRedraw = true;
    return;
  }


  int yPos = 50 - scrollOffset;

  int doneCount = 0;
  for(int i=0;i<count;i++) if(tasks[i].done) doneCount++;

  // ---------- INCOMPLETE ----------
  if(doneCount < count){
    yPos += 25;
  }

  for(int i=0;i<count;i++){
    if(!tasks[i].done){
      if(newX < taskW && newY >= yPos && newY <= yPos+32){
        tasks[i].done = true;
        
        saveRoutines();
        
        needsRedraw = true;
        return;
      }
      yPos += 40;
    }
  }

  // ---------- COMPLETE ----------
  if(doneCount > 0){
    yPos += 25;
  }

  for(int i=0;i<count;i++){
    if(tasks[i].done){
      if(newX < taskW && newY >= yPos && newY <= yPos+32){
        tasks[i].done = false;
        
        saveRoutines();
        
        needsRedraw = true;
        return;
      }
      yPos += 40;
    }
  }
}

// ============================================================
// TIMER PAGES
// ============================================================

void drawCombinedTimerPage(){

  tft.fillScreen(BG_COLOR);
  drawBackButton();

  // ===== MODE SWITCH BUTTONS =====
  tft.setTextSize(2);

  // Homework button
  uint16_t hwColor = (!isStudyMode) ? 0x07E0 : BOX_COLOR;
  tft.fillRoundRect(10, 10, 120, 40, 8, hwColor);
  tft.setCursor(20, 20);
  tft.print("Homework");

  // Study button
  uint16_t stColor = (isStudyMode) ? 0x07E0 : BOX_COLOR;
  tft.fillRoundRect(140, 10, 120, 40, 8, stColor);
  tft.setCursor(165, 20);
  tft.print("Study");

  int cx = tft.width()/2;
  int cy = tft.height()/2;

  unsigned long remaining = isStudyMode ? studyRemaining : timerRemaining;
  unsigned long total = isStudyMode ? 
    (studyMode ? STUDY_DURATION : BREAK_DURATION) : TIMER_DURATION;

  float progress = (float)remaining / total;

  int r = 100;

  for(int i=0;i<120;i++){
    float angle = -PI/2 - (2*PI*i/120);

    int x1 = cx + cos(angle)*r;
    int y1 = cy + sin(angle)*r;

    uint16_t color = (i < progress*120) ? 0x07E0 : BOX_COLOR;
    tft.fillCircle(x1,y1,2,color);
  }

  // TITLE
  const char* title;

  if(isStudyMode){
    title = studyMode ? "Study Time" : "Break Time";
  } else {
    title = "Homework Time";
  }

  tft.setTextSize(2);
  tft.setCursor(cx - 60, cy - 80);
  tft.print(title);

  // TIME
  int seconds = remaining / 1000;
  int minutes = seconds / 60;
  seconds %= 60;

  char buf[10];
  sprintf(buf,"%02d:%02d",minutes,seconds);

  tft.setTextSize(4);
  tft.setCursor(cx - 60, cy - 20);
  tft.print(buf);

  tft.updateScreen();
}

void handleCombinedTimerTouch(){

  if(!touchController.read_td_status()) return;

  uint16_t x=touchController.read_touch1_x();
  uint16_t y=touchController.read_touch1_y();

  uint16_t newX = tft.width() - map(y,0,480,0,tft.width());
  uint16_t newY = map(x,0,320,0,tft.height());

  if(backPressed(newX,newY)){
    currentPage = HOME_PAGE;
    needsRedraw = true;
    
    return;
  }

  // Homework button
  if(newX>=10 && newX<=130 && newY>=10 && newY<=50){
    isStudyMode = false;
    needsRedraw = true;
    return;
  }

  // Study button
  if(newX>=140 && newX<=260 && newY>=10 && newY<=50){
    isStudyMode = true;
    needsRedraw = true;
    return;
  }
}


void drawTimerPage(){

  tft.fillScreen(BG_COLOR);
  drawBackButton();

  int cx = tft.width()/2;
  int cy = tft.height()/2 - 40;

  float progress = (float)timerRemaining / TIMER_DURATION;

  // Thin circle
  int r = 100;

  for(int i=0;i<120;i++){
    float angle = -PI/2 - (2*PI*i/120);

    int x1 = cx + cos(angle)*r;
    int y1 = cy + sin(angle)*r;

    uint16_t color = (i < progress*120) ? 0x07E0 : 0xC618;

    tft.fillCircle(x1,y1,2,color);
  }

  // TITLE
  tft.setTextSize(2);
  tft.setTextColor(TEXT_COLOR);
  
  const char* title = "Homework Time";
  
  int16_t tx,ty;
  uint16_t tw,th;
  tft.getTextBounds(title,0,0,&tx,&ty,&tw,&th);
  
  tft.setCursor(cx - tw/2, cy - 35);
  tft.print(title);

  // Time display
  int seconds = timerRemaining / 1000;
  int minutes = seconds / 60;
  seconds = seconds % 60;

  char buf[10];
  sprintf(buf,"%02d:%02d",minutes,seconds);

  tft.setTextSize(4);
  tft.setTextColor(TEXT_COLOR);

  int16_t x1,y1;
  uint16_t w,h;
  tft.getTextBounds(buf,0,0,&x1,&y1,&w,&h);

  tft.setCursor(cx - w/2, cy - h/2);
  tft.print(buf);

  // -------- BUTTONS --------

  // RESET (left)
  tft.fillRoundRect(cx-140, cy+120, 100, 50, 10, 0xC618);
  tft.setCursor(cx-120, cy+135);
  tft.setTextSize(2);
  tft.print("Reset");

  // START / PAUSE (right)
  tft.fillRoundRect(cx+40, cy+120, 100, 50, 10, 0x07E0);
  tft.setCursor(cx+60, cy+135);

  if(!timerRunning) tft.print("Start");
  else if(timerPaused) tft.print("Resume");
  else tft.print("Pause");

  tft.updateScreen();
}

void drawStudyTimerPage(){

  tft.fillScreen(BG_COLOR);
  drawBackButton();

  int cx = tft.width()/2;
  int cy = tft.height()/2 - 40;

  unsigned long total = studyMode ? STUDY_DURATION : BREAK_DURATION;
  float progress = (float)studyRemaining / total;

  // circle
  int r = 100;

  for(int i=0;i<120;i++){
    float angle = -PI/2 - (2*PI*i/120);

    int x1 = cx + cos(angle)*r;
    int y1 = cy + sin(angle)*r;

    uint16_t color = (i < progress*120) ? 0x07E0 : 0xC618;

    tft.fillCircle(x1,y1,2,color);
  }

  // TITLE
  tft.setTextSize(2);
  tft.setTextColor(TEXT_COLOR);

  const char* title = studyMode ? "Study Time" : "Break Time";

  int16_t tx,ty;
  uint16_t tw,th;
  tft.getTextBounds(title,0,0,&tx,&ty,&tw,&th);

  tft.setCursor(cx - tw/2, cy - 35);
  tft.print(title);

  // TIME
  int seconds = studyRemaining / 1000;
  int minutes = seconds / 60;
  seconds %= 60;

  char buf[10];
  sprintf(buf,"%02d:%02d",minutes,seconds);

  tft.setTextSize(4);

  int16_t x1,y1;
  uint16_t w,h;
  tft.getTextBounds(buf,0,0,&x1,&y1,&w,&h);

  tft.setCursor(cx - w/2, cy - h/2);
  tft.print(buf);

  // RESET
  tft.fillRoundRect(cx-140, cy+120, 100, 50, 10, 0xC618);
  tft.setCursor(cx-120, cy+135);
  tft.setTextSize(2);
  tft.print("Reset");

  // START / PAUSE
  tft.fillRoundRect(cx+40, cy+120, 100, 50, 10, 0x07E0);
  tft.setCursor(cx+60, cy+135);

  if(!studyRunning) tft.print("Start");
  else if(studyPaused) tft.print("Resume");
  else tft.print("Pause");

  tft.updateScreen();
}

void updateTimer(){

  if(timerRunning && !timerPaused){
    unsigned long now = millis();
    unsigned long delta = now - lastUpdate;
    lastUpdate = now;

    if(timerRemaining > delta) timerRemaining -= delta;
    else{
      timerRemaining = 0;
      timerRunning = false;
    }
  }
}

void updateStudyTimer(){

  if(studyRunning && !studyPaused){
    unsigned long now = millis();
    unsigned long delta = now - studyLastUpdate;
    studyLastUpdate = now;

    if(studyRemaining > delta){
      studyRemaining -= delta;
    }
    else{
      // SWITCH MODES
      studyMode = !studyMode;

      studyRemaining = studyMode ? STUDY_DURATION : BREAK_DURATION;
    }
  }
}

void handleTimerTouch(){

  if(!touchController.read_td_status()) return;

  uint16_t x=touchController.read_touch1_x();
  uint16_t y=touchController.read_touch1_y();

  uint16_t newX = tft.width() - map(y,0,480,0,tft.width());
  uint16_t newY = map(x,0,320,0,tft.height());

  if(backPressed(newX,newY)){
    currentPage = HOME_PAGE;
    needsRedraw = true;
    
    return;
  }

  int cx = tft.width()/2;
  int cy = tft.height()/2 - 40;

  // RESET
  if(newX >= cx-140 && newX <= cx-40 &&
     newY >= cy+120 && newY <= cy+170){

    timerRunning = false;
    timerPaused = false;
    timerRemaining = TIMER_DURATION;
    needsRedraw = true;
    
    return;
  }

  // START / PAUSE
  if(newX >= cx+40 && newX <= cx+140 &&
     newY >= cy+120 && newY <= cy+170){

    if(!timerRunning){
      timerRunning = true;
      timerPaused = false;
      lastUpdate = millis();
    }
    else if(timerPaused){
      timerPaused = false;
      lastUpdate = millis();
    }
    else{
      timerPaused = true;
    }

    needsRedraw = true;
    
    return;
  }
}

void handleStudyTouch(){

  if(!touchController.read_td_status()) return;

  uint16_t x=touchController.read_touch1_x();
  uint16_t y=touchController.read_touch1_y();

  uint16_t newX = tft.width() - map(y,0,480,0,tft.width());
  uint16_t newY = map(x,0,320,0,tft.height());

  if(backPressed(newX,newY)){
    currentPage = HOME_PAGE;
    needsRedraw = true;
    
    return;
  }

  int cx = tft.width()/2;
  int cy = tft.height()/2 - 40;

  // RESET
  if(newX >= cx-140 && newX <= cx-40 &&
     newY >= cy+120 && newY <= cy+170){

    studyRunning = false;
    studyPaused = false;
    studyMode = true;
    studyRemaining = STUDY_DURATION;

    needsRedraw = true;
    
    return;
  }

  // START / PAUSE
  if(newX >= cx+40 && newX <= cx+140 &&
     newY >= cy+120 && newY <= cy+170){

    if(!studyRunning){
      studyRunning = true;
      studyPaused = false;
      studyLastUpdate = millis();
    }
    else if(studyPaused){
      studyPaused = false;
      studyLastUpdate = millis();
    }
    else{
      studyPaused = true;
    }

    needsRedraw = true;
    
    return;
  }
}

// ============================================================
// MEAL PAGE
// ============================================================
// MEAL TRACKER — FULL REWRITE
// ============================================================

// ---------------- WEIGH STATE VARIABLES ----------------
WeighStep weighStep = WEIGH_WAIT_PLATE;

float capturedFullWeight  = 0;
float capturedEmptyWeight = 0;
float finalFoodWeight     = 0;

unsigned long countdownStart = 0;
const unsigned long COUNTDOWN_MS = 3000;

// Scale offset compensation (subtract this from all readings)
const float SCALE_OFFSET = 2.95; // lbs - the default value when scale is unplugged

// stable detection (reused across both phases)
float  weighStableRef   = 0;
unsigned long weighStableStart = 0;
const float  WEIGH_STABLE_THRESHOLD = 0.02; // lbs
const unsigned long WEIGH_STABLE_MS = 1200;

// ---------------- FOOD SELECTION VARIABLES ----------------
FoodCategory selectedCategory = CAT_NONE;
int          selectedFoodIdx  = -1;

// ---- FOOD LISTS (edit these to match your habits) ----
const char* breakfastFoods[] = {
  "Yogurt + Frozen Fruit",
  "Scramlbed Eggs",
  "Cereal",
  "Protein Shake",
  "Pancakes",
  "Smoothie",
  "Coffee"
};
const int BREAKFAST_COUNT = sizeof(breakfastFoods) / sizeof(breakfastFoods[0]);

const char* lunchFoods[] = {
  "Chicken Quesadilla",
  "Chicken Sandwich",
  "Salad",
  "Soup",
  "Pasta",
  "Burrito Bowl",
  "Leftovers",
  "Chicken Wrap"
};
const int LUNCH_COUNT = sizeof(lunchFoods) / sizeof(lunchFoods[0]);

const char* dinnerFoods[] = {
  "Chicken + Veg",
  "Steak + Sides",
  "Salmon + Rice",
  "Pasta",
  "Stir Fry",
  "Tacos",
  "Pizza",
  "Burger"
};
const int DINNER_COUNT = sizeof(dinnerFoods) / sizeof(dinnerFoods[0]);

const char* snackFoods[] = {
  "Protein Bar",
  "Fruit",
  "Nuts",
  "Greek Yogurt",
  "Rice Cakes",
  "Cheese",
  "Crackers",
  "PB + Apple"
};
const int SNACK_COUNT = sizeof(snackFoods) / sizeof(snackFoods[0]);

// helpers
const char** getFoodList(FoodCategory cat) {
  if (cat == CAT_BREAKFAST) return breakfastFoods;
  if (cat == CAT_LUNCH)     return lunchFoods;
  if (cat == CAT_DINNER)    return dinnerFoods;
  if (cat == CAT_SNACK)     return snackFoods;
  return nullptr;
}

int getFoodCount(FoodCategory cat) {
  if (cat == CAT_BREAKFAST) return BREAKFAST_COUNT;
  if (cat == CAT_LUNCH)     return LUNCH_COUNT;
  if (cat == CAT_DINNER)    return DINNER_COUNT;
  if (cat == CAT_SNACK)     return SNACK_COUNT;
  return 0;
}

const char* getCatName(FoodCategory cat) {
  if (cat == CAT_BREAKFAST) return "Breaky";
  if (cat == CAT_LUNCH)     return "Lunch";
  if (cat == CAT_DINNER)    return "Dinner";
  if (cat == CAT_SNACK)     return "Snack";
  return "Unknown";
}

// ---------------- MEAL LOG ----------------
struct MealLogEntry {
  String foodName;   // "Unnamed" if skipped
  float  weightLbs;
  String timeStr;    // "08:32 AM"
  bool   valid;
};

const int MAX_MEAL_LOG = 10; // store up to 10 in memory today
MealLogEntry mealLog[MAX_MEAL_LOG];
int mealLogCount = 0;

// ============================================================
// SAVE / LOAD MEALS
// ============================================================

String getTimeString() {
  DateTime now = rtc.now();
  int h = now.hour();
  bool isPM = (h >= 12);
  h = h % 12;
  if (h == 0) h = 12;
  char buf[12];
  sprintf(buf, "%02d:%02d %s", h, now.minute(), isPM ? "PM" : "AM");
  return String(buf);
}

void saveMealEntry(const String& foodName, float weightLbs, const String& timeStr) {

  String today = getDateString();

  String newLine = today;
  newLine += ",";
  newLine += timeStr;
  newLine += ",";
  newLine += foodName;
  newLine += ",";

  char wbuf[10];
  dtostrf(weightLbs, 4, 2, wbuf);
  newLine += String(wbuf);

  File f = SD.open("meals2.csv", FILE_WRITE);
  if (f) {
    f.println(newLine);
    f.close();
  }
}

void loadTodayMealsFromCSV2() {

  mealLogCount = 0;

  File f = SD.open("meals2.csv", FILE_READ);
  if (!f) return;

  String today = getDateString();

  // temp buffer — read all today's lines
  String lines[MAX_MEAL_LOG];
  int lineCount = 0;

  while (f.available() && lineCount < MAX_MEAL_LOG) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;
    if (!line.startsWith(today)) continue;
    lines[lineCount++] = line;
  }
  f.close();

  // parse newest-first (reverse order)
  for (int i = lineCount - 1; i >= 0 && mealLogCount < MAX_MEAL_LOG; i--) {
    String& line = lines[i];

    // format: date,time,name,weight
    int c1 = line.indexOf(',');
    int c2 = line.indexOf(',', c1 + 1);
    int c3 = line.indexOf(',', c2 + 1);

    if (c1 == -1 || c2 == -1 || c3 == -1) continue;

    MealLogEntry& e = mealLog[mealLogCount++];
    e.timeStr   = line.substring(c1 + 1, c2);
    e.foodName  = line.substring(c2 + 1, c3);
    e.weightLbs = line.substring(c3 + 1).toFloat();
    e.valid     = true;
  }
}

void deleteMealEntry(int idx) {
  // Rebuild file without that entry.
  // mealLog is newest-first, file is oldest-first, so we match by time+name+weight.

  String today = getDateString();
  String skipTime = mealLog[idx].timeStr;
  String skipName = mealLog[idx].foodName;

  File f = SD.open("meals2.csv", FILE_READ);
  String output = "";

  if (f) {
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) continue;

      if (line.startsWith(today)) {
        // check if this is the one to delete
        int c1 = line.indexOf(',');
        int c2 = line.indexOf(',', c1 + 1);
        int c3 = line.indexOf(',', c2 + 1);

        String t = line.substring(c1 + 1, c2);
        String n = line.substring(c2 + 1, c3);

        if (t == skipTime && n == skipName) continue; // skip this one
      }

      output += line + "\n";
    }
    f.close();
  }

  SD.remove("meals2.csv");
  f = SD.open("meals2.csv", FILE_WRITE);
  if (f) {
    f.print(output);
    f.close();
  }

  loadTodayMealsFromCSV2(); // reload
}

// ============================================================
// WEIGH PAGE (full rewrite)
// ============================================================

void resetWeighFlow() {
  weighStep         = WEIGH_WAIT_PLATE;
  capturedFullWeight  = 0;
  capturedEmptyWeight = 0;
  finalFoodWeight     = 0;
  countdownStart      = 0;
  weighStableRef      = 0;
  weighStableStart    = 0;
}

// Returns true when weight has been stable for WEIGH_STABLE_MS
bool weighIsStable(float w) {
  if (abs(w - weighStableRef) > WEIGH_STABLE_THRESHOLD) {
    weighStableRef   = w;
    weighStableStart = millis();
    return false;
  }
  return (millis() - weighStableStart) >= WEIGH_STABLE_MS;
}

// Draw the circular countdown arc (0.0–1.0 fill)
void drawCountdownCircle(int cx, int cy, int r, float progress) {
  int segments = 60;
  int filled   = (int)(progress * segments);

  for (int i = 0; i < segments; i++) {
    float angle = -PI / 2 + (2 * PI * i / segments);
    int x1 = cx + cos(angle) * r;
    int y1 = cy + sin(angle) * r;

    uint16_t color = (i < filled) ? 0x07E0 : BOX_COLOR;
    tft.fillCircle(x1, y1, 4, color);
  }
}

void drawWeighPage2() {

  tft.fillScreen(BG_COLOR);
  drawBackButton();

  tft.setTextColor(TEXT_COLOR);
  tft.setTextSize(3);
  tft.setCursor(10, 10);
  tft.print("Weigh Meal");

  int cx = tft.width() / 2;
  int cy = 160;

  // ---- STATUS MESSAGE ----
  tft.setTextSize(2);
  const char* msg = "";

  if (weighStep == WEIGH_WAIT_PLATE) {
    msg = "Place plate + food";
  } else if (weighStep == WEIGH_COUNTDOWN_FULL) {
    msg = "Hold still...";
  } else if (weighStep == WEIGH_WAIT_EMPTY) {
    msg = "Eat your food, then";
    tft.setCursor(cx - 110, cy - 80);
    tft.print(msg);
    tft.setCursor(cx - 120, cy - 55);
    tft.print("place empty plate");
    goto skipMsg;
  } else if (weighStep == WEIGH_WAIT_RETURN) {
    msg = "Waiting for plate...";
  } else if (weighStep == WEIGH_COUNTDOWN_EMPTY) {
    msg = "Hold still...";
  }

  {
    int16_t x1, y1; uint16_t w, h;
    tft.getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(cx - w / 2, cy - 80);
    tft.print(msg);
  }

  skipMsg:

  // ---- WEIGHT DISPLAY ----
  // Apply offset compensation
  float displayWeight = currentWeight - SCALE_OFFSET;
  
  char wbuf[20];
  
  // Check if scale is unplugged (reading is around -2.96 after offset)
  if (displayWeight < -2.90 && displayWeight > -3.00) {
    sprintf(wbuf, "Plug in scale");
    tft.setTextSize(2);
  } else {
    sprintf(wbuf, "%.2f lb", displayWeight);
    tft.setTextSize(4);
  }

  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(wbuf, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(cx - w / 2, cy - 20);
  tft.print(wbuf);

  // ---- COUNTDOWN CIRCLE ----
  if (weighStep == WEIGH_COUNTDOWN_FULL || weighStep == WEIGH_COUNTDOWN_EMPTY) {
    unsigned long elapsed = millis() - countdownStart;
    float progress = (float)elapsed / COUNTDOWN_MS;
    if (progress > 1.0) progress = 1.0;
    drawCountdownCircle(cx, cy + 70, 35, progress);
  }

  // ---- CAPTURED WEIGHTS ----
  if (weighStep >= WEIGH_WAIT_EMPTY) {
    tft.setTextSize(2);
    char buf2[30];
    sprintf(buf2, "Total: %.2f lb", capturedFullWeight);
    tft.setCursor(20, cy + 60);
    tft.print(buf2);
  }

  tft.updateScreen();
}

void updateWeighFlow() {

  if (!scaleConnected) return;

  // Apply offset compensation to current weight
  float compensatedWeight = currentWeight - SCALE_OFFSET;

  switch (weighStep) {

    // ---- STEP 1: wait for plate+food to settle ----
    case WEIGH_WAIT_PLATE:
      if (compensatedWeight > 0.1 && weighIsStable(compensatedWeight)) {
        countdownStart = millis();
        weighStep      = WEIGH_COUNTDOWN_FULL;
      }
      break;

    // ---- STEP 2: countdown, then capture ----
    case WEIGH_COUNTDOWN_FULL:
      if (millis() - countdownStart >= COUNTDOWN_MS) {
        capturedFullWeight = compensatedWeight;
        weighStep          = WEIGH_WAIT_EMPTY;
        weighStableRef     = 0; // reset stable tracking
        weighStableStart   = millis();
      }
      break;

    // ---- STEP 3: wait for scale to go near zero (plate removed) ----
    case WEIGH_WAIT_EMPTY:
      if (compensatedWeight < 0.05 && weighIsStable(compensatedWeight)) {
        weighStep = WEIGH_WAIT_RETURN;
      }
      break;

    // ---- STEP 4: wait for empty plate to return ----
    case WEIGH_WAIT_RETURN:
      if (compensatedWeight > 0.1 && weighIsStable(compensatedWeight)) {
        countdownStart = millis();
        weighStep      = WEIGH_COUNTDOWN_EMPTY;
      }
      break;

    // ---- STEP 5: countdown, then capture empty plate ----
    case WEIGH_COUNTDOWN_EMPTY:
      if (millis() - countdownStart >= COUNTDOWN_MS) {
        capturedEmptyWeight = compensatedWeight;
        finalFoodWeight     = capturedFullWeight - capturedEmptyWeight;
        if (finalFoodWeight < 0) finalFoodWeight = 0;
        weighStep = WEIGH_DONE;
      }
      break;

    case WEIGH_DONE:
      // auto-advance to food selection page
      selectedCategory = CAT_NONE;
      selectedFoodIdx  = -1;
      currentPage      = FOOD_PAGE;
      needsRedraw      = true;
      break;
  }
}

void handleWeighTouch2() {

  if (!touchController.read_td_status()) return;

  uint16_t x = touchController.read_touch1_x();
  uint16_t y = touchController.read_touch1_y();

  uint16_t newX = tft.width() - map(y, 0, 480, 0, tft.width());
  uint16_t newY = map(x, 0, 320, 0, tft.height());

  if (backPressed(newX, newY)) {
    // log weight-only entry (no name)
    if (capturedFullWeight > 0 || finalFoodWeight > 0) {
      float logWeight = (finalFoodWeight > 0) ? finalFoodWeight : capturedFullWeight;
      String t = getTimeString();
      saveMealEntry("Unnamed", logWeight, t);
      loadTodayMealsFromCSV2();
    }
    resetWeighFlow();
    currentPage = MEAL_PAGE;
    needsRedraw = true;
  }
}

// ============================================================
// FOOD SELECTION PAGE
// ============================================================

void drawFoodPage() {

  tft.fillScreen(BG_COLOR);
  drawBackButton();

  tft.setTextColor(TEXT_COLOR);
  tft.setTextSize(2);

  // ---- WEIGHT CONFIRMED BANNER ----
  char banner[30];
  sprintf(banner, "Food: %.2f lb", finalFoodWeight);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print(banner);

  // ---- CATEGORY PICKER ----
  if (selectedCategory == CAT_NONE) {

    tft.setTextSize(3);
    tft.setCursor(10, 45);
    tft.print("What meal?");

    const char* catLabels[4] = { "Breaky", "Lunch", "Dinner", "Snack" };
    uint16_t catColors[4]    = { 0xFEA0, 0x07FF, 0xFD20, 0x801F };

    int pad  = 15;
    int btnW = (tft.width() - pad * 5) / 4;
    int btnH = 60;
    int y    = 100;

    for (int i = 0; i < 4; i++) {
      int x = pad + i * (btnW + pad);
      tft.fillRoundRect(x, y, btnW, btnH, 10, catColors[i]);

      tft.setTextSize(2);
      tft.setTextColor(BG_COLOR);

      int16_t x1, y1; uint16_t w, h;
      tft.getTextBounds(catLabels[i], 0, 0, &x1, &y1, &w, &h);
      tft.setCursor(x + (btnW - w) / 2, y + (btnH - h) / 2);
      tft.print(catLabels[i]);
    }

    tft.setTextColor(TEXT_COLOR);

  } else {

    // ---- FOOD LIST FOR CATEGORY ----
    tft.setTextSize(2);
    tft.setTextColor(TEXT_COLOR);

    // Back-to-categories button
    tft.fillRoundRect(10, 40, 140, 36, 8, BOX_COLOR);
    tft.setCursor(20, 50);
    tft.print("< Category");

    // Category name
    tft.setTextSize(2);
    tft.setCursor(165, 50);
    tft.print(getCatName(selectedCategory));

    const char** foods = getFoodList(selectedCategory);
    int count          = getFoodCount(selectedCategory);

    // 4 per row grid
    int cols    = 4;
    int pad     = 10;
    int btnW    = (tft.width() - pad * (cols + 1)) / cols;
    int btnH    = 45;
    int startY  = 90;
    int rows    = (count + cols - 1) / cols;

    for (int i = 0; i < count; i++) {
      int row = i / cols;
      int col = i % cols;

      int x = pad + col * (btnW + pad);
      int y = startY + row * (btnH + pad);

      uint16_t bg = (selectedFoodIdx == i) ? 0x07E0 : BOX_COLOR;
      tft.fillRoundRect(x, y, btnW, btnH, 8, bg);

      tft.setTextColor(TEXT_COLOR);
      tft.setTextSize(1);

      int16_t x1, y1; uint16_t w, h;
      tft.getTextBounds(foods[i], 0, 0, &x1, &y1, &w, &h);

      // Wrap if needed — simple 2-line split at '+'
      String label = String(foods[i]);
      int plusIdx  = label.indexOf('+');

      if (plusIdx != -1 && w > btnW - 4) {
        String l1 = label.substring(0, plusIdx);
        String l2 = label.substring(plusIdx);
        l1.trim(); l2.trim();

        tft.getTextBounds(l1.c_str(), 0, 0, &x1, &y1, &w, &h);
        tft.setCursor(x + (btnW - w) / 2, y + 8);
        tft.print(l1);

        tft.getTextBounds(l2.c_str(), 0, 0, &x1, &y1, &w, &h);
        tft.setCursor(x + (btnW - w) / 2, y + 22);
        tft.print(l2);
      } else {
        tft.setCursor(x + (btnW - w) / 2, y + (btnH - h) / 2);
        tft.print(foods[i]);
      }

      tft.setTextColor(TEXT_COLOR);
    }

    // CONFIRM button (only if food selected)
    if (selectedFoodIdx != -1) {
      tft.fillRoundRect(tft.width() / 2 - 70, tft.height() - 55, 140, 45, 10, 0x07E0);
      tft.setTextSize(2);
      tft.setTextColor(0x0000);
      tft.setCursor(tft.width() / 2 - 28, tft.height() - 40);
      tft.print("Log It");
      tft.setTextColor(TEXT_COLOR);
    }
  }

  tft.updateScreen();
}

void handleFoodTouch() {

  if (!touchController.read_td_status()) return;

  uint16_t x = touchController.read_touch1_x();
  uint16_t y = touchController.read_touch1_y();

  uint16_t newX = tft.width() - map(y, 0, 480, 0, tft.width());
  uint16_t newY = map(x, 0, 320, 0, tft.height());

  // ---- BACK BUTTON ----
  if (backPressed(newX, newY)) {
    // log unnamed and go to meal page
    saveMealEntry("Unnamed", finalFoodWeight, getTimeString());
    loadTodayMealsFromCSV2();
    resetWeighFlow();
    currentPage = MEAL_PAGE;
    needsRedraw = true;
    return;
  }

  // ---- CATEGORY PICKER VIEW ----
  if (selectedCategory == CAT_NONE) {

    int pad  = 15;
    int btnW = (tft.width() - pad * 5) / 4;
    int btnH = 60;
    int y    = 100;

    for (int i = 0; i < 4; i++) {
      int x = pad + i * (btnW + pad);
      if (newX >= x && newX <= x + btnW &&
          newY >= y && newY <= y + btnH) {
        selectedCategory = (FoodCategory)i;
        selectedFoodIdx  = -1;
        needsRedraw      = true;
        return;
      }
    }
    return;
  }

  // ---- FOOD LIST VIEW ----

  // Back-to-categories
  if (newX >= 10 && newX <= 150 && newY >= 40 && newY <= 76) {
    selectedCategory = CAT_NONE;
    selectedFoodIdx  = -1;
    needsRedraw      = true;
    return;
  }

  // Food grid
  int cols   = 4;
  int pad    = 10;
  int btnW   = (tft.width() - pad * (cols + 1)) / cols;
  int btnH   = 45;
  int startY = 90;
  int count  = getFoodCount(selectedCategory);

  for (int i = 0; i < count; i++) {
    int row = i / cols;
    int col = i % cols;

    int bx = pad + col * (btnW + pad);
    int by = startY + row * (btnH + pad);

    if (newX >= bx && newX <= bx + btnW &&
        newY >= by && newY <= by + btnH) {
      selectedFoodIdx = (selectedFoodIdx == i) ? -1 : i; // toggle
      needsRedraw     = true;
      return;
    }
  }

  // Confirm / Log It
  if (selectedFoodIdx != -1) {
    if (newX >= tft.width() / 2 - 70 && newX <= tft.width() / 2 + 70 &&
        newY >= tft.height() - 55    && newY <= tft.height() - 10) {

      const char** foods = getFoodList(selectedCategory);
      String name = String(getCatName(selectedCategory)) + ": " + String(foods[selectedFoodIdx]);
      String t    = getTimeString();

      saveMealEntry(name, finalFoodWeight, t);
      loadTodayMealsFromCSV2();

      resetWeighFlow();
      currentPage = MEAL_PAGE;
      needsRedraw = true;
    }
  }
}

// ============================================================
// MEAL TRACKER HOME PAGE (full rewrite)
// ============================================================

// Edit state for a tapped log entry
int  editMealIdx     = -1; // which meal is being edited (-1 = none)
bool editDeleteMode  = false;

// Manual weight entry
float manualWeight = 1.0; // default 1.0 lb

void drawMealPage2() {

  tft.fillScreen(BG_COLOR);
  drawBackButton();

  tft.setTextColor(TEXT_COLOR);
  tft.setTextSize(3);
  tft.setCursor(10, 10);
  tft.print("Meal Tracker");

  // ---- LOG MEAL BUTTON ----
  tft.fillRoundRect(10, 50, 160, 46, 10, 0x07E0);
  tft.setTextSize(2);
  tft.setTextColor(0x0000);
  tft.setCursor(25, 65);
  tft.print("+ Log Meal");
  tft.setTextColor(TEXT_COLOR);

  // ---- PREVIOUS MEAL BUTTON ----
  tft.fillRoundRect(200, 50, 210, 46, 10, 0x07FF);
  tft.setTextSize(2);
  tft.setTextColor(0x0000);
  tft.setCursor(215, 65);
  tft.print("+ Previous Meal");
  tft.setTextColor(TEXT_COLOR);

  // ---- MEAL LOG ENTRIES (newest on top, max 4 visible) ----
  int cardX      = 10;
  int cardY      = 108;
  int cardW      = tft.width() - 20;
  int cardH      = 48;
  int cardSpacing = 54;

  int show = min(mealLogCount, 4);

  for (int i = 0; i < show; i++) {
    MealLogEntry& e = mealLog[i];

    bool isEditing = (editMealIdx == i);

    uint16_t cardColor = isEditing ? 0xFD20 : BOX_COLOR;
    tft.fillRoundRect(cardX, cardY, cardW, cardH, 8, cardColor);

    // food name (truncated if needed)
    tft.setTextSize(2);
    tft.setTextColor(TEXT_COLOR);
    tft.setCursor(cardX + 8, cardY + 6);

    String name = e.foodName;
    tft.print(name);

    // weight + time on second line
    tft.setTextSize(1);
    char sub[40];
    sprintf(sub, "%.2f lb  |  %s", e.weightLbs, e.timeStr.c_str());
    tft.setCursor(cardX + 8, cardY + 30);
    tft.print(sub);

    // If this card is selected — show Delete button on the right
    if (isEditing) {
      tft.fillRoundRect(cardX + cardW - 70, cardY + 8, 62, 32, 6, 0xF800);
      tft.setTextSize(1);
      tft.setTextColor(0xFFFF);
      tft.setCursor(cardX + cardW - 58, cardY + 18);
      tft.print("Delete");
      tft.setTextColor(TEXT_COLOR);
    }

    cardY += cardSpacing;
  }

  tft.updateScreen();
}

void handleMealTouch2() {

  if (!touchController.read_td_status()) return;

  uint16_t x = touchController.read_touch1_x();
  uint16_t y = touchController.read_touch1_y();

  uint16_t newX = tft.width() - map(y, 0, 480, 0, tft.width());
  uint16_t newY = map(x, 0, 320, 0, tft.height());

  if (backPressed(newX, newY)) {
    editMealIdx = -1;
    currentPage = HOME_PAGE;
    needsRedraw = true;
    return;
  }

  // LOG MEAL BUTTON
  if (newX >= 10 && newX <= 210 && newY >= 50 && newY <= 96) {
    editMealIdx = -1;
    resetWeighFlow();
    currentPage = WEIGH_PAGE;
    needsRedraw = true;
    return;
  }

  // PREVIOUS MEAL BUTTON (manual weight entry)
  if (newX >= 220 && newX <= 460 && newY >= 50 && newY <= 96) {
    editMealIdx = -1;
    manualWeight = 1.0; // reset to default
    currentPage = MANUAL_WEIGHT_PAGE;
    needsRedraw = true;
    return;
  }

  // MEAL CARDS
  int cardX      = 10;
  int cardY      = 108;
  int cardW      = tft.width() - 20;
  int cardH      = 48;
  int cardSpacing = 54;

  int show = min(mealLogCount, 4);

  for (int i = 0; i < show; i++) {

    if (newX >= cardX && newX <= cardX + cardW &&
        newY >= cardY && newY <= cardY + cardH) {

      if (editMealIdx == i) {
        // already selected — check if tapped delete button
        if (newX >= cardX + cardW - 70 && newX <= cardX + cardW - 8 &&
            newY >= cardY + 8          && newY <= cardY + 40) {
          deleteMealEntry(i);
          editMealIdx = -1;
          needsRedraw = true;
          return;
        }
        // tapped again = deselect
        editMealIdx = -1;
      } else {
        editMealIdx = i;
      }

      needsRedraw = true;
      return;
    }

    cardY += cardSpacing;
  }

  // tapped outside cards = deselect
  if (editMealIdx != -1) {
    editMealIdx = -1;
    needsRedraw = true;
  }
}
// ============================================================
// MANUAL WEIGHT ENTRY PAGE
// ============================================================

void drawManualWeightPage() {
  tft.fillScreen(BG_COLOR);
  drawBackButton();

  tft.setTextColor(TEXT_COLOR);
  tft.setTextSize(3);
  tft.setCursor(10, 10);
  tft.print("Enter Weight");

  int cx = tft.width() / 2;
  int cy = tft.height() / 2;

  // ---- WEIGHT DISPLAY ----
  char wbuf[20];
  sprintf(wbuf, "%.2f lb", manualWeight);
  
  tft.setTextSize(4);
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(wbuf, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(cx - w / 2, cy - 60);
  tft.print(wbuf);

  // ---- SLIDER ----
  int sliderX = 40;
  int sliderY = cy;
  int sliderW = tft.width() - 80;
  int sliderH = 20;

  // Slider track
  tft.fillRoundRect(sliderX, sliderY, sliderW, sliderH, 10, BOX_COLOR);

  // Slider thumb position (0-2 lbs mapped to slider width)
  int thumbX = sliderX + (int)((manualWeight / 2.0) * sliderW);
  tft.fillCircle(thumbX, sliderY + sliderH / 2, 15, 0x07E0);

  // ---- SLIDER LABELS ----
  tft.setTextSize(2);
  tft.setCursor(sliderX - 10, sliderY + 30);
  tft.print("0");
  
  tft.setCursor(sliderX + sliderW - 10, sliderY + 30);
  tft.print("2");

  // ---- LOG WEIGHT BUTTON ----
  tft.fillRoundRect(cx - 80, cy + 80, 160, 50, 10, 0x07E0);
  tft.setTextSize(2);
  tft.setTextColor(0x0000);
  tft.setCursor(cx - 60, cy + 95);
  tft.print("Log Weight");
  tft.setTextColor(TEXT_COLOR);

  tft.updateScreen();
}

void handleManualWeightTouch() {
  if (!touchController.read_td_status()) return;

  uint16_t x = touchController.read_touch1_x();
  uint16_t y = touchController.read_touch1_y();

  uint16_t newX = tft.width() - map(y, 0, 480, 0, tft.width());
  uint16_t newY = map(x, 0, 320, 0, tft.height());

  if (backPressed(newX, newY)) {
    currentPage = MEAL_PAGE;
    needsRedraw = true;
    return;
  }

  int cx = tft.width() / 2;
  int cy = tft.height() / 2;

  // ---- SLIDER INTERACTION ----
  int sliderX = 40;
  int sliderY = cy;
  int sliderW = tft.width() - 80;
  int sliderH = 20;

  if (newY >= sliderY - 20 && newY <= sliderY + sliderH + 20) {
    if (newX >= sliderX && newX <= sliderX + sliderW) {
      // Calculate weight from touch position
      float ratio = (float)(newX - sliderX) / sliderW;
      manualWeight = ratio * 2.0; // 0-2 lbs
      if (manualWeight < 0) manualWeight = 0;
      if (manualWeight > 2.0) manualWeight = 2.0;
      needsRedraw = true;
      return;
    }
  }

  // ---- LOG WEIGHT BUTTON ----
  if (newX >= cx - 80 && newX <= cx + 80 &&
      newY >= cy + 80 && newY <= cy + 130) {
    // Set finalFoodWeight and go to food selection
    finalFoodWeight = manualWeight;
    selectedCategory = CAT_NONE;
    selectedFoodIdx = -1;
    currentPage = FOOD_PAGE;
    needsRedraw = true;
    return;
  }
}


// ===================== SCALE FUNCTIONS (keep existing) =====================

void updateScaleIdle(){

  unsigned long now = millis();

  scale.get_units(1); // minimal read

  unsigned long dt = now - lastReadTime;
  lastReadTime = now;

  // ONLY detect connection
  if(dt < FAST_THRESHOLD){
    scaleConnected = false;
    return;
  }

  if(!scaleConnected){
    delay(500);
    scale.tare();
    scaleConnected = true;
  }
}

void updateScaleActive(){

  unsigned long now = millis();

  scale.get_units(1);

  unsigned long dt = now - lastReadTime;
  lastReadTime = now;

  if(dt < FAST_THRESHOLD){
    scaleConnected = false;
    return;
  }

  if(!scaleConnected){
    delay(500);
    scale.tare();
    scaleConnected = true;
    return;
  }

  // ONLY HERE do heavy read
  currentWeight = scale.get_units(10);
}

// ============================================================
// EXERCISE PAGE
// ============================================================
float loadLastWeightFromCSV(){

  File f = SD.open("exercise.csv", FILE_READ);
  if(!f) return 200.0;

  float lastWeight = 200.0;

  while(f.available()){
    String line = f.readStringUntil('\n');
    line.trim();

    if(line.length() == 0) continue;

    // get last value (weight)
    int lastComma = line.lastIndexOf(',');
    if(lastComma != -1){
      String w = line.substring(lastComma + 1);
      lastWeight = w.toFloat();
    }
  }

  f.close();
  return lastWeight;
}

void loadTodayExerciseFromCSV(){

  File f = SD.open("exercise.csv", FILE_READ);
  if(!f) return;

  String today = getDateString();

  while(f.available()){
    String line = f.readStringUntil('\n');
    line.trim();

    if(!line.startsWith(today)) continue;

    // parse CSV
    int idx = 0;
    int last = 0;

    String values[6];

    for(int i=0;i<6;i++){
      idx = line.indexOf(',', last);
      if(idx == -1) idx = line.length();

      values[i] = line.substring(last, idx);
      last = idx + 1;
    }

    todayExercise.templateType = values[1].toInt();
    todayExercise.minutes      = values[2].toInt();
    todayExercise.effort       = values[3].toInt();
    todayExercise.hunger       = values[4].toInt();
    todayExercise.weight       = values[5].toFloat();
  }

  f.close();
}

void saveExerciseToCSV(){

  String today = getDateString();
  String newLine = today;

  newLine += "," + String(todayExercise.templateType);
  newLine += "," + String(todayExercise.minutes);
  newLine += "," + String(todayExercise.effort);
  newLine += "," + String(todayExercise.hunger);
  newLine += "," + String(todayExercise.weight);

  // ---------- READ OLD ----------
  String output = "";

  File f = SD.open("exercise.csv", FILE_READ);

  if(f){
    while(f.available()){
      String line = f.readStringUntil('\n');
      line.trim();

      if(line.length() == 0) continue;

      // KEEP only lines NOT from today
      if(!line.startsWith(today)){
        output += line + "\n";
      }
    }
    f.close();
  }

  // ---------- ADD NEW ----------
  output += newLine + "\n";

  // ---------- OVERWRITE ----------
  SD.remove("exercise.csv");
  
  f = SD.open("exercise.csv", FILE_WRITE);
  
  if(f){
    f.print(output);
    f.close();
  }
}

void drawExercisePage(){

  tft.fillScreen(BG_COLOR);
  drawBackButton();

  tft.setTextColor(TEXT_COLOR);

  // ---------- TITLE ----------
  tft.setTextSize(3);
  tft.setCursor(10, 10);
  tft.print("Exercise Tracker");

  // ---------- TEMPLATE ----------
  tft.setTextSize(2);
  tft.setCursor(10, 60);
  tft.print("Template:");

  const char* tempLabels[5] = {"A","B","C","D","R"};

  for(int i=0;i<5;i++){
    int x = 140 + i*60;

    uint16_t color = (todayExercise.templateType == i+1) ? 0x07E0 : 0xC618;

    tft.fillRoundRect(x, 50, 50, 40, 8, color);
    tft.setCursor(x+20, 63);
    tft.print(tempLabels[i]);
  }

  // ---------- TIME ----------
  tft.setCursor(10, 120);
  tft.print("Time: (mins)");

  tft.fillRoundRect(180, 105, 50, 40, 8, 0xC618);
  tft.setCursor(200, 120);
  tft.print("-");

  tft.setCursor(260, 115);
  tft.print(todayExercise.minutes);

  tft.fillRoundRect(320, 105, 50, 40, 8, 0xC618);
  tft.setCursor(340, 120);
  tft.print("+");

  // ---------- EFFORT ----------
  tft.setCursor(10, 160);
  tft.print("Effort:");

  for(int i=0;i<=10;i++){
    int x = 110 + i*30;

    uint16_t color = (todayExercise.effort == i) ? 0x07E0 : 0xC618;

    tft.fillRect(x, 160, 25, 25, color);

    tft.setCursor(x+5, 165);
    tft.print(i);
  }

  // ---------- HUNGER ----------
  tft.setCursor(10, 210);
  tft.print("Hunger:");
  
  for(int i=1;i<=5;i++){
    int x = 105 + (i-1)*60;
  
    uint16_t color = (todayExercise.hunger == i) ? 0x07E0 : 0xC618;
  
    tft.fillRoundRect(x, 200, 50, 40, 8, color);
  
    tft.setCursor(x+20, 215);
    tft.print(i);
  }
  
  // ---------- WEIGHT ----------
  tft.setCursor(10, 270);
  tft.print("Weight:");
  
  int baseY = 260;
  
  // -1
  tft.fillRoundRect(105, baseY, 50, 40, 8, 0xC618);
  tft.setCursor(120, baseY+12);
  tft.print("-1");
  
  // -0.1
  tft.fillRoundRect(165, baseY, 60, 40, 8, 0xC618);
  tft.setCursor(170, baseY+12);
  tft.print("-0.1");
  
  // WEIGHT
  char buf[10];
  sprintf(buf,"%.1f",todayExercise.weight);
  tft.setCursor(237, baseY+12);
  tft.print(buf);
  
  // +0.1
  tft.fillRoundRect(305, baseY, 60, 40, 8, 0xC618);
  tft.setCursor(310, baseY+12);
  tft.print("+0.1");
  
  // +1
  tft.fillRoundRect(375, baseY, 50, 40, 8, 0xC618);
  tft.setCursor(390, baseY+12);
  tft.print("+1");

  tft.updateScreen();
}

void handleExerciseTouch(){

  if(!touchController.read_td_status()) return;

  uint16_t x=touchController.read_touch1_x();
  uint16_t y=touchController.read_touch1_y();

  uint16_t newX = tft.width() - map(y,0,480,0,tft.width());
  uint16_t newY = map(x,0,320,0,tft.height());

  // ================= BACK =================
  if(backPressed(newX,newY)){
    saveExerciseToCSV();
    currentPage = HOME_PAGE;
    needsRedraw = true;
    
    return;
  }

  // ================= TEMPLATE =================
  int templateY = 50;

  for(int i=0;i<5;i++){
    int x0 = 140 + i*60;

    if(newX>=x0 && newX<=x0+50 &&
       newY>=templateY && newY<=templateY+40){

      todayExercise.templateType = i+1;
      saveExerciseToCSV();
      needsRedraw = true;
      return;
    }
  }

  // ================= TIME =================
  int timeY = 105;

  // -
  if(newX>=180 && newX<=230 &&
     newY>=timeY && newY<=timeY+40){

    todayExercise.minutes -= 5;
    if(todayExercise.minutes < 0) todayExercise.minutes = 0;

    saveExerciseToCSV();
    needsRedraw = true;
    return;
  }

  // +
  if(newX>=320 && newX<=370 &&
     newY>=timeY && newY<=timeY+40){

    todayExercise.minutes += 5;

    saveExerciseToCSV();
    needsRedraw = true;
    return;
  }

  // ================= EFFORT =================
  int effortY = 160;

  for(int i=0;i<=10;i++){
    int x0 = 110 + i*30;

    if(newX>=x0 && newX<=x0+25 &&
       newY>=effortY && newY<=effortY+25){

      todayExercise.effort = i;
      saveExerciseToCSV();
      needsRedraw = true;
      return;
    }
  }
  
  // ================= HUNGER =================
  int hungerY = 200;
  
  for(int i=1;i<=5;i++){
    int x0 = 120 + (i-1)*60;
  
    if(newX>=x0 && newX<=x0+50 &&
       newY>=hungerY && newY<=hungerY+40){
  
      todayExercise.hunger = i;
      saveExerciseToCSV();
      needsRedraw = true;
      return;
    }
  }

  // ================= WEIGHT =================
  int weightY = 260;

  // -1
  if(newX>=120 && newX<=170 &&
     newY>=weightY && newY<=weightY+40){

    todayExercise.weight -= 1.0;
    saveExerciseToCSV();
    needsRedraw = true;
    return;
  }

  // -0.1
  if(newX>=180 && newX<=240 &&
     newY>=weightY && newY<=weightY+40){

    todayExercise.weight -= 0.1;
    saveExerciseToCSV();
    needsRedraw = true;
    return;
  }

  // +0.1
  if(newX>=310 && newX<=370 &&
     newY>=weightY && newY<=weightY+40){

    todayExercise.weight += 0.1;
    saveExerciseToCSV();
    needsRedraw = true;
    return;
  }

  // +1
  if(newX>=380 && newX<=430 &&
     newY>=weightY && newY<=weightY+40){

    todayExercise.weight += 1.0;
    saveExerciseToCSV();
    needsRedraw = true;
    return;
  }
}

// ============================================================
// STUB PAGE
// ============================================================
void drawStub(const char* title){

  tft.fillScreen(BG_COLOR);
  drawBackButton();

  tft.setTextSize(3);
  tft.setTextColor(TEXT_COLOR);

  int16_t x1,y1;
  uint16_t w,h;
  tft.getTextBounds(title,0,0,&x1,&y1,&w,&h);

  tft.setCursor((tft.width()-w)/2, tft.height()/2);
  tft.print(title);

  tft.updateScreen();
}

void handleStubTouch(){
  if(!touchController.read_td_status()) return;

  uint16_t x=touchController.read_touch1_x();
  uint16_t y=touchController.read_touch1_y();

  uint16_t newX = tft.width() - map(y,0,480,0,tft.width());
  uint16_t newY = map(x,0,320,0,tft.height());

  if(backPressed(newX,newY)){
    currentPage = HOME_PAGE;
    needsRedraw = true;
    
  }
}

// ============================================================
// SETTINGS
// ============================================================

void drawSettingsPage(){

  tft.fillScreen(BG_COLOR);
  drawBackButton();

  tft.setTextSize(3);
  tft.setCursor(10,10);
  tft.print("Settings");

  tft.setTextSize(2);
  tft.setCursor(10,100);
  tft.print("Dark Mode");

  int x = 200;
  int y = 90;

  tft.fillRoundRect(x, y, 120, 50, 25, BOX_COLOR);

  int circleX = darkMode ? (x+80) : (x+20);

  tft.fillCircle(circleX, y+25, 20, 0xFFFF);

  tft.updateScreen();
}

void handleSettingsTouch(){

  if(!touchController.read_td_status()) return;

  uint16_t x=touchController.read_touch1_x();
  uint16_t y=touchController.read_touch1_y();

  uint16_t newX = tft.width() - map(y,0,480,0,tft.width());
  uint16_t newY = map(x,0,320,0,tft.height());

  if(backPressed(newX,newY)){
    currentPage = HOME_PAGE;
    needsRedraw = true;
    
    return;
  }

  if(newX>=200 && newX<=320 && newY>=90 && newY<=140){
    darkMode = !darkMode;
    applyTheme();
    saveThemeToSD();
    needsRedraw = true;
    
  }
}

void saveThemeToSD() {
  File f = SD.open("theme.txt", FILE_WRITE);

  if (f) {
    f.seek(0);
    f.print(darkMode ? "1" : "0");
    f.close();
    Serial.println("Theme saved");
  } else {
    Serial.println("Save failed");
  }
}

void loadThemeFromSD() {
  File f = SD.open("theme.txt", FILE_READ);

  if (f) {
    char c = f.read();
    darkMode = (c == '1');
    f.close();
    Serial.println("Theme loaded");
  } else {
    Serial.println("No theme file, defaulting to DARK");
    darkMode = true; // default
  }

  applyTheme();
}

// is it mornin or naw?1?

bool isMorningTime() {
  DateTime now = rtc.now();
  int h = now.hour();
  return (h >= 3 && h < 15); // 3am → 3pm
}

// clock shit!!

void drawClockPage(){

  tft.fillScreen(BG_COLOR);
  drawBackButton();

  DateTime now = rtc.now();

  // TIME
  char timeBuf[10];
  sprintf(timeBuf,"%02d:%02d:%02d",now.hour(),now.minute(),now.second());

  tft.setTextSize(4);
  tft.setTextColor(TEXT_COLOR);

  int16_t x1,y1;
  uint16_t w,h;
  tft.getTextBounds(timeBuf,0,0,&x1,&y1,&w,&h);

  tft.setCursor((tft.width()-w)/2, tft.height()/2 - 40);
  tft.print(timeBuf);

  // DATE
  char dateBuf[20];
  sprintf(dateBuf,"%02d/%02d/%04d",now.month(),now.day(),now.year());

  tft.setTextSize(2);

  tft.getTextBounds(dateBuf,0,0,&x1,&y1,&w,&h);

  tft.setCursor((tft.width()-w)/2, tft.height()/2 + 20);
  tft.print(dateBuf);

  tft.updateScreen();
}

void handleClockTouch(){
  if(!touchController.read_td_status()) return;

  uint16_t x=touchController.read_touch1_x();
  uint16_t y=touchController.read_touch1_y();

  uint16_t newX = tft.width() - map(y,0,480,0,tft.width());
  uint16_t newY = map(x,0,320,0,tft.height());

  if(backPressed(newX,newY)){
    currentPage = HOME_PAGE;
    needsRedraw = true;
  }
}

// ============================================================
// SERIAL OUTPUT TO PYTHON
// ============================================================

void handleSerialCommands(){

  if(!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if(cmd.startsWith("GET ")){

    String filename = cmd.substring(4);

    File f = SD.open(filename.c_str());

    if(!f){
      Serial.println("ERROR:FILE_NOT_FOUND");
      return;
    }

    Serial.println("START_FILE");

    while(f.available()){
      Serial.write(f.read());
    }

    f.close();

    Serial.println();
    Serial.println("END_FILE");
  }
}

// ============================================================
// SETUP
// ============================================================
void setup(){

  Serial.begin(115200);
  delay(1000);

  pinMode(TFT_LED,OUTPUT);
  digitalWrite(TFT_LED,HIGH);

  Wire.begin();
  
  // RTC INIT
  if (!rtc.begin()) {
    Serial.println("❌ RTC NOT FOUND");
  } else {
    Serial.println("✅ RTC FOUND");
  }
  
  if (rtc.lostPower()) {
    Serial.println("⚠️ RTC LOST POWER — setting time");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // SCALE INIT
  scale.begin(HX_DT, HX_SCK);
  scale.set_scale(CAL_FACTOR);
  
  touchController.begin(Wire,0x38);

  // SD INIT
  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("SD FAILED");
  } else {
    Serial.println("SD OK");
  }

  loadThemeFromSD();

  tft.begin();
  tft.setRotation(3);
  tft.useFrameBuffer(true);

  loadRoutines();
  
  // ---------- LOAD TRACKERS ----------
  todayExercise.weight = loadLastWeightFromCSV();
  
  loadTodayExerciseFromCSV();
  loadTodayMealsFromCSV2();

  randomSeed(analogRead(A0));
  assignColors(morningTasks,MORNING_COUNT);
  assignColors(afternoonTasks,AFTERNOON_COUNT);
  assignColors(nightTasks,NIGHT_COUNT);

  applyTheme();
}

// ============================================================
// LOOP
// ============================================================
void loop(){

  handleSerialCommands();

  checkRoutineReset();

  if(currentPage!=lastPage){
    needsRedraw=true;
    scrollOffset=0;
    lastPage=currentPage;
  }

  if(currentPage==HOME_PAGE){
    handleHomeTouch();
  
    DateTime now = rtc.now();
  
    if(needsRedraw || now.minute() != lastMinute){
      lastMinute = now.minute();
      drawHome();
      needsRedraw=false;
    }
  }

  else if(currentPage==CLOCK_PAGE){

    handleClockTouch();
  
    if(needsRedraw){
      drawClockPage();
      needsRedraw = false;
    }
  }

  else if(currentPage==MORNING_PAGE){
    handleTasks(morningTasks,MORNING_COUNT);
    if(needsRedraw){ drawTasks(morningTasks,MORNING_COUNT,"Morning Routine"); needsRedraw=false; }
  }

  else if(currentPage==AFTERNOON_PAGE){
    handleTasks(afternoonTasks,AFTERNOON_COUNT);
    if(needsRedraw){ drawTasks(afternoonTasks,AFTERNOON_COUNT,"Afternoon Routine"); needsRedraw=false; }
  }

  else if(currentPage==NIGHT_PAGE){
    handleTasks(nightTasks,NIGHT_COUNT);
    if(needsRedraw){ drawTasks(nightTasks,NIGHT_COUNT,"Night Routine"); needsRedraw=false; }
  }

  else if(currentPage==MEAL_PAGE){
    handleMealTouch2();
    if(needsRedraw) {
      drawMealPage2();
      needsRedraw = false;
    }
  }

  else if(currentPage==WEIGH_PAGE){
    updateScaleActive();        // your existing function, keep as-is
    handleWeighTouch2();
    updateWeighFlow();
    drawWeighPage2();           // always redraw (scale data is live)
  }

  else if(currentPage==FOOD_PAGE){
    handleFoodTouch();
    if(needsRedraw) {
      drawFoodPage();
      needsRedraw = false;
    }
  }

  else if(currentPage==MANUAL_WEIGHT_PAGE){
    handleManualWeightTouch();
    if(needsRedraw) {
      drawManualWeightPage();
      needsRedraw = false;
    }
  }

  else if(currentPage==EXERCISE_PAGE){
  
    handleExerciseTouch();
  
    if(needsRedraw){
      drawExercisePage();
      needsRedraw = false;
    }
  }

  else if(currentPage==TIMER_PAGE){
  
    handleCombinedTimerTouch();
    updateTimer();
    updateStudyTimer();
  
    if(needsRedraw || timerRunning || studyRunning){
      drawCombinedTimerPage();
      needsRedraw = false;
    }
  }

  else if(currentPage==SETTINGS_PAGE){
  
    handleSettingsTouch();
  
    if(needsRedraw){
      drawSettingsPage();
      needsRedraw = false;
    }
  }
}

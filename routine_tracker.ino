#include <ILI9488_t3.h>
#include <Wire.h>
#include "RAK14014_FT6336U.h"
#include <SPI.h>

struct Task;

// ---------------- DISPLAY ----------------
#define TFT_CS   9
#define TFT_DC   7
#define TFT_RST  8
#define TFT_LED  6

ILI9488_t3 tft(TFT_CS, TFT_DC, TFT_RST);
FT6336U touchController;

// ---------------- TIME ----------------
elapsedMillis runtime;
elapsedMillis idleTimer;
const unsigned long IDLE_TIMEOUT = 300000;

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
struct MealDay {
  int breakfast = 0; // 0 none, 1 small, 2 med, 3 large
  int lunch = 0;
  int snack = 0;
  int dinner = 0;

  String notes = "";
};

MealDay todayMeals;

// ---------------- EXERCISE TRACKER ----------------
struct ExerciseDay {
  int templateType = 0; // 0 none, 1 A, 2 B, 3 C, 4 Rest

  int minutes = 45;

  int effort = 5;   // 0–10
  int energy = 0;   // -2 to +2
  int hunger = 2;   // 0–5

  float weight = 202.0;

  String notes = "";
};

ExerciseDay todayExercise;

// ---------------- UI STATE ----------------
enum Page { 
  HOME_PAGE, 
  CLOCK_PAGE, 
  MORNING_PAGE, 
  NIGHT_PAGE,
  MEAL_PAGE,
  EXERCISE_PAGE,
  TIMER_PAGE,
  SETTINGS_PAGE
};

Page currentPage = HOME_PAGE;
Page lastPage = HOME_PAGE;


// ---------------- TASK STRUCT ----------------
struct Task {
  const char* label;
  bool done;
  uint16_t color;
};

// ---------------- TASKS ----------------
Task morningTasks[] = {
  {"Go Piss", false, 0},
  {"Brush Teeth", false, 0},
  {"Wash Face", false, 0},
  {"Shave", false, 0},
  {"Sunscreen", false, 0},
  {"Take weight", false, 0},
  {"Breakfast", false, 0}
};

Task nightTasks[] = {
  {"Floss", false, 0},
  {"Brush Teeth", false, 0},
  {"Wash Face", false, 0},
  {"Moisturise", false, 0},
  {"Journal", false, 0},
  {"Make bed", false, 0},
};

const int MORNING_COUNT = sizeof(morningTasks)/sizeof(Task);
const int NIGHT_COUNT   = sizeof(nightTasks)/sizeof(Task);

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

// ============================================================
// DONUT
// ============================================================
void drawDonut(int cx, int cy, int rOuter, int rInner, float progress) {

  tft.fillCircle(cx, cy, rOuter, 0xC618);
  tft.fillCircle(cx, cy, rInner, BG_COLOR);

  int segments = 120;
  int filled = progress * segments;

  for (int i = 0; i < filled; i++) {
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

    tft.fillTriangle(x1,y1,x2,y2,x3,y3,0x07E0);
    tft.fillTriangle(x2,y2,x3,y3,x4,y4,0x07E0);
  }

  tft.setTextSize(3);
  tft.setTextColor(TEXT_COLOR);

  int pct = round(progress * 100);
  char buf[6];
  sprintf(buf,"%d%%",pct);

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

  const char* labels[6] = {
    "Morning Routine","Night Routine","School Timer",
    "Meal Tracker","Exercise Tracker","Settings"
  };

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
      tft.setTextColor(TEXT_COLOR);
      
      // Split into two words
      String full = String(labels[i]);
      int spaceIndex = full.indexOf(' ');
      
      String line1 = full;
      String line2 = "";
      
      if(spaceIndex != -1){
        line1 = full.substring(0, spaceIndex);
        line2 = full.substring(spaceIndex + 1);
      }
      
      // Measure both lines
      int16_t x1,y1;
      uint16_t w1,h1,w2,h2;
      
      tft.getTextBounds(line1.c_str(),0,0,&x1,&y1,&w1,&h1);
      tft.getTextBounds(line2.c_str(),0,0,&x1,&y1,&w2,&h2);
      
      // Vertical centering (two lines)
      int totalHeight = h1 + h2 + 6;
      int startY = y + (boxH - totalHeight)/2;
      
      // Draw line 1
      tft.setCursor(x + (boxW - w1)/2, startY);
      tft.print(line1);
      
      // Draw line 2 (if exists)
      if(line2 != ""){
        tft.setCursor(x + (boxW - w2)/2, startY + h1 + 6);
        tft.print(line2);
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

        if(i==0) currentPage=MORNING_PAGE;
        if(i==1) currentPage=NIGHT_PAGE;
        if(i==2) currentPage=TIMER_PAGE;
        if(i==3) currentPage=MEAL_PAGE;
        if(i==4) currentPage=EXERCISE_PAGE;
        if(i==5) currentPage=SETTINGS_PAGE;

        needsRedraw=true;
        delay(200);
        return;
      }
    }
  }
}

// ============================================================
// TASK PAGE
// ============================================================
void drawTasks(Task tasks[],int count,const char* title){

  tft.fillScreen(BG_COLOR);
  drawBackButton();

  int taskW = tft.width()*0.55;

  int done=0;
  for(int i=0;i<count;i++) if(tasks[i].done) done++;

  float progress = (float)done/count;

  int y=50-scrollOffset;

  if(done<count){
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

  drawDonut(taskW+(tft.width()-taskW)/2,tft.height()/2-40,70,50,progress);


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
    currentPage=HOME_PAGE;
    needsRedraw=true;
    delay(200);
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
    delay(150);
    return;
  }
  
  // DOWN
  if(newX >= cx+15 && newX <= cx+15+45 &&
     newY >= cy+110 && newY <= cy+110+45){
    scrollOffset += 40;
    needsRedraw = true;
    delay(150);
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
        needsRedraw = true;
        delay(150);
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
        needsRedraw = true;
        delay(150);
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
    delay(200);
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
    delay(200);
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
    delay(200);
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
    delay(200);
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
    delay(200);
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
    delay(200);
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
    delay(200);
    return;
  }
}

// ============================================================
// MEAL PAGE
// ============================================================

void drawMealPage(){

  tft.fillScreen(BG_COLOR);
  drawBackButton();

  tft.setTextColor(TEXT_COLOR);

  // ---------- DATE ----------
  tft.setTextSize(3);
  tft.setCursor(10, 10);
  tft.print("March 28"); // (we’ll make dynamic later)

  // ---------- NOTES ----------
  tft.setTextSize(2);
  tft.drawRect(10, 50, tft.width()-20, 50, 0x0000);
  tft.setCursor(15, 65);
  tft.print("Notes...");

  // ---------- MEALS ----------
  const char* labels[4] = {"Breakfast","Lunch","Snack","Dinner"};
  int* values[4] = {
    &todayMeals.breakfast,
    &todayMeals.lunch,
    &todayMeals.snack,
    &todayMeals.dinner
  };

  int startY = 120;

  for(int i=0;i<4;i++){

    int y = startY + i*70;

    // Label
    tft.setCursor(10, y);
    tft.print(labels[i]);

    // Buttons S M L
    for(int j=1;j<=3;j++){

      int x = 160 + (j-1)*70;

      uint16_t color = (*values[i] == j) ? 0x07E0 : 0xC618;

      tft.fillRoundRect(x, y-5, 50, 40, 8, color);

      tft.setCursor(x+15, y+5);

      if(j==1) tft.print("S");
      if(j==2) tft.print("M");
      if(j==3) tft.print("L");
    }
  }

  tft.updateScreen();
}

void handleMealTouch(){

  if(!touchController.read_td_status()) return;

  uint16_t x=touchController.read_touch1_x();
  uint16_t y=touchController.read_touch1_y();

  uint16_t newX = tft.width() - map(y,0,480,0,tft.width());
  uint16_t newY = map(x,0,320,0,tft.height());

  // BACK
  if(backPressed(newX,newY)){
    currentPage = HOME_PAGE;
    needsRedraw = true;
    delay(200);
    return;
  }

  int startY = 120;

  int* values[4] = {
    &todayMeals.breakfast,
    &todayMeals.lunch,
    &todayMeals.snack,
    &todayMeals.dinner
  };

  for(int i=0;i<4;i++){

    int y = startY + i*70;

    for(int j=1;j<=3;j++){

      int x = 160 + (j-1)*70;

      if(newX >= x && newX <= x+50 &&
         newY >= y-5 && newY <= y+35){

        *values[i] = j;

        // 🔥 AUTO SAVE HOOK (we'll implement SD next)
        // saveMealsToSD();

        needsRedraw = true;
        delay(150);
        return;
      }
    }
  }

  // NOTES CLICK (future keyboard)
  if(newY >= 50 && newY <= 100){
    Serial.println("Open keyboard here later");
  }
}


// ============================================================
// EXERCISE PAGE
// ============================================================

void drawExercisePage(){

  tft.fillScreen(BG_COLOR);
  drawBackButton();

  tft.setTextColor(TEXT_COLOR);

  // ---------- TITLE ----------
  tft.setTextSize(3);
  tft.setCursor(10, 10);
  tft.print("Exercise");

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

  // ---------- ENERGY ----------
  tft.setCursor(10, 210);
  tft.print("Energy:");

  int energyVals[5] = {-2,-1,0,+1,+2};

  for(int i=0;i<5;i++){
    int x = 105 + i*60;

    uint16_t color = (todayExercise.energy == energyVals[i]) ? 0x07E0 : 0xC618;

    tft.fillRoundRect(x, 200, 50, 40, 8, color);

    tft.setCursor(x+10, 215);
    tft.print(energyVals[i]);
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
    currentPage = HOME_PAGE;
    needsRedraw = true;
    delay(200);
    return;
  }

  // ================= TEMPLATE =================
  int templateY = 50;

  for(int i=0;i<5;i++){
    int x0 = 140 + i*60;

    if(newX>=x0 && newX<=x0+50 &&
       newY>=templateY && newY<=templateY+40){

      todayExercise.templateType = i+1;
      needsRedraw = true;
      delay(120);
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

    needsRedraw = true;
    delay(120);
    return;
  }

  // +
  if(newX>=320 && newX<=370 &&
     newY>=timeY && newY<=timeY+40){

    todayExercise.minutes += 5;

    needsRedraw = true;
    delay(120);
    return;
  }

  // ================= EFFORT =================
  int effortY = 160;

  for(int i=0;i<=10;i++){
    int x0 = 110 + i*30;

    if(newX>=x0 && newX<=x0+25 &&
       newY>=effortY && newY<=effortY+25){

      todayExercise.effort = i;
      needsRedraw = true;
      delay(120);
      return;
    }
  }

  // ================= ENERGY =================
  int energyY = 200;
  int energyVals[5] = {-2,-1,0,1,2};

  for(int i=0;i<5;i++){
    int x0 = 120 + i*60;

    if(newX>=x0 && newX<=x0+50 &&
       newY>=energyY && newY<=energyY+40){

      todayExercise.energy = energyVals[i];
      needsRedraw = true;
      delay(120);
      return;
    }
  }

  // ================= WEIGHT =================
  int weightY = 260;

  // -1
  if(newX>=120 && newX<=170 &&
     newY>=weightY && newY<=weightY+40){

    todayExercise.weight -= 1.0;
    needsRedraw = true;
    delay(120);
    return;
  }

  // -0.1
  if(newX>=180 && newX<=240 &&
     newY>=weightY && newY<=weightY+40){

    todayExercise.weight -= 0.1;
    needsRedraw = true;
    delay(120);
    return;
  }

  // +0.1
  if(newX>=310 && newX<=370 &&
     newY>=weightY && newY<=weightY+40){

    todayExercise.weight += 0.1;
    needsRedraw = true;
    delay(120);
    return;
  }

  // +1
  if(newX>=380 && newX<=430 &&
     newY>=weightY && newY<=weightY+40){

    todayExercise.weight += 1.0;
    needsRedraw = true;
    delay(120);
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
    delay(200);
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
    delay(200);
    return;
  }

  if(newX>=200 && newX<=320 && newY>=90 && newY<=140){
    darkMode = !darkMode;
    applyTheme();
    needsRedraw = true;
    delay(200);
  }
}


// ============================================================
// SETUP
// ============================================================
void setup(){

  pinMode(TFT_LED,OUTPUT);
  digitalWrite(TFT_LED,HIGH);

  Wire.begin();
  touchController.begin(Wire,0x38);

  SPI.setMOSI(11);
  SPI.setSCK(13);

  tft.begin();
  tft.setRotation(3);
  tft.useFrameBuffer(true);

  randomSeed(analogRead(A0));
  assignColors(morningTasks,MORNING_COUNT);
  assignColors(nightTasks,NIGHT_COUNT);

  applyTheme();
}

// ============================================================
// LOOP
// ============================================================
void loop(){

  if(currentPage!=lastPage){
    needsRedraw=true;
    scrollOffset=0;
    lastPage=currentPage;
  }

  if(currentPage==HOME_PAGE){
    handleHomeTouch();
    if(needsRedraw){ drawHome(); needsRedraw=false; }
  }

  else if(currentPage==MORNING_PAGE){
    handleTasks(morningTasks,MORNING_COUNT);
    if(needsRedraw){ drawTasks(morningTasks,MORNING_COUNT,"Morning Routine"); needsRedraw=false; }
  }

  else if(currentPage==NIGHT_PAGE){
    handleTasks(nightTasks,NIGHT_COUNT);
    if(needsRedraw){ drawTasks(nightTasks,NIGHT_COUNT,"Night Routine"); needsRedraw=false; }
  }

  else if(currentPage==MEAL_PAGE){
  
    handleMealTouch();
  
    if(needsRedraw){
      drawMealPage();
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

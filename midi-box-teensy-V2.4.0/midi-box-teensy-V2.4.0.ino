/* 
 * ----------------------------------------
 * VWR MIDI CLOCK BOX V2.4.0 for Teensy 4.0
 * ----------------------------------------
 * Notes:
 * No midi clock input yet! (maybe never in this box version)
 * Korg sync out not working (yeah maybe innit)
 * Din Sync working correctly (tested on Roland TB-303, Roland TR606, Acidlab Miami and Arturia DrumBrute)
 * ----------------------------------------
   TFT Pin Settings:
    +5V:                    +3.3V
    MISO:                   12 Miso
    SCK:                    13 Sck
    MOSI:                   11 Mosi
    LCD CS:                 10
    SD CS:                  4
    D/C:                    9
    RESET:                  3
    BL:                     +3.3V / 34
    GND:                    GND
 * ----------------------------------------
   PINS IN/OUT
 * ----------------------------------------
    BPM DIMMER              A4  (18)
    OFFSET1                 A5  (19)
    OFFSET2                 A8  (22)
    OFFSET3                 A9  (23)
    OFFSET4                 A12 (26)
    OFFSETDINSYNC           A13 (27)

    BPM BLINK PIN           5
    SAVE PIN                6
    START/STOP PIN          2

    DIN SYNC pulse pin      24 //
    DIN SYNC start pin      31 //
    DIN SYNC continue pin   32 (not in use now)
    DIN SYNC ground         33 

    MIDI OUT PORTS          RX/TX (TX ONLY!)
    Serial1                 0 / 1
    Serial2                 7 / 8
    Serial3                 15/14
    Serial4                 16/17

    MIDI IN PORT            RX/TX (RX ONLY!)
    Serial5                 21/20
    ----------------------------------------
 */

#include <ST7735_t3.h> // Teensy 3.2>4.0 ST7735 mod
#include <SPI.h>

#include <TimerOne.h>
#include <EEPROM.h>
#include <eepromtheshit.h>
#include <neotimer.h>

/* =============== TFT SETTINGS =================== */

#define TFT_MISO  12
#define TFT_MOSI  11  //a12
#define TFT_SCK   13  //a13
#define TFT_DC   9 
#define TFT_CS   10  
#define TFT_RST  3         

#define BRIGHTNESS    204       
#define RANGE         100            
#define WIDTH         160            
#define HALFWIDTH     80
#define QUARTERWIDTH  40        
#define HEIGHT        128             
#define PERSIST       500        

ST7735_t3 TFTscreen = ST7735_t3(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCK, TFT_RST);

// TFT COLOR SCHEME
uint16_t grey = 0x7BCF;
uint16_t textcolor = 0xFFFF;

// CHARS FOR TFT TEXT OUPUT STRINGS
char bpmChar[4];
char offset1Char[4];
char offset2Char[4];
char offset3Char[4];
char offset4Char[4];
char offsetDinSyncChar[4];

/* =============== MORE SETTINGS =================== */

// Update BPM and OFFSETS on boot from a. Current pot value OR b. Eeprom
int once = 0;

// Debug ON/OFF (disable serial port midi!)
boolean debug = false;

/* =============== INPUT PINS CONF =================== */

// DIMMER BPM INPUT
#define DIMMER_INPUT_PIN A4
#define DIMMER_CHANGE_MARGIN 5

// BPM TEMPO LED
#define BLINK_OUTPUT_PIN 5
#define BLINK_PIN_POLARITY 0
#define BLINK_TIME 4

// EEPROM SETTINGS SAVING BUTTON
#define SAVE_INPUT_PIN 6
#define SAVE_PIN_POLARITY 0

// START STOP BUTTON
#define START_STOP_INPUT_PIN 2
#define START_STOP_PIN_POLARITY 0
#define MIDI_START 0xFA
#define MIDI_STOP 0xFC
#define DEBOUNCE_INTERVAL 500L // Milliseconds

// GENERAL TIMING PARAMETERS
#define MIDI_TIMING_CLOCK 0xF8
#define CLOCKS_PER_BEAT 48
#define CLOCKS_PER_BEAT_SPLIT 24
#define MINIMUM_BPM 400 // Used for debouncing
#define MAXIMUM_BPM 2500 // Used for debouncing

/* =============== MORE INT =================== */

// SPLIT MIDI CLOCK VS DIN COUNTER
int clockCount = 0;

// INTERVAL TIME
unsigned long intervalMicroSeconds;
unsigned long interval;

// DATATYPES TO INCLUDE IN EEPROM
struct config_t{
  int bpm;  // BPM in 10th of a BPM!!
  int offset1;
  int offset2;
  int offset3;
  int offset4;
  int offsetDinSync;
} int2eeprom;

boolean initialized = false;
volatile int blinkCount = 0;
int lastDimmerValue = 0;
boolean playing = false;
unsigned long lastStartStopTime = 0;
unsigned long lastSaveTime = 0;

#ifdef DIMMER_CHANGE_PIN
long changeValue = 0;
#endif

/* =============== CLOCK OFFSETS =================== */

const int setOffset = 99; // SET GLOBAL OFFSET RANGE HERE, yes offset range not the actual offset eh

#define OFFSET1_INPUT_PIN A5
#define OFFSET2_INPUT_PIN A8
#define OFFSET3_INPUT_PIN A9
#define OFFSET4_INPUT_PIN A12
#define OFFSETDINSYNC_INPUT_PIN A13

#ifdef OFFSET1_INPUT_PIN
    #define OFFSET1_CHANGE_MARGIN 5
    int curOffsetValue1 = 0;
    int lastOffsetValue1 = 0;
    boolean delayStart1Running = false;
    Neotimer startTimer1 = Neotimer(10);
#endif
#ifdef OFFSET2_INPUT_PIN
    #define OFFSET2_CHANGE_MARGIN 5
    int curOffsetValue2 = 0;
    int lastOffsetValue2 = 0;
    boolean delayStart2Running = false;
    Neotimer startTimer2 = Neotimer(10);
#endif
#ifdef OFFSET3_INPUT_PIN
    #define OFFSET3_CHANGE_MARGIN 5
    int curOffsetValue3 = 0;
    int lastOffsetValue3 = 0;
    boolean delayStart3Running = false;
    Neotimer startTimer3 = Neotimer(10);
#endif
#ifdef OFFSET4_INPUT_PIN
    #define OFFSET4_CHANGE_MARGIN 5
    int curOffsetValue4 = 0;
    int lastOffsetValue4 = 0;
    boolean delayStart4Running = false;
    Neotimer startTimer4 = Neotimer(10);
#endif
#ifdef OFFSETDINSYNC_INPUT_PIN
    #define OFFSETDINSYNC_CHANGE_MARGIN 5
    int curOffsetValueDinSync = 0;
    int lastOffsetValueDinSync = 0;
    boolean delayStartDinSyncRunning = false;
    Neotimer startTimerDinSync = Neotimer(10);
#endif

// DIN SYNC STUFF HERE

#ifdef OFFSETDINSYNC_INPUT_PIN
    const int sync_24_pulse_pin = 24;
    const int sync_24_start_pin = 31; 
    const int sync_24_continue_pin = 32; 
    const int sync_ground = 33; 
#endif

/* =============== TIME =================== */

Neotimer startTimerSave = Neotimer(10);

  unsigned long now = micros(); // NOW FOR BPM RESOLUTION
  unsigned long now2 = millis();  // NOW2 FOR BUTTONS DEBOUNCE


/* =============== START SETUP ==================== */
/* ================================================ */

void setup() {

  Serial.begin(9600); // DEBUG USB CONSOLE

  // SET BAUD RATES FOR MIDI PORTS:
  
  Serial1.begin(31250);
  Serial2.begin(31250);
  Serial3.begin(31250);
  Serial4.begin(31250);
  
/* =============== TFT LAYOUT DRAWING =================== */

  // FORMAT: TFTscreen.drawRect(X,Y,W,H,RGB565);
  
    TFTscreen.initR(INITR_BLACKTAB);        
    TFTscreen.setRotation(1);
    TFTscreen.fillScreen(ST7735_BLACK);
    
    //testdrawrects(ST7735_GREEN); // TFT SCREEN TEST FUNCTION

    delay(1000);
    TFTscreen.drawRect(0,0,160,128,ST7735_WHITE);
    delay(1000);
    
    TFTscreen.setTextColor(textcolor);
    
    TFTscreen.setTextSize(2);
    TFTscreen.setCursor(9, 20);
    TFTscreen.println("V");
    delay(800);
    TFTscreen.setCursor(9, 20);
    TFTscreen.println("VW");

    delay(500);
    TFTscreen.setTextSize(1);
    TFTscreen.setCursor(36, 26);
    TFTscreen.println("C");delay(100);TFTscreen.setCursor(36, 26);
    TFTscreen.println("CR");delay(100);TFTscreen.setCursor(36, 26);
    TFTscreen.println("CRE");delay(100);TFTscreen.setCursor(36, 26);
    TFTscreen.println("CREA");delay(100);TFTscreen.setCursor(36, 26);
    TFTscreen.println("CREAT");delay(100);TFTscreen.setCursor(36, 26);
    TFTscreen.println("CREATI");delay(100);TFTscreen.setCursor(36, 26);
    TFTscreen.println("CREATIV");delay(100);TFTscreen.setCursor(36, 26);
    TFTscreen.println("CREATIVE");delay(100);TFTscreen.setCursor(36, 26);
    TFTscreen.println("CREATIVE T");delay(100);TFTscreen.setCursor(36, 26);
    TFTscreen.println("CREATIVE TE");delay(100);TFTscreen.setCursor(36, 26);
    TFTscreen.println("CREATIVE TEC");delay(100);TFTscreen.setCursor(36, 26);
    TFTscreen.println("CREATIVE TECH");delay(100);TFTscreen.setCursor(36, 26);
    TFTscreen.println("CREATIVE TECHNO");delay(100);TFTscreen.setCursor(36, 26);
    TFTscreen.println("CREATIVE TECHNOL");delay(100);TFTscreen.setCursor(36, 26);
    TFTscreen.println("CREATIVE TECHNOLO");delay(100);TFTscreen.setCursor(36, 26);
    TFTscreen.println("CREATIVE TECHNOLOG");delay(100);TFTscreen.setCursor(36, 26);
    TFTscreen.println("CREATIVE TECHNOLOGY");delay(100);
    delay(500);
    TFTscreen.setCursor(25, 60);
    TFTscreen.println("CLOCK BOX PROTOTYPE");
    delay(1000);
    TFTscreen.setCursor(22, 100);
    TFTscreen.println("FIRMWARE VERSION 4.1");
    delay(2000);


// RIGHT SIDE (OFFSETS)

TFTscreen.fillScreen(ST7735_BLACK);


TFTscreen.drawRect(82,0,78,14,0x74DF);
TFTscreen.fillRect(82,0,78,14,0x74DF);
TFTscreen.setTextColor(textcolor);
TFTscreen.setCursor(103, 3);
TFTscreen.println("OUTPUTS");


TFTscreen.drawRect(82,18,20,18,0xFE0B);
TFTscreen.fillRect(82,18,20,18,0xFE0B);
TFTscreen.setTextColor(textcolor);
TFTscreen.setCursor(87, 24);
TFTscreen.println("DS");
TFTscreen.drawRect(105,18,56,18,grey);
TFTscreen.fillRect(105,18,56,18,grey);


TFTscreen.drawRect(82,40,20,18,0xFBAA);
TFTscreen.fillRect(82,40,20,18,0xFBAA);
TFTscreen.setTextColor(textcolor);
TFTscreen.setCursor(90,46);
TFTscreen.println("1");
TFTscreen.drawRect(105,40,56,18,grey);
TFTscreen.fillRect(105,40,56,18,grey);


TFTscreen.drawRect(82,62,20,18,0xFB48);
TFTscreen.fillRect(82,62,20,18,0xFB48);
TFTscreen.setTextColor(textcolor);
TFTscreen.setCursor(90,68);
TFTscreen.println("2");
TFTscreen.drawRect(105,62,56,18,grey);
TFTscreen.fillRect(105,62,56,18,grey);


TFTscreen.drawRect(82,84,20,18,0xFAC5);
TFTscreen.fillRect(82,84,20,18,0xFAC5);
TFTscreen.setTextColor(textcolor);
TFTscreen.setCursor(90,90);
TFTscreen.println("3");
TFTscreen.drawRect(105,84,56,18,grey);
TFTscreen.fillRect(105,84,56,18,grey);


TFTscreen.drawRect(82,106,20,18,0xF9E1);
TFTscreen.fillRect(82,106,20,18,0xF9E1);
TFTscreen.setTextColor(textcolor);
TFTscreen.setCursor(90,112);
TFTscreen.println("4");
TFTscreen.drawRect(105,106,56,18,grey);
TFTscreen.fillRect(105,106,56,18,grey);


 // LEFT SIDE BPM

TFTscreen.drawRect(0,0,78,14,0xF9E1);
TFTscreen.fillRect(0,0,78,14,0xF9E1);
TFTscreen.setTextColor(textcolor);
TFTscreen.setCursor(30,3);
TFTscreen.println("BPM");
TFTscreen.drawRect(0,18,78,106,grey);
TFTscreen.fillRect(0,18,78,106,grey);
   
    if(debug){Serial.println("TFT Online");}

    
/* =============== SET PIN MODES =================== */
  
#ifdef BLINK_OUTPUT_PIN
  pinMode(BLINK_OUTPUT_PIN, OUTPUT);
#endif
#ifdef DIMMER_INPUT_PIN
  pinMode(DIMMER_INPUT_PIN, INPUT);
#endif
#ifdef OFFSET1_INPUT_PIN
  pinMode(OFFSET1_INPUT_PIN, INPUT);
#endif
#ifdef OFFSET2_INPUT_PIN
  pinMode(OFFSET2_INPUT_PIN, INPUT);
#endif
#ifdef OFFSET3_INPUT_PIN
  pinMode(OFFSET3_INPUT_PIN, INPUT);
#endif
#ifdef OFFSET4_INPUT_PIN
  pinMode(OFFSET4_INPUT_PIN, INPUT);
#endif
#ifdef OFFSETDINSYNC_INPUT_PIN
  pinMode(OFFSETDINSYNC_INPUT_PIN, INPUT);
#endif
#ifdef START_STOP_INPUT_PIN
  pinMode(START_STOP_INPUT_PIN, INPUT);
#endif
#ifdef SAVE_INPUT_PIN
  pinMode(SAVE_INPUT_PIN, INPUT);
#endif


/* =============== START INTERRUPT TIMERS =================== */

  Timer1.initialize(intervalMicroSeconds);
  Timer1.setPeriod(calculateIntervalMicroSecs(int2eeprom.bpm));
  Timer1.attachInterrupt(sendClockPulse);

/* =============== READ POTS =================== */

  #ifdef DIMMER_INPUT_PIN
    lastDimmerValue = analogRead(DIMMER_INPUT_PIN);
  #endif
  #ifdef OFFSET1_INPUT_PIN
    lastOffsetValue1 = analogRead(OFFSET1_INPUT_PIN);
  #endif
  #ifdef OFFSET2_INPUT_PIN
    lastOffsetValue2 = analogRead(OFFSET2_INPUT_PIN);
  #endif
  #ifdef OFFSET3_INPUT_PIN
    lastOffsetValue3 = analogRead(OFFSET3_INPUT_PIN);
  #endif
  #ifdef OFFSET4_INPUT_PIN
    lastOffsetValue4 = analogRead(OFFSET4_INPUT_PIN);
  #endif
  #ifdef OFFSETDINSYNC_INPUT_PIN
    lastOffsetValueDinSync = analogRead(OFFSETDINSYNC_INPUT_PIN);
  #endif

/* =============== DIN SYNC PIN SETTINGS =================== */

  #ifdef OFFSETDINSYNC_INPUT_PIN
  // DIN SYNC
    pinMode(sync_ground, OUTPUT); 
    digitalWrite(sync_ground, LOW);
    pinMode(sync_24_pulse_pin, OUTPUT); // Sync 24 sync pulse output
    pinMode(sync_24_continue_pin, OUTPUT); // Sync 24 sync pulse output
    pinMode(sync_24_start_pin, OUTPUT); // Sync 24 start / stop output
  #endif
  
int2eeprom.bpm = 120; // SET STATIC BOOT BPM ON INIT (if not done by anything else like eeprom or analogread the pot)

load(); // LOAD DATATYPE VALUES FROM EEPROM


} // END SETUP



/* =============== MAIN PROGRAM LOOP =================== */
/* ===================================================== */

void loop() {

/* ================= TIME =================== */

 now = micros(); // NOW FOR BPM RESOLUTION
 now2 = millis();  // NOW2 FOR BUTTONS DEBOUNCE


/* ================= POT AND BUTTON READING =================== */

// BPM DIMMER POT

#ifdef DIMMER_INPUT_PIN
  int curDimValue = analogRead(DIMMER_INPUT_PIN);
  if (curDimValue > lastDimmerValue + DIMMER_CHANGE_MARGIN
      || curDimValue < lastDimmerValue - DIMMER_CHANGE_MARGIN) {
    int2eeprom.bpm = map(curDimValue, 0, 1024, MINIMUM_BPM, MAXIMUM_BPM);
      updateBpm(now);
      lastDimmerValue = curDimValue;
  }


#endif

// START-STOP BUTTON

#ifdef START_STOP_INPUT_PIN
  boolean startStopPressed = digitalRead(START_STOP_INPUT_PIN) ? true : false;
  if (startStopPressed && (lastStartStopTime + (DEBOUNCE_INTERVAL)) < now2) {
    startOrStop();
    lastStartStopTime = now2;
  }

#endif

// SAVE BUTTON

#ifdef SAVE_INPUT_PIN
  boolean savePressed = digitalRead(SAVE_INPUT_PIN) ? true : false;
  if (savePressed && (lastSaveTime + (DEBOUNCE_INTERVAL)) < now2) {
    save();
    lastSaveTime = now2;
  }
#endif

// CLOCK OFFSET STUFF 4X OUTPUT

#ifdef OFFSET1_INPUT_PIN
  int curOffsetValue1 = analogRead(OFFSET1_INPUT_PIN);
  if (curOffsetValue1 > lastOffsetValue1 + OFFSET1_CHANGE_MARGIN
      || curOffsetValue1 < lastOffsetValue1 - OFFSET1_CHANGE_MARGIN) {
        int2eeprom.offset1 = map(curOffsetValue1, 0, 1024, 0, setOffset); // Offset value for 1st clock
        lastOffsetValue1 = curOffsetValue1;
        startTimer1 = Neotimer(int2eeprom.offset1);
          offset1display(); // TFT
      }
      
  if(startTimer1.done()){
    sendStart1();
    startTimer1.stop();
    startTimer1.reset();
  }

#endif


#ifdef OFFSET2_INPUT_PIN
  int curOffsetValue2 = analogRead(OFFSET2_INPUT_PIN);
  if (curOffsetValue2 > lastOffsetValue2 + OFFSET2_CHANGE_MARGIN
      || curOffsetValue2 < lastOffsetValue2 - OFFSET2_CHANGE_MARGIN) {
        int2eeprom.offset2 = map(curOffsetValue2, 0, 1024, 0, setOffset); // Offset value for 1st clock
        lastOffsetValue2 = curOffsetValue2;
        startTimer2 = Neotimer(int2eeprom.offset2);
         offset2display(); // TFT
      }
  if(startTimer2.done()){
    sendStart2();
    startTimer2.stop();
    startTimer2.reset();
  }

#endif


#ifdef OFFSET3_INPUT_PIN
  int curOffsetValue3 = analogRead(OFFSET3_INPUT_PIN);
  if (curOffsetValue3 > lastOffsetValue3 + OFFSET3_CHANGE_MARGIN
      || curOffsetValue3 < lastOffsetValue3 - OFFSET3_CHANGE_MARGIN) {
        int2eeprom.offset3 = map(curOffsetValue3, 0, 1024, 0, setOffset); // Offset value for 1st clock
        lastOffsetValue3 = curOffsetValue3;        
        startTimer3 = Neotimer(int2eeprom.offset3);
         offset3display(); // TFT
      }
  if(startTimer3.done()){
    sendStart3();
    startTimer3.stop();
    startTimer3.reset();
  }

#endif


#ifdef OFFSET4_INPUT_PIN
  int curOffsetValue4 = analogRead(OFFSET4_INPUT_PIN);
  if (curOffsetValue4 > lastOffsetValue4 + OFFSET4_CHANGE_MARGIN
      || curOffsetValue4 < lastOffsetValue4 - OFFSET4_CHANGE_MARGIN) {
        int2eeprom.offset4 = map(curOffsetValue4, 0, 1024, 0, setOffset); // Offset value for 1st clock
        lastOffsetValue4 = curOffsetValue4;
        startTimer4 = Neotimer(int2eeprom.offset4);
          offset4display(); // TFT
      }
  if(startTimer4.done()){
    sendStart4();
    startTimer4.stop();
    startTimer4.reset();
  }

#endif

#ifdef OFFSETDINSYNC_INPUT_PIN
  int curOffsetValueDinSync = analogRead(OFFSETDINSYNC_INPUT_PIN);
  if (curOffsetValueDinSync > lastOffsetValueDinSync + OFFSETDINSYNC_CHANGE_MARGIN
      || curOffsetValueDinSync < lastOffsetValueDinSync - OFFSETDINSYNC_CHANGE_MARGIN) {
        int2eeprom.offsetDinSync = map(curOffsetValueDinSync, 0, 1024, 0, setOffset); // Offset value for 1st clock
        lastOffsetValueDinSync = curOffsetValueDinSync;
        startTimerDinSync = Neotimer(int2eeprom.offsetDinSync);
          offsetDinSyncdisplay(); // TFT
      }
  if(startTimerDinSync.done()){
    sendStartDinSync();
    startTimerDinSync.stop();
    startTimerDinSync.reset();
  }

#endif


// INIT BPM ON BOOT (SET FROM EEPROM OR POT VALUES)

  if (once == 0){
    
    updateBpm(now);

    #ifdef OFFSET1_INPUT_PIN
      startTimer1 = Neotimer(int2eeprom.offset1);
      offset1display(); // TFT
      
      if(startTimer1.done()){
        sendStart1();
        startTimer1.stop();
        startTimer1.reset();
      }
    #endif
    #ifdef OFFSET2_INPUT_PIN
      startTimer2 = Neotimer(int2eeprom.offset2);
      offset2display(); // TFT
      
      if(startTimer2.done()){
        sendStart2();
        startTimer2.stop();
        startTimer2.reset();
      }
     #endif
    #ifdef OFFSET3_INPUT_PIN
      startTimer3 = Neotimer(int2eeprom.offset3);
      offset3display(); // TFT
     
      if(startTimer3.done()){
        sendStart3();
        startTimer3.stop();
        startTimer3.reset();
      }
      #endif
    #ifdef OFFSET4_INPUT_PIN
      startTimer4 = Neotimer(int2eeprom.offset4);
      offset4display(); // TFT

      if(startTimer4.done()){
        sendStart4();
        startTimer4.stop();
        startTimer4.reset();
      }
    #endif
    #ifdef OFFSETDINSYNC_INPUT_PIN
      startTimerDinSync = Neotimer(int2eeprom.offsetDinSync);
      offsetDinSyncdisplay(); // TFT
      if(startTimerDinSync.done()){
        sendStartDinSync();
        startTimerDinSync.stop();
        startTimerDinSync.reset();
      }
  #endif
    once = 1;
    
  }

      // REVERT TFT AFTER SAVE
      startTimerSave = Neotimer(2000);
      if(startTimerSave.done()){
        saveTFTrevert();
        startTimerSave.stop();
        startTimerSave.reset();
      }

       
} 
/* ================= NED OF MAIN PROGRAM LOOP =================== */
/* ============================================================== */



/* ======================= START SUB ROUTINES =================== */
/* ============================================================== */

// START-STOP ROUTINE

void startOrStop() {
  if (!playing) {
     if(debug){Serial.println("Start playing");}
    playingdisplay(); // Screen BPM goes green

    #ifdef OFFSET1_INPUT_PIN
      startTimer1.start();
    #endif
    #ifdef OFFSET2_INPUT_PIN
      startTimer2.start();
    #endif
    #ifdef OFFSET3_INPUT_PIN
      startTimer3.start();
    #endif
    #ifdef OFFSET4_INPUT_PIN
      startTimer4.start();
    #endif
    #ifdef OFFSETDINSYNC_INPUT_PIN
      startTimerDinSync.start();
    #endif
    
  } else {
     if(debug){Serial.println("Stop playing");}
    stoppeddisplay(); // Screen BPM goes red
    
    Serial1.write(MIDI_STOP);
    Serial2.write(MIDI_STOP);
    Serial3.write(MIDI_STOP);
    Serial4.write(MIDI_STOP);
    
    digitalWrite(sync_24_start_pin, LOW);  
    digitalWrite(sync_24_continue_pin, LOW); 
    
  }
  playing = !playing;
}


/* ================= NED OF MAIN PROGRAM LOOP =================== */
/* ============================================================== */




/* ======================== SUBROUTINES ========================= */
/* ============================================================== */


// SOLUTION FOR ROLAND DIN SYNC WAVEFORM MESS: 
// DOUBLE CLOCKS PER BEAT , then send midi clock only once per two main clocks and din sync on/off per main clock

void sendClockPulse() {

  dinSync(); // fire din sync double to split square wave into on/off in one midi clock tick, so skip one clock pulse

  if (clockCount == 2){
        
      clockCount = 0;
      Serial1.write(MIDI_TIMING_CLOCK);
      Serial2.write(MIDI_TIMING_CLOCK);
      Serial3.write(MIDI_TIMING_CLOCK);
      Serial4.write(MIDI_TIMING_CLOCK);

      blinkCount = (blinkCount + 1) % CLOCKS_PER_BEAT_SPLIT; // split in two
      if (blinkCount == 0) {
        // Turn led on
        #ifdef BLINK_OUTPUT_PIN
            analogWrite(BLINK_OUTPUT_PIN, 255 - BLINK_PIN_POLARITY);
        #endif
    
      } else {
        
        #ifdef BLINK_OUTPUT_PIN
            if (blinkCount == BLINK_TIME) {
              // Turn led on
              analogWrite(BLINK_OUTPUT_PIN, 0 + BLINK_PIN_POLARITY);
            }
        #endif
      }

} // end if clockcount
  
   clockCount++;


}


// WRITE MIDI START TO SERIAL FROM OFFSET

#ifdef OFFSET1_INPUT_PIN
void sendStart1() {
  Serial1.write(MIDI_START);
}
#endif
#ifdef OFFSET2_INPUT_PIN
void sendStart2() {
  Serial2.write(MIDI_START);
}
#endif
#ifdef OFFSET3_INPUT_PIN
void sendStart3() {
  Serial3.write(MIDI_START);
}
#endif
#ifdef OFFSET4_INPUT_PIN
void sendStart4() {
  Serial4.write(MIDI_START);
}
#endif
#ifdef OFFSETDINSYNC_INPUT_PIN
void sendStartDinSync() {
   digitalWrite(sync_24_start_pin, HIGH); 
    digitalWrite(sync_24_continue_pin, LOW); 
}
#endif


// SET BPM CHANGES (INTERVAL) TO TIMER/CLOCK

void updateBpm(long now) {
  // Update the timer
  interval = calculateIntervalMicroSecs(int2eeprom.bpm);
  Timer1.setPeriod(interval);
   bpmdisplay(); // SET BPM ON TFT
   
     if(debug){Serial.print("Set BPM to: ");}
     if(debug){Serial.print(int2eeprom.bpm / 10);}
     if(debug){Serial.print('.');}
     if(debug){Serial.println(int2eeprom.bpm % 10);}

}


// CALC INTERVAL MS BPM
long calculateIntervalMicroSecs(int bpm) {
  return 60L * 1000 * 1000 * 10 / bpm / CLOCKS_PER_BEAT;
}


// TFT functions --------------------------------------------------------------------------------

void bpmdisplay(){
      String bpmString = String((int2eeprom.bpm / 10));
      bpmString.toCharArray(bpmChar, 4);
      
      TFTscreen.drawRect(12, 54, 56, 26,grey);
      TFTscreen.fillRect(12, 54, 56, 26,grey);
      TFTscreen.setTextSize(3);
      
      if ((int2eeprom.bpm / 10) < 100){
        TFTscreen.setCursor(22,54);
        TFTscreen.println(bpmChar);
      } else {
        TFTscreen.setCursor(12,54);
        TFTscreen.println(bpmChar);
      }
      
}
void playingdisplay(){
      TFTscreen.drawRect(0,0,78,14,0x2EA7);
      TFTscreen.fillRect(0,0,78,14,0x2EA7);
      TFTscreen.setTextColor(textcolor);
      TFTscreen.setTextSize(1);
      TFTscreen.setCursor(30,3);
      TFTscreen.println("BPM");
}
void stoppeddisplay(){
      TFTscreen.drawRect(0,0,78,14,0xF9E1);
      TFTscreen.fillRect(0,0,78,14,0xF9E1);
      TFTscreen.setTextColor(textcolor);
      TFTscreen.setTextSize(1);
      TFTscreen.setCursor(30,3);
      TFTscreen.println("BPM");
}



#ifdef OFFSETDINSYNC_INPUT_PIN
void offsetDinSyncdisplay(){
      String offsetDinSyncString = String(int2eeprom.offsetDinSync);
      offsetDinSyncString.toCharArray(offsetDinSyncChar, 4);
          TFTscreen.setTextSize(1);
          TFTscreen.drawRect(124, 24, 18, 8,grey);
          TFTscreen.fillRect(124, 24, 18, 8,grey);
          TFTscreen.setTextColor(textcolor);
          TFTscreen.setCursor(126, 24);
          TFTscreen.println(offsetDinSyncChar);
}
#endif
#ifdef OFFSET1_INPUT_PIN
void offset1display(){
      String offset1String = String(int2eeprom.offset1);
      offset1String.toCharArray(offset1Char, 4);
          TFTscreen.setTextSize(1);
          TFTscreen.drawRect(124, 46, 18, 8,grey);
          TFTscreen.fillRect(124, 46, 18, 8,grey);
          TFTscreen.setTextColor(textcolor);
          TFTscreen.setCursor(126, 46);
          TFTscreen.println(offset1Char);
}
#endif
#ifdef OFFSET2_INPUT_PIN
void offset2display(){
      String offset2String = String(int2eeprom.offset2);
      offset2String.toCharArray(offset2Char, 4);
          TFTscreen.setTextSize(1);
          TFTscreen.drawRect(124, 68, 18, 8,grey);
          TFTscreen.fillRect(124, 68, 18, 8,grey);
          TFTscreen.setTextColor(textcolor);
          TFTscreen.setCursor(126, 68);
          TFTscreen.println(offset2Char);
}
#endif
#ifdef OFFSET3_INPUT_PIN
void offset3display(){
      String offset3String = String(int2eeprom.offset3);
      offset3String.toCharArray(offset3Char, 4);
          TFTscreen.setTextSize(1);
          TFTscreen.drawRect(124, 90, 18, 8,grey);
          TFTscreen.fillRect(124, 90, 18, 8,grey);
          TFTscreen.setTextColor(textcolor);
          TFTscreen.setCursor(126, 90);
          TFTscreen.println(offset3Char);

}
#endif
#ifdef OFFSET4_INPUT_PIN
void offset4display(){
      String offset4String = String(int2eeprom.offset4);
      offset4String.toCharArray(offset4Char, 4);
          TFTscreen.setTextSize(1);
          TFTscreen.drawRect(124, 112, 18, 8,grey);
          TFTscreen.fillRect(124, 112, 18, 8,grey);
          TFTscreen.setTextColor(textcolor);
          TFTscreen.setCursor(126, 112);
          TFTscreen.println(offset4Char);
}
#endif


// EEPROM THE SHIT (DATATYPES)----------------------------------------------------------------------

void load() {
EEPROM_readAnything(0,int2eeprom);
}

void save(){
  if (!playing) {
    EEPROM_writeAnything(0,int2eeprom);
    saveTFT();
  }
}

void saveTFT(){
      TFTscreen.drawRect(82,0,78,14,0xF800);
      TFTscreen.fillRect(82,0,78,14,0xF800);
      TFTscreen.setTextColor(textcolor);
      TFTscreen.setTextSize(1);
      TFTscreen.setCursor(103, 3);
      TFTscreen.println("SAVING");
      startTimerSave.start();
}

void saveTFTrevert(){
      TFTscreen.drawRect(82,0,78,14,0x74DF);
      TFTscreen.fillRect(82,0,78,14,0x74DF);
      TFTscreen.setTextSize(1);
      TFTscreen.setTextColor(textcolor);
      TFTscreen.setCursor(103, 3);
      TFTscreen.println("OUTPUTS");
}

// DIN SYNC 24 SQUARE WAVE CLOCK SIGNAL

void dinSync() {
  if (digitalRead(sync_24_pulse_pin) == HIGH){
         digitalWrite(sync_24_pulse_pin, LOW); 
     } else if (digitalRead(sync_24_pulse_pin) == LOW) {
         digitalWrite(sync_24_pulse_pin, HIGH);   
  }
}




// TEST SHIT TFT CONVERSION 8BIT TO 16BIT

void testdrawrects(uint16_t color) {
  TFTscreen.fillScreen(ST7735_BLACK);
  for (int16_t x=0; x < TFTscreen.width(); x+=6) {
    TFTscreen.drawRect(TFTscreen.width()/2 -x/2, TFTscreen.height()/2 -x/2 , x, x, color);
  }
}

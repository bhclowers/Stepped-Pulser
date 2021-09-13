/*
 * TODO: 
 * 
 * Convert period to float
 * Report Float
 * 
 */

#include <Arduino.h>

//#include "SAMD51_InterruptTimer.h"


#include <TFT_eSPI.h>
#include"Free_Fonts.h" //include the header file

#include <SPI.h>//ultimately used for interface to AD9850 (code not included).

#include "AD9850.h"

#include <Seeed_FS.h>
#include "SD/Seeed_SD.h"

TFT_eSPI gfx;

int test=25;

#define BTN_SEL WIO_5S_PRESS  // Select button
#define BTN_UP WIO_5S_UP // Up
#define BTN_DOWN WIO_5S_DOWN // Down


const int W_CLK_PIN = D2;
const int FQ_UD_PIN = D3;
const int DATA_PIN = D5;
const int RESET_PIN = D7;
const int PD_PIN = D4;//not really used but needed to instantiate the AD9850

int phase = 0;

//#define MHZ_125      124999500UL     // Module clock rate

AD9850 dds(FQ_UD_PIN, W_CLK_PIN, RESET_PIN, PD_PIN, DATA_PIN);

// the following variables are unsigned longs because the time, measured in
// microseconds, will quickly become a bigger number than can be stored in an int.
volatile unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 1000;    // the debounce time; increase if the output flickers


///////Pulsing Variables/////////

//volatile bool toggleBool = true;
volatile bool updated = false;

volatile bool outState = true;
//Interrupts Variables
int irqPin = D6; // Pin 33


int pLen = 10; //This number should be the same as the length of the lists for freq and dc (duty cycle)
//used to setup the variables and for debugging.
float freqs[10] = {1000,900,800,700,600,500,400,300,200,100}; //Period of the pulsing (e.g. a period of 100 with 125 is a pulse width of 50 us)
int dc[10] =   {127, 127, 127, 127, 127, 127, 127, 127, 127, 127};//duty cycle out of 255 if using just the interal PWM. Future will use an external digipot to set the duty cycle

int32_t numAves = 1;//this is going to be the number of times a frequency repeats before moving to the next.
int32_t curAve = 0;//variable for the current averages. Adjust for future use.

//File Read Variables from on-board flash
float freqNdc[2];//container to hold the frequency and the DC
float curVal;
int curIndex;
int curLine;
int buffersRead = 0;
int linesReported = 0;//number of lines that are reported/sent out.
  
uint32_t filePos;
int numLines = 0;//total number of lines in the file
long linesCounted;//number of lines counted on each call, used to account for end of file counting.
int lineBuf = 10;//number of lines to read that go into the buffer

File file;
String strLine;
char* pulseFile = "ps.csv";//this is hard coded.  Future could use a UI on the display but that is a distraction at this point. 
bool fileOK = 0;

//Setup Variables
float curFreq = 0.0;
uint32_t curPeriod = 2000;
int curDC = 127;//the duty cycle is scaled based between 0-255
uint32_t curIter = 0; //current index of the frequencies, //value associated with a given array regardless of refill.
float percDC = 50.0;
int maxDC = 255;


String blank = String("");
String inStr = blank;    // A string to hold incoming commands

//Progress Bar Info
int barVal;
int barWidth;
int barHeight;
int barYLoc;

////////Font Variables
int startY = 40;
int fontY = 25;
int fontX = 0;
int fontWidth = 10;

int gfxLineCount = 1;

///#####################

const uint32_t maxVal = 10000000;//This the value in seconds (10 s)

//Note that the changeState bool also needs to be paired with the enableBool, think of the enableBool as a manual check as we using this rig to trigger high voltage.
volatile bool changeState = LOW;//When this is high the change pin has been triggered, set initially to HIGH to start immediately with the first frequency.

volatile bool serialState = LOW;//high when the system is connected to a serial port, used for issuing commands

//###KEY VARIABLES####
uint32_t triggerSkip = 2;//number of triggers to skip. The LTQ, depending on the scan type put out 2 pulses per scan.  Need to ignore one.
uint32_t triggerCount = 0;//remember counting from zero

byte enableBool = 1;//controlled by the buttons.  The handleEnable function toggles this and is initially called in the setup. By setting this high HERE, when called again in the setup, it is automatically toggled to low.

///#####################


#include <menu.h>
#include <menuIO/TFT_eSPIOut.h>

#define Black RGB565(0,0,0)
#define Red  RGB565(255,0,0)
#define Green RGB565(0,255,0)
#define Blue RGB565(0,0,255)
#define Gray RGB565(128,128,128)
#define LighterRed RGB565(255,150,150)
#define LighterGreen RGB565(150,255,150)
#define LighterBlue RGB565(150,150,255)
#define DarkerRed RGB565(150,0,0)
#define DarkerGreen RGB565(0,150,0)
#define DarkerBlue RGB565(0,0,150)
#define Cyan RGB565(0,255,255)
#define Magenta RGB565(255,0,255)
#define Yellow RGB565(255,255,0)
#define White RGB565(255,255,255)

//Screen Size
#define GFX_WIDTH 320
#define GFX_HEIGHT 240
#define fontW 16
#define fontH 20

void handleEnable(){
  enableBool = not(enableBool);
  if (enableBool){
//    attachInterrupt(digitalPinToInterrupt(irqPin), CHANGEISR, RISING);
    dds.PowerDown(false);//DON'T POWER DOWN, POWER UP
  }
  else{
    detachInterrupt(irqPin);
    dds.PowerDown(true);
    
  }
}

void topbutton3(){
  long currentTime = micros();
  if ((currentTime - lastDebounceTime) > debounceDelay){
    lastDebounceTime = currentTime;
    Serial.println("Top Button 3");
    
    //Control Block for Menu Selection
    handleEnable();
  }

  updateEnable();
}

void topbutton2(){
  long currentTime = micros();
  if ((currentTime - lastDebounceTime) > debounceDelay){
    lastDebounceTime = currentTime;
    Serial.println("Top Button 2");
    }
    //Used for debugging
//  test += 1;
//  curFreq +=5;//1.0/((curPeriod/1E6)*2.0);
//  linesReported+=1;
  Serial.print(curFreq);
  Serial.print(", ");
  Serial.println(curPeriod);
  
  barVal = map(test, 0, 100, 0, 100);
//  int barWidth = gfx.width();
//  int barHeight = 25;
//  int barYLoc = gfx.height()-barHeight-10;//The 10 is for an offset so it is not slammed on the bottom
  gfx.fillRect(0, barYLoc, barWidth, barHeight, White);
  drawPercentbar(0, barYLoc, barWidth, barHeight, barVal);

  displayUpdate();

}

void topbutton1(){
  long currentTime = micros();
  if ((currentTime - lastDebounceTime) > debounceDelay){
    lastDebounceTime = currentTime;
    Serial.println("Top Button 1");

  }
}

void updateEnable(){
  
  gfxLineCount = 1;
  gfxLineCount+=3;
  
  if (enableBool){
    gfx.fillRect(fontWidth*14, startY+fontY*gfxLineCount, 10*14, fontY, Black);
    gfx.drawString("True", fontWidth*14, startY+fontY*gfxLineCount);  
    gfx.fillEllipse(300, 20, 10, 10, Red);
  }
  else{
    gfx.fillRect(fontWidth*14, startY+fontY*gfxLineCount, 10*14, fontY, Black);
    gfx.drawString("False", fontWidth*14, startY+fontY*gfxLineCount);
    gfx.fillEllipse(300, 20, 10, 10, Black);
  }  
  
}

void displayUpdate(){
  gfx.setTextColor(Yellow,Black);
  gfxLineCount = 1;
  gfx.fillRect(fontWidth*14, startY+fontY*gfxLineCount, 10*14, fontY, Black);//Clear Line
  gfx.drawFloat(curFreq, 2, fontWidth*14, startY+fontY*gfxLineCount);//"Current Freq: "
  gfxLineCount+=1;
  gfx.drawNumber(linesReported, fontWidth*14, startY+fontY*gfxLineCount);//"Current Line: "
//  gfx.drawNumber(test, fontWidth*14, startY+fontY*gfxLineCount);//"Current Line: "
  
}

void displaySetup(){
  gfx.setFreeFont(FSS12);
  gfx.setTextColor(Red,Black);
  gfx.drawString("Pulsing Control",70,20);//prints string at (70,80)

  gfx.setFreeFont(FSS9);
  gfx.setTextSize(1);//test scalling
  gfx.setTextColor(LighterBlue,Black);

//  int startY = 40;
//  int fontY = 25;
  gfxLineCount = 1;
  gfx.drawString("Current Freq: ", 10, startY+fontY*gfxLineCount);
  gfxLineCount+=1;
  gfx.drawString("Current Line: ", 10, startY+fontY*gfxLineCount);
  gfxLineCount+=2;
  gfx.drawString("Enabled: ", 10, startY+fontY*gfxLineCount);

  displayUpdate();
  updateEnable();
}

void gfxReportValue(String reportStr, long reportValue, int startX, int startY){
  gfx.setTextColor(Yellow);
  gfx.setTextSize(1);  
  gfx.setCursor(startX,startY);
  gfx.print(reportStr);
  gfx.println(reportValue);
}

void gfxReportMessage(String reportStr, int startX, int startY, uint16_t txtColor){
  gfx.setTextColor(txtColor);
  gfx.setTextSize(1);  
  gfx.setCursor(startX,startY);
  gfx.println(reportStr);
}

void setup() {

  pinMode(irqPin, INPUT);
  
  Serial.begin(115200);
  delay(1000);
//  while(!Serial);//Start right away when commented out.
  Serial.println("Pulse Control Module");
  Serial.flush(); 
  

  SPI.begin();
  gfx.begin();
  gfx.setRotation(3);

//  gfx.setFreeFont(FF17);
  gfx.setFreeFont(FSS9);
  
  gfx.setTextSize(1);//test scalling
  gfx.setTextWrap(false);
  gfx.fillScreen(Black);
  gfx.setTextColor(Red,Black);

  gfxReportMessage("Initializing SD Card...", 10, startY+fontY*6, Green);
  Serial.println("Initializing SD card...");
  if (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI)) {
    Serial.println("initialization failed!");
    gfx.fillScreen(Black);
    gfxReportMessage("Initialization Failed...Insert SD?", 10, startY+fontY*6, Green);
    while (1);
  }
  gfx.fillScreen(Black);
  gfxReportMessage("SD Card OK", 10, startY+fontY*6, Green);

//  float fVal = 3.0;
//  int perVal = 2;
//  fVal = fVal/perVal;
//  gfxReportMessage("Float Test", 10, startY+fontY*3, Green);
//  gfxReportMessage(String(fVal), 10, startY+fontY*4, Green);
  
  delay(2000);
  gfx.fillScreen(Black);

  barWidth = gfx.width();
  barHeight = 25;
  barYLoc = gfx.height()-barHeight-10;//The 10 is for an offset so it is not slammed on the bottom

  dds.Reset();

  handleEnable();//handles attaching and detatching the interrupt for the output pin
  displaySetup();

  gfxReportMessage("Lines in File: ", 10, startY+fontY*6, Green);
  countLines();
  gfxReportMessage(String(numLines), fontWidth*14, startY+fontY*6, Yellow);

/////Currently not used 
//  pinMode(WIO_5S_UP, INPUT_PULLUP);
//  pinMode(WIO_5S_DOWN, INPUT_PULLUP);
//  pinMode(WIO_5S_LEFT, INPUT_PULLUP);
//  pinMode(WIO_5S_RIGHT, INPUT_PULLUP);
//  pinMode(WIO_5S_PRESS, INPUT_PULLUP);

  pinMode(WIO_KEY_A, INPUT_PULLUP);
  pinMode(WIO_KEY_B, INPUT_PULLUP);
  pinMode(WIO_KEY_C, INPUT_PULLUP);
  
  attachInterrupt(digitalPinToInterrupt(WIO_KEY_A), topbutton3, RISING);
  attachInterrupt(digitalPinToInterrupt(WIO_KEY_B), topbutton2, RISING);
  attachInterrupt(digitalPinToInterrupt(WIO_KEY_C), topbutton1, RISING);
    
  delay(1000);

  lines2Buffer();

}


//Pin states are used instead of using interrupts based on falling or rising edges
int lastButtonState = LOW;
int buttonState;

void loop() {

  int reading = digitalRead(irqPin);

  if (reading != lastButtonState){
    lastDebounceTime = micros();
  }

  if ((micros() - lastDebounceTime) > debounceDelay){

    if (reading != buttonState){
      buttonState = reading;

      if (buttonState == HIGH){
        triggerCount += 1;
        if (triggerCount%triggerSkip == 0){
          changeState = HIGH;
        }
        
      }
      
      }
    
  }
  
if (changeState){
//      HighStateLen = 0;
      //condition where we need to change the output freq.
      //What is the default state of changeState? 
      if (curAve < numAves){
  
        curAve = 0;
        curFreq= freqs[curIter];//this should be 1/2 of the period of the frequency desired
//        curFreq = 1.0/(curPeriod*2.0/1000000);
        curDC = dc[curIter];//can set this to 127
        //   Duty cycle is 50%, set with the external pot

        dds.ApplySignal(curFreq, phase);
//        Serial.print(curFreq);
//        Serial.print(", ");
//        Serial.println(linesReported);
//        TC.restartTimer(curPeriod);
  
        
        changeState = LOW;
        curIter+=1;
        linesReported+=1;
        curIter%=pLen;//Need to ensure that this number is the length of the freq array
  
  
        //Refill Freq and Duty Cycle arrays:
        if (curIter == 0){
          lines2Buffer();
        }
        
        displayUpdate();
  
        barVal = map(linesReported, 0, numLines, 0, 100);
        gfx.fillRect(0, barYLoc, barWidth, barHeight, White);
        drawPercentbar(0, barYLoc, barWidth, barHeight, barVal);

        if (linesReported == numLines){
          detachInterrupt(irqPin);
          delay(10000);//this is a hack to delay the shutting off of the pulsing...If your process is longer than 10 seconds you need to change
          //### KEY VARIABLES: COMMENT OUT IF YOU WANT THE SYSTEM TO CONTINUE PULSING WHEN DONE...
          enableBool = false;
          dds.PowerDown(true);
          //////
//          TC.stopTimer();
          gfx.fillRect(10, startY+5+fontY*5, 240, fontY, Black);//clear previous message
          gfxReportMessage("Sequence Complete!", 10, startY+fontY*6, Green);
          updateEnable();  
        }
        
      }
      else {
        curAve += 1;
        changeState = LOW;  
      }
      
    }
  lastButtonState = reading;
}
  

void drawPercentbar(int x, int y, int width,int height, int progress){
   progress = progress > 100 ? 100 : progress;
   progress = progress < 0 ? 0 :progress;

   float bar = ((float)(width-4) / 100) * progress; 

   gfx.drawRect(x, y, width, height, DarkerBlue);//ST77XX_WHITE);
   gfx.fillRect(x+2, y+2, bar , height-4, DarkerBlue);//
  // Display progress text

  if( height >= 15){//THIS BEHAVIOR COULD BE FIXED BUT IT WOULD TAKE MORE COMPUTE CYCLES.  NOT GONNA MESS WITH IS RIGHT NOW.
    gfx.setCursor((width/2) - 10, y+18 );//The 10 and 20 are for the offset
    gfx.setTextColor(White);
//    tft.print(progress);
//    tft.print("%");
  if( progress >=50)
    gfx.setTextColor(White);//, ST77XX_WHITE); // 'inverted' text
  if( progress <= 50)
    gfx.setTextColor(DarkerBlue);//, ST77XX_WHITE); // 'inverted' text
  gfx.print(progress);
  gfx.print("%");
  
  }
}


void countLines(){
  // Waiting until there is any user input and echo

    file = SD.open(pulseFile, FILE_READ);
    if (!file) {
        Serial.println("Failed to open file for reading");
        return;
    }
    while( file.available() ) {
      strLine = file.readStringUntil('\n');
      numLines++;
    }
    file.close();
    Serial.print("Number of Lines: ");
    Serial.println(numLines);
}

void lines2Buffer(){
  
    // Get File Info and check if it is File or Directory
    int j = 0;
    linesCounted = 0;

//    File file = fs.open(path);
    file = SD.open(pulseFile, FILE_READ);
    if (!file) {
        Serial.println("Failed to open file for reading");
        return;
    }    
    while( file.available() ){
        if (j>=lineBuf || curLine>=numLines){
              break;//don't go too far and break the loop
        }
        file.seek(filePos);
  //          https://forum.arduino.cc/index.php?topic=231631.0
        strLine = file.readStringUntil('\n');
  //          Serial.println(file.tell());
        strLine.trim();
        if (strLine != "") {
          int l_start_posn = 0;
  
          curIndex = 0;
              
          while (l_start_posn != -1){
//            curVal = ENDF2(strLine,l_start_posn,',').toInt();
//            curVal = ENDF2(strLine,l_start_posn,',').toFloat();
//
            curVal = ENDF2(strLine,l_start_posn,',').toFloat();
            freqNdc[curIndex] = curVal;
            curIndex+=1;
//            Serial.print(curLine);
//            Serial.print(", ");
//            Serial.println(curVal);
          }
          freqs[j] = freqNdc[0];
          dc[j] = freqNdc[1];
          j+=1;
          curLine+=1;
          linesCounted+=1;
          filePos = file.position();
        } //skip blank (NULL) lines
      
    }
    file.close();
}

//Function to translate string of csvs to integer values
String ENDF2(String &p_line, int &p_start, char p_delimiter) {
//EXTRACT NEXT DELIMITED FIELD VERSION 2
//Extract fields from a line one at a time based on a delimiter.
//Because the line remains intact we dont fragment heap memory
//p_start would normally start as 0
//p_start increments as we move along the line
//We return p_start = -1 with the last field

  //If we have already parsed the whole line then return null
  if (p_start == -1) {
    return "";
  }

  int l_start = p_start;
  int l_index = p_line.indexOf(p_delimiter,l_start);
  if (l_index == -1) { //last field of the data line
    p_start = l_index;
    return p_line.substring(l_start);
  }
  else { //take the next field off the data line
    p_start = l_index + 1;
    return p_line.substring(l_start,l_index); //Include, Exclude
  }
}



//void CHANGEISR()
//{
//  //long currentTime = micros();
//  if ((currentTime - lastDebounceTime) > debounceDelay)
//  {
//    //lastDebounceTime = currentTime;
//    triggerCount+=1; 
//    if (changeState == LOW)
//    {
//      if (triggerCount%triggerSkip == 0){
//        changeState = HIGH;
//      }     
//    }
//  }
//  //lastDebounceTime = currentTime;
//}

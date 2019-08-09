#include <Esp32WifiManager.h>

#include <Adafruit_NeoPixel.h>
#include "TM1638plus.h"
#include <EEPROM.h>

#ifdef __AVR__
 #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif

// Which pin on the Arduino is connected to the NeoPixels?
// On a Trinket or Gemma we suggest changing this to 1:
#define  LED_PIN    23
#define  STROBE_TM  18
#define  CLOCK_TM   5
#define  DIO_TM     17
#define  STRIP_PIN  32

//#define  LED_PIN    2
//#define  STROBE_TM  3
//#define  CLOCK_TM   4
//#define  DIO_TM     5
//#define  STRIP_PIN  6

#define COLOUR_ARRAY_LEN 10
#define BRIGHTNESS_ARRAY_LEN 10

// Number Config:
#define LEDS_IN_SEGMENT   8
#define SEGMENTS_IN_DIGIT 7
#define MAX_DIGITS        5
#define DIGIT_COUNT       5
#define LED_COUNT LEDS_IN_SEGMENT * SEGMENTS_IN_DIGIT * DIGIT_COUNT
Adafruit_NeoPixel number_strip(LED_COUNT, LED_PIN, NEO_GRBW + NEO_KHZ800);

//// Strip Config:
#define STRIP_LED_COUNT   81
Adafruit_NeoPixel strip(STRIP_LED_COUNT, STRIP_PIN, NEO_GRBW + NEO_KHZ800);
int currentStripPosition;
bool goingUp;
unsigned long stripCycleTime;

// TM Config:
TM1638plus tm(STROBE_TM, CLOCK_TM , DIO_TM);
uint8_t digits[10] = { 0x3F, 0x21, 0x76, 0x73, 0x69, 0x5B, 0x5F, 0x31, 0x7F, 0x7B};
String default_display=" HELLO  ";
uint8_t last_button_state = 0x00;
bool updatedisplay=false;
int menu_mode=0;
unsigned long last_valid_button_press=0;
char display_buffer[9];

// Pride Config:
uint32_t firstPixelHue;

// Christmas Config:
int christmasSeedValue = 0;
unsigned long colourCycleTime=0;

class BrightnessProcessor{
  int brightness_increment = 10;
  int min_brightness = 10;
  int max_brightness = 100;
  int current_brightness_percent = 10;

  public:
    void setup(int percent) {
      current_brightness_percent=percent;
    }

    void setNextBrightness() {
      current_brightness_percent+=brightness_increment;
      if (current_brightness_percent > max_brightness) {
        current_brightness_percent = min_brightness;
      }
      strip.setBrightness(getBrightness());
      number_strip.setBrightness(getBrightness());
    }

    void setPreviousBrightness() {
      current_brightness_percent-=brightness_increment;
      if (current_brightness_percent < min_brightness) {
        current_brightness_percent=max_brightness;
      }
      strip.setBrightness(getBrightness());
      number_strip.setBrightness(getBrightness());
    }

    String getBrightnessName() {
      String brightnessAsString=String(current_brightness_percent);
      for (int i=brightnessAsString.length(); i<3; i++) {
        brightnessAsString=" "+brightnessAsString;
      }
      return "brt  " + brightnessAsString;
    }

    int getBrightnessPercent() {
      return current_brightness_percent;
    }

    int getBrightness() {
      return current_brightness_percent*2.5;
    }

    void processMenuSelection(uint32_t buttons) {
      if (bitRead(buttons,6) == 1) {
        setNextBrightness();
      } else if (bitRead(buttons,7) == 1) {
        setPreviousBrightness();
      }
    }
};
BrightnessProcessor brightnessProcessor;

class ColourProcessor{
  String colour_name[COLOUR_ARRAY_LEN] = { "RED", "GRE", "BLU", "CYA", "PUR", "YLO", "DAY", "CRE", "PRD", "CHS"  };
  uint32_t strip_colour[COLOUR_ARRAY_LEN] = { number_strip.Color(255,0,0), number_strip.Color(0,255,0), number_strip.Color(0,0,255), number_strip.Color(0,255,255), number_strip.Color(255,0,255), number_strip.Color(255,255,0), number_strip.Color(255,255,255), number_strip.Color(0,0,0,255), 0x00 };
  int current_colour_index;
  bool rainbow = false;
  String displayPrefix;
  
  public:
    void setup(int colour_index) {
      current_colour_index=colour_index;
    }

    void setDisplayPrefix(String prefix) {
      displayPrefix = prefix;
    }

    void setNextColour() {
      if (current_colour_index == (COLOUR_ARRAY_LEN-1)) {
        current_colour_index=0;
      } else {
        current_colour_index++;
      }
    }

    void setPreviousColour() {
      if (current_colour_index == 0) {
        current_colour_index=(COLOUR_ARRAY_LEN-1);
      } else {
        current_colour_index--;
      }
    }

    String getDisplayText() {
      Serial.println(displayPrefix);
      return displayPrefix + colour_name[current_colour_index];
    }

    int getColourIndex() {
      return current_colour_index;
    }

    uint32_t getColour(int ledPosition, unsigned long time_now) {
      if (colour_name[current_colour_index] == "PRD") {
        int pixelHue = firstPixelHue + (ledPosition * 65536L / number_strip.numPixels());
        return number_strip.gamma32(number_strip.ColorHSV(pixelHue));
      } else if (colour_name[current_colour_index] == "CHS") {
        if(time_now > colourCycleTime + 400){
          colourCycleTime=time_now;
          christmasSeedValue += 1;
        }
        if (((christmasSeedValue + ledPosition) % 5) == 0) {
          return number_strip.gamma32(number_strip.Color(255,0,0));
        } else if (((christmasSeedValue + ledPosition) % 18) == 0) {
          return number_strip.gamma32(number_strip.Color(255,255,255));
        } else {
          return number_strip.gamma32(number_strip.Color(0,150,0));
        }
      }
      return number_strip.gamma32(strip_colour[current_colour_index]);
    }

    void processMenuSelection(uint8_t buttons){
      if (bitRead(buttons,6) == 1) {
        setNextColour();
      } else if (bitRead(buttons,7) == 1) {
        setPreviousColour();
      }
    }
};
ColourProcessor digitColour;
ColourProcessor stripOnColour;
ColourProcessor stripOffColour;

class ValueProcessor{
  int value[MAX_DIGITS];
  String displayPrefix;
  
  public:
    void setup() {
      for (int i=0; i<MAX_DIGITS; i++) {
        value[i]=0;
      }
    }

    void setDisplayPrefix(String prefix) {
      displayPrefix = prefix;
    }

    int getDigit(int pos) {
      return value[pos];
    }

    int getValue() {
      return getValueAsString().toInt();
    }

    void setDigit(int pos, int newValue) {
      value[pos] = newValue;
    }

    String getDisplayText() {
      Serial.println(displayPrefix);
      return displayPrefix + getValueAsString();
    }

    String getValueAsString() {
      String numberAsString="";
      for (int i=0; i< MAX_DIGITS; i++) {
        numberAsString+=value[i];
      }
      return numberAsString;
    }

    void processMenuSelection(uint8_t buttons) {
      for (int digitPos = 3; digitPos<8; digitPos++) {
        if (bitRead(buttons,digitPos) == 1) {
          int numAtLoc=getDigit(digitPos-3);
          if (numAtLoc < 9) {
            setDigit(digitPos-3, numAtLoc+1);
          } else {
            setDigit(digitPos-3, 0);
          }
        }
      }
    }
};
ValueProcessor currentAmount;
ValueProcessor targetAmount;

void setup() {
  // These lines are specifically to support the Adafruit Trinket 5V 16 MHz.
  // Any other board, you can remove this part (but no harm leaving it):
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
  clock_prescale_set(clock_div_1);
#endif
  btStop();
  WiFi.mode(WIFI_OFF);
  touch_pad_intr_disable( );
  // END of Trinket-specific code.
  digitColour.setDisplayPrefix("COL- ");
  stripOnColour.setDisplayPrefix("ON - ");
  stripOffColour.setDisplayPrefix("OFF- ");
  currentAmount.setDisplayPrefix("CUR");
  targetAmount.setDisplayPrefix("TAR");
  loadData();                         // load data from the eeprom
  Serial.begin(9600);             // Initialise for debug
  firstPixelHue = 0;              // 
  number_strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  number_strip.setBrightness(brightnessProcessor.getBrightness());  
  strip.begin();                  // INITIALIZE NeoPixel strip object (REQUIRED)
  resetStrip();
  strip.setBrightness(brightnessProcessor.getBrightness());  
  tm.setLED(0, 1);
  tm.reset();
  default_display.toCharArray(display_buffer, 9);
  tm.displayText(display_buffer);
}

void loadData() {
  if (EEPROM.length() >=6) {
    currentAmount.setDigit(0, EEPROM.read(0));
    currentAmount.setDigit(1, EEPROM.read(1));
    currentAmount.setDigit(2, EEPROM.read(2));
    currentAmount.setDigit(3, EEPROM.read(3));
    currentAmount.setDigit(4, EEPROM.read(4));
    targetAmount.setDigit(0, EEPROM.read(5));
    targetAmount.setDigit(1, EEPROM.read(6));
    targetAmount.setDigit(2, EEPROM.read(7));
    targetAmount.setDigit(3, EEPROM.read(8));
    targetAmount.setDigit(4, EEPROM.read(9));
    brightnessProcessor.setup(EEPROM.read(10));
    digitColour.setup(EEPROM.read(11));
    stripOnColour.setup(EEPROM.read(12));
    stripOffColour.setup(EEPROM.read(13));
  } else {
    brightnessProcessor.setup(10);  // Set default starting brightness
    currentAmount.setup();
    targetAmount.setup();
    digitColour.setup(0);       // Set default starting colour
    stripOnColour.setup(0);       // Set default starting colour
    stripOffColour.setup(2);       // Set default starting colour
  }
}

void saveData() {
  EEPROM.write(0,currentAmount.getDigit(0));
  EEPROM.write(1,currentAmount.getDigit(1));
  EEPROM.write(2,currentAmount.getDigit(2));
  EEPROM.write(3,currentAmount.getDigit(3));
  EEPROM.write(4,currentAmount.getDigit(4));
  EEPROM.write(5,targetAmount.getDigit(0));
  EEPROM.write(6,targetAmount.getDigit(1));
  EEPROM.write(7,targetAmount.getDigit(2));
  EEPROM.write(8,targetAmount.getDigit(3));
  EEPROM.write(9,targetAmount.getDigit(4));
  EEPROM.write(10,brightnessProcessor.getBrightnessPercent());
  EEPROM.write(11,digitColour.getColourIndex());
  EEPROM.write(12,stripOnColour.getColourIndex());
  EEPROM.write(13,stripOffColour.getColourIndex());
}

void loop() {
  unsigned long time_now = millis();  // used for input lag and christmas colour cycle
  processTM1638(time_now);            // Process user input from the TM1638
  displayNumbers(time_now);           // Display the stuff on the things
  displayStrip(time_now); // Display the bouncing strip
  firstPixelHue += 200;               // Advance just a little along the color wheel for Pride  
}

void processTM1638(unsigned long time_now) {
  // Get user input
  uint8_t buttons = tm.readButtons();
  String display_text="";

  if (buttons != last_button_state) {
    last_button_state = buttons;
    if (buttons != 0x00) {
      Serial.print("Processing button press : ");
      Serial.println(buttons);
       if(time_now > last_valid_button_press + 100){
        last_valid_button_press=time_now;
        if (bitRead(buttons,0) == 1) {
          if (menu_mode == 6) {
            menu_mode=0;
          } else {
            menu_mode++;  
          }
        }
        Serial.print("Menu mode is : ");
        Serial.println(menu_mode);
        if (menu_mode == 1) {
          currentAmount.processMenuSelection(buttons);
          display_text=currentAmount.getDisplayText();
        } else if (menu_mode == 2) {
          targetAmount.processMenuSelection(buttons);
          display_text=targetAmount.getDisplayText();
        } else if (menu_mode == 3) {
          digitColour.processMenuSelection(buttons);
          display_text=digitColour.getDisplayText();
        } else if (menu_mode == 4) {
          stripOnColour.processMenuSelection(buttons);
          display_text=stripOnColour.getDisplayText();
        } else if (menu_mode == 5) {
          stripOffColour.processMenuSelection(buttons);
          display_text=stripOffColour.getDisplayText();
          resetStrip();
        } else if (menu_mode == 6) {
          brightnessProcessor.processMenuSelection(buttons);
          display_text=brightnessProcessor.getBrightnessName();
        } else {
          display_text=default_display;
        }
        display_text.toCharArray(display_buffer, 9);
        tm.displayText(display_buffer);
        saveData();                         // Save data to the eeprom
      }
    }
  }
}

// scans the individual bits of value
void doLEDs(uint8_t value) {
  for (uint8_t position = 0; position < 8; position++) {
    tm.setLED(position, value & 1);
    value = value >> 1;
  }
}

void displayNumbers(unsigned long time_now) {
  int currentPixelPosition=LED_COUNT;
  bool stripOn=false;
  for(int digit=(DIGIT_COUNT-1); digit>=0; digit--) {
    for (int n=0; n<SEGMENTS_IN_DIGIT; n++) {
      if (digits[currentAmount.getDigit(digit)] & (1 << n) ) {
        stripOn=true;
      } else {
        stripOn=false;
      }

      for(int led=0; led<LEDS_IN_SEGMENT; led++) {
        --currentPixelPosition;
        if (stripOn) {
          number_strip.setPixelColor(currentPixelPosition, digitColour.getColour(currentPixelPosition, time_now));
        } else {
          number_strip.setPixelColor(currentPixelPosition, number_strip.Color(0,0,0));
        }
      }
    }
  }
  delay(1);
  portDISABLE_INTERRUPTS();
  number_strip.show();                          //  Update strip to match
  portENABLE_INTERRUPTS();
}

void displayStrip(unsigned long time_now) {
  int displayPercent = 50;
  if (currentAmount.getValue() > 0 && targetAmount.getValue() > 0){
    displayPercent = (currentAmount.getValue()/(targetAmount.getValue()/100));    
  }

  int maxLedOn = (strip.numPixels()*displayPercent/100)-1;

  if(time_now > stripCycleTime + 50){
    stripCycleTime=time_now;
    if (goingUp) {
      currentStripPosition++;
      goingUp = (currentStripPosition <= maxLedOn);
      strip.setPixelColor((currentStripPosition-1), stripOnColour.getColour(currentStripPosition,stripCycleTime));         //  Set pixel's color (in RAM)
    } else {
      currentStripPosition--;
      goingUp = (currentStripPosition < 1);
      strip.setPixelColor((currentStripPosition-1), stripOffColour.getColour(currentStripPosition,stripCycleTime));         //  Set pixel's color (in RAM)
    }
    delay(1);
    portDISABLE_INTERRUPTS();
    strip.show();
    portENABLE_INTERRUPTS();
    //  Update strip to match
  }
}

void resetStrip() {
  currentStripPosition=0;
  goingUp=true;
  stripCycleTime=0;
  strip.fill(stripOffColour.getColour(currentStripPosition,stripCycleTime));
}

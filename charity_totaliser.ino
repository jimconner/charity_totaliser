#include <Adafruit_NeoPixel.h>
#include <TM1638plus.h>
#include <EEPROM.h>
#ifdef __AVR__
 #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif

// Which pin on the Arduino is connected to the NeoPixels?
// On a Trinket or Gemma we suggest changing this to 1:
#define  LED_PIN    2
#define  STROBE_TM  3
#define  CLOCK_TM   4
#define  DIO_TM     5

// How many NeoPixels are attached to the Arduino?
#define LEDS_IN_SEGMENT   1
#define SEGMENTS_IN_DIGIT 7
#define DIGIT_COUNT       1
#define LED_COUNT LEDS_IN_SEGMENT * SEGMENTS_IN_DIGIT * DIGIT_COUNT

#define COLOUR_ARRAY_LEN 10
#define BRIGHTNESS_ARRAY_LEN 10

//Constructor object
TM1638plus tm(STROBE_TM, CLOCK_TM , DIO_TM);

// Declare our NeoPixel strip object:
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRBW + NEO_KHZ800);
// Argument 1 = Number of pixels in NeoPixel strip
// Argument 2 = Arduino pin number (most are valid)
// Argument 3 = Pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)

uint32_t ledColour;
uint32_t firstPixelHue;
int christmasSeedValue = 0;
char display_buffer[9];

uint8_t digits[] = { 0x3F, 0x21, 0x76, 0x73, 0x69, 0x5B, 0x5F, 0x31, 0x7F, 0x7B};
int current_value[5]={0,0,0,0,0};
String default_display=" HELLO  ";
uint8_t last_button_state = 0x00;
bool updatedisplay=false;
int menu_mode=0;
unsigned long last_valid_button_press=0;
unsigned long colourCycleTime=0;

class BrightnessProcessor{
  int brightness_increment = 10;
  int min_brightness = 10;
  int max_brightness = 100;
  int current_brightness_percent = 30;

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
    }

    void setPreviousBrightness() {
      current_brightness_percent-=brightness_increment;
      if (current_brightness_percent < min_brightness) {
        current_brightness_percent=max_brightness;
      }
      strip.setBrightness(getBrightness());
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
};
BrightnessProcessor brightnessProcessor;

class ColourProcessor{
  String colour_name[COLOUR_ARRAY_LEN] = { "RED", "GRE", "BLU", "CYA", "PUR", "YLO", "DAY", "CRE", "PRD", "CHS" };
  uint32_t strip_colour[COLOUR_ARRAY_LEN] = { strip.Color(255,0,0), strip.Color(0,255,0), strip.Color(0,0,255), strip.Color(0,255,255), strip.Color(255,0,255), strip.Color(255,255,0), strip.Color(255,255,255), strip.Color(0,0,0,255), 0x00 };
  int current_colour_index;
  bool rainbow = false;
  
  public:
    void setup(int colour_index) {
      current_colour_index=colour_index;
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

    String getColourName() {
      return "COL  "+colour_name[current_colour_index];
    }

    int getColourIndex() {
      return current_colour_index;
    }

    uint32_t getColour(int ledPosition, unsigned long time_now) {
      if (colour_name[current_colour_index] == "PRD") {
        int pixelHue = firstPixelHue + (ledPosition * 65536L / strip.numPixels());
        return strip.gamma32(strip.ColorHSV(pixelHue));
      } else if (colour_name[current_colour_index] == "CHS") {
        if(time_now > colourCycleTime + 400){
          colourCycleTime=time_now;
          christmasSeedValue += 1;
        }
        
        if (((christmasSeedValue + ledPosition) % 5) == 0) {
          return strip.gamma32(strip.Color(255,0,0));
        } else if (((christmasSeedValue + ledPosition) % 18) == 0) {
          return strip.gamma32(strip.Color(255,255,255));
        } else {
          return strip.gamma32(strip.Color(0,150,0));
        }
      }
      return strip.gamma32(strip_colour[current_colour_index]);
    }
};
ColourProcessor colourProcessor;

void setup() {
  // These lines are specifically to support the Adafruit Trinket 5V 16 MHz.
  // Any other board, you can remove this part (but no harm leaving it):
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
  clock_prescale_set(clock_div_1);
#endif
  // END of Trinket-specific code.
  loadData();                         // load data from the eeprom
  Serial.begin(9600);             // Initialise for debug
  firstPixelHue = 0;              // 
  strip.begin();                  // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.setBrightness(brightnessProcessor.getBrightness());  
  tm.setLED(0, 1);
  tm.reset();
  default_display.toCharArray(display_buffer, 9);
  tm.displayText(display_buffer);
}

void loadData() {
  if (EEPROM.length() >=6) {
    current_value[0]=EEPROM.read(0);
    current_value[1]=EEPROM.read(1);
    current_value[2]=EEPROM.read(2);
    current_value[3]=EEPROM.read(3);
    current_value[4]=EEPROM.read(4);
    brightnessProcessor.setup(EEPROM.read(5));
    colourProcessor.setup(EEPROM.read(6));
  } else {
    brightnessProcessor.setup(30);  // Set default starting brightness
    colourProcessor.setup(0);       // Set default starting colour
  }
}

void saveData() {
  EEPROM.write(0,current_value[0]);
  EEPROM.write(1,current_value[1]);
  EEPROM.write(2,current_value[2]);
  EEPROM.write(3,current_value[3]);
  EEPROM.write(4,current_value[4]);
  EEPROM.write(5,brightnessProcessor.getBrightnessPercent());
  EEPROM.write(6,colourProcessor.getColourIndex());
}

void loop() {
  unsigned long time_now = millis();  // used for input lag and christmas colour cycle
  processTM1638(time_now);            // Process user input from the TM1638
  displayStrip(time_now);             // Display the stuff on the things
  firstPixelHue += 200;               // Advance just a little along the color wheel for Pride  
  saveData();                         // Save data to the eeprom
}

void processTM1638(unsigned long time_now) {
  // Get user input
  uint8_t buttons = tm.readButtons();
  String display_text="";

  if (buttons != last_button_state) {
    last_button_state = buttons;
    if (buttons != 0x00) {
      if(time_now > last_valid_button_press + 100){
        last_valid_button_press=time_now;
        changeMode(buttons);
        if (menu_mode == 1) {
          display_text="SET";
          processValueChange(buttons);
          for (int i=0; i< 5; i++) {
            display_text+=current_value[i];
          }
        } else if (menu_mode == 2) {
          if (bitRead(buttons,6) == 1) {
            colourProcessor.setNextColour();
          } else if (bitRead(buttons,7) == 1) {
            colourProcessor.setPreviousColour();
          }
          display_text=colourProcessor.getColourName();
        } else if (menu_mode == 3) {
          if (bitRead(buttons,6) == 1) {
            brightnessProcessor.setNextBrightness();
          } else if (bitRead(buttons,7) == 1) {
            brightnessProcessor.setPreviousBrightness();
          }
          display_text=brightnessProcessor.getBrightnessName();
        } else {
          display_text=default_display;
        }
        display_text.toCharArray(display_buffer, 9);
        tm.displayText(display_buffer);
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

void changeMode(uint8_t buttons) {
  if (bitRead(buttons,0) == 1) {
    if (menu_mode == 3) {
      menu_mode=0;
    } else {
      menu_mode++;  
    }
  }
}

void processValueChange(uint8_t buttons) {
  for (int valuePos = 3; valuePos<8; valuePos++) {
    if (bitRead(buttons,valuePos) == 1) {
      if (current_value[valuePos-3] < 9) {
        current_value[valuePos-3]++;
      } else {
        current_value[valuePos-3] = 0;
      }
    }
  }
}

void displayStrip(unsigned long time_now) {
  int currentPixelPosition=LED_COUNT;
  bool stripOn=false;
  for(int digit=(DIGIT_COUNT-1); digit>=0; digit--) {
    for (int n=0; n<SEGMENTS_IN_DIGIT; n++) {
      if (digits[current_value[digit]] & (1 << n) ) {
        stripOn=true;
      } else {
        stripOn=false;
      }

      for(int led=0; led<LEDS_IN_SEGMENT; led++) {
        --currentPixelPosition;
        if (stripOn) {
          strip.setPixelColor(currentPixelPosition, colourProcessor.getColour(currentPixelPosition, time_now));
        } else {
          strip.setPixelColor(currentPixelPosition, strip.Color(0,0,0));
        }
      }
    }
  }
  strip.show();
}

#include <Adafruit_NeoPixel.h>
#include <TM1638plus.h>
#ifdef _AVR_
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

// NeoPixel brightness, 0 (min) to 255 (max)
#define COLOUR_ARRAY_LEN 6
#define BRIGHTNESS_ARRAY_LEN 6

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
char display_buffer[9];

uint8_t digits[] = { 0x3F, 0x21, 0x76, 0x73, 0x69, 0x5B, 0x5F, 0x31, 0x7F, 0x7B};
int current_value[5]={5,2,3,4,5};
String default_display=" FIL 30 ";
uint8_t last_button_state = 0x00;
bool updatedisplay=false;
int menu_mode=0;
unsigned long last_valid_button_press=0;

class BrightnessProcessor{
  int brightness_values[BRIGHTNESS_ARRAY_LEN] = { 0, 10, 50, 100, 150, 200 };
  int current_brightness_index=1;
  
  public:
    void setup(int index) {
      current_brightness_index=index;
    }

    void setNextBrightness() {
      if (current_brightness_index == (BRIGHTNESS_ARRAY_LEN-1)) {
        current_brightness_index=0;
      } else {
        current_brightness_index++;
      }
      strip.setBrightness(brightness_values[current_brightness_index]);
    }

    void setPreviousBrightness() {
      if (current_brightness_index == 0) {
        current_brightness_index=(BRIGHTNESS_ARRAY_LEN-1);
      } else {
        current_brightness_index--;
      }
      strip.setBrightness(brightness_values[current_brightness_index]);
    }

    String getBrightnessName() {
      String brightnessAsString=String(brightness_values[current_brightness_index]);
      for (int i=brightnessAsString.length(); i<3; i++) {
        brightnessAsString=" "+brightnessAsString;
      }
      return brightnessAsString+"   ud";
    }

    int getBrightness(int ledPosition) {
      return brightness_values[current_brightness_index];
    }
};
BrightnessProcessor brightnessProcessor;

class ColourProcessor{
  String colour_name[COLOUR_ARRAY_LEN] = { "red", "grn", "blu", "wht", "bwt", "prd" };
  uint32_t strip_colour[COLOUR_ARRAY_LEN] = { strip.Color(255,0,0), strip.Color(0,255,0), strip.Color(0,0,255), strip.Color(255,255,255), strip.Color(0,0,0,255), 0x00 };
//  uint16_t strip_colour[COLOUR_ARRAY_LEN] = { 0xF800, 0x07E0, 0x001F, 0xFFFF, 0xFFFF, 0x00 };
  int current_colour_index;
  bool rainbow = false;
  
  public:
    void setup(int colour) {
      current_colour_index=colour;
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
      return colour_name[current_colour_index];
    }

    uint32_t getColour(int ledPosition) {
      if (colour_name[current_colour_index] == "prd") {
        int pixelHue = firstPixelHue + (ledPosition * 65536L / strip.numPixels());
        return strip.gamma32(strip.ColorHSV(pixelHue));
      }
      return strip.gamma32(strip_colour[current_colour_index]);
    }
};
ColourProcessor colourProcessor;

void setup() {
  // These lines are specifically to support the Adafruit Trinket 5V 16 MHz.
  // Any other board, you can remove this part (but no harm leaving it):
#if defined(_AVR_ATtiny85_) && (F_CPU == 16000000)
  clock_prescale_set(clock_div_1);
#endif
  // END of Trinket-specific code.
  Serial.begin(9600);
  firstPixelHue = 0;

  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();            // Turn OFF all pixels ASAP
  strip.setBrightness(brightnessProcessor.getBrightness(0)); // Set BRIGHTNESS to about 1/5 (max = 255)
  colourProcessor.setup(1);
  tm.setLED(0, 1);
  tm.reset();
  default_display.toCharArray(display_buffer, 9);
  tm.displayText(display_buffer);
  // Set default text and show the number in default colour
  strip.show();                          //  Update strip to match
}

void loop() {
  // Process user input from the TM1638
  processTM1638();

  // Display the stuff on the things
  displayStrip();
}

void processTM1638() {
  // Get user input
  uint8_t buttons = tm.readButtons();
  String display_text="";

  if (buttons != last_button_state) {
    last_button_state = buttons;
    if (buttons != 0x00) {
      unsigned long time_now = millis();
      if(time_now > last_valid_button_press + 150){
        last_valid_button_press=time_now;
        changeMode(buttons);
        if (menu_mode == 1) {
          display_text="SET";
          processValueChange(buttons);
          for (int i=0; i< 5; i++) {
            display_text+=current_value[i];
          }
        } else if (menu_mode == 2) {
          doLEDs(buttons);
          if (bitRead(buttons,6) == 1) {
            colourProcessor.setNextColour();
          } else if (bitRead(buttons,7) == 1) {
            colourProcessor.setPreviousColour();
          }
          display_text=colourProcessor.getColourName()+"   ud";
        } else if (menu_mode == 3) {
          doLEDs(buttons);
          if (bitRead(buttons,6) == 1) {
            brightnessProcessor.setNextBrightness();
          } else if (bitRead(buttons,7) == 1) {
            brightnessProcessor.setPreviousBrightness();
          }
          display_text=brightnessProcessor.getBrightnessName();
        } else {
          doLEDs(0x00);
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
  doLEDs(buttons);
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

void displayStrip() {
  firstPixelHue += 20; // Advance just a little along the color wheel
  int currentPixelPosition=LED_COUNT;
  for(int digit=(DIGIT_COUNT-1); digit>=0; digit--) {
    for (int n=0; n<SEGMENTS_IN_DIGIT; n++) {
      if (digits[current_value[digit]] & (1 << n) ) {
        ledColour = colourProcessor.getColour(currentPixelPosition);
      } else {
        ledColour = strip.Color(0,0,0);
      }

      for(int led=0; led<LEDS_IN_SEGMENT; led++) {
        --currentPixelPosition;
        strip.setPixelColor(currentPixelPosition, ledColour);
      }
    }
  }
  strip.show();
}

#include <Arduino.h>
#define BOARD_IDENTIFY_WARNING
#define USE_OCTOWS2811
#include <OctoWS2811.h>
#include <FastLED.h>

#define NUM_LEDS_PER_STRIP 300
#define NUM_STRIPS 5
#define NUM_LEDS NUM_LEDS_PER_STRIP * NUM_STRIPS
#define FRAME_RATE 100

#define OUR_PURPLE CRGB(CRGB::Purple).nscale8_video(128)
#define OUR_CYAN CRGB(CRGB::Cyan).nscale8_video(128)
#define OUR_SPARKLE CRGB::White

#if defined(FRAME_RATE)
  #define UWAIT ((1E6 / FRAME_RATE))
#else
  #define UWAIT 16666
#endif

CRGBArray<NUM_LEDS> leds;

uint16_t fade_count[NUM_LEDS];

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  pinMode(18, OUTPUT);
  digitalWrite(18, HIGH);
  pinMode(PIN_A3, INPUT);

  Serial.begin(115200);

  delay(125);
  // FastLED.addLeds<NEOPIXEL, 2>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip).setDither(DISABLE_DITHER);
  FastLED.addLeds<OCTOWS2811>(leds, NUM_LEDS_PER_STRIP);
  FastLED.setCorrection(TypicalLEDStrip);

  FastLED.setMaxPowerInVoltsAndMilliamps(5,2000);

  leds.fill_solid(OUR_PURPLE);

  randomSeed(0);
}


elapsedMicros framecounter;

void send_leds() {
  FastLED.show();
  digitalWriteFast(LED_BUILTIN, LOW);
  uint32_t micros_to_go = UWAIT - framecounter;
  uint32_t delay = micros_to_go / 1000;

  FastLED.delay(delay);
  framecounter -= (delay*1000);
  digitalWriteFast(LED_BUILTIN, HIGH);
}


#define PORTION .25   // portion of leds on ave that should be fading.
#define FADE_TIME 2
void add_blues(int i) {

  long probability = __LONG_MAX__ * PORTION / (FADE_TIME * FRAME_RATE);
  long num = random(__LONG_MAX__);

  if ((num < probability) && (fade_count[i] == 0)) {
    leds[i] = OUR_CYAN;
    fade_count[i] = FADE_TIME * FRAME_RATE;
  }
}

void apply_fade(int i) {
  if (fade_count[i] <= 0) {
    leds[i] = OUR_PURPLE;
    return;
  }
  uint8_t blend_prog = fade_count[i] * 256 / (FADE_TIME*FRAME_RATE);

  auto faded = blend(OUR_PURPLE, OUR_CYAN, blend_prog);
  fade_count[i]--;
  leds[i] = faded;
}


void sparkle(int i, float probability) {
  unsigned long l_probability = __LONG_MAX__ * probability;
  unsigned long num = random(__LONG_MAX__);

  if (num < l_probability ) {
    leds[i] = OUR_SPARKLE;
  }
}

void purble() {
  for (int i = 0; i < leds.len; i++) {
    add_blues(i);
    apply_fade(i);
  }
}

#define SPARKS_FADE_TIME 5.0
#define FRAMES (FRAME_RATE * SPARKS_FADE_TIME)
void sparks(uint16_t frame_count) {
  float prob = 4 * (frame_count / FRAMES) / FRAMES;
  for (int i = 0; i < leds.len; i++) {
    sparkle(i, prob);
  }
}


#define CH_FR 20  // pixels per second
#define MS_PER_PIX  (1000 / CH_FR)
#define PX_SPACE 10
void pink_chase(elapsedMillis phase) {

  int offset = phase / MS_PER_PIX;


  for (int i = 0; i < leds.len; i++) {
    if (((i - offset) % PX_SPACE) == 0) {
      leds[i] = OUR_PURPLE;
    } else {
      leds[i] = OUR_CYAN;
    }
  }
}

typedef enum {
  LEFT, TOP, RIGHT
} location_e;

#define HEIGHT 120
#define WIDTH (NUM_LEDS_PER_STRIP - (2*HEIGHT))
location_e get_pix_side(uint16_t pixel) {
  pixel %= NUM_LEDS_PER_STRIP;    // where in the strip we are

  if (pixel < HEIGHT) return LEFT;
  if (pixel < (HEIGHT + WIDTH)) return TOP;
  return RIGHT;

}

uint16_t get_height(uint16_t i) {
  location_e side = get_pix_side(i);
  uint16_t strip_loc = i % NUM_LEDS_PER_STRIP;
  switch(side) {
    case LEFT:
      return strip_loc;
    case TOP:
      return HEIGHT;
    case RIGHT:
      return NUM_LEDS_PER_STRIP - strip_loc;
  }
  return 0;
}

#define RISE_RATE 0.0    // LEDs per sec
#define SPACING   20.0     // Distance between chases

void rising_chase(elapsedMillis phase) {
  for (int i = 0; i < leds.len; i++) {

    if (get_pix_side(i) == TOP) {
    leds[i].nscale8(0);
    } else {
    uint16_t height = get_height(i);   // how many pixels up from the ground

    auto q = fmodf((2*RISE_RATE*phase/1000.0-(height-HEIGHT)), SPACING);
    auto brightness = max(0, abs(q-SPACING)-SPACING+1);

    leds[i] = OUR_CYAN;
    leds[i].nscale8_video(brightness*255);


    }
  }
}


elapsedMillis phase;

void loop() {
  static uint8_t current_setting;

  static uint8_t new_setting;
  new_setting = map(analogRead(PIN_A3), 0, 1023, 0, 9);
  // Serial.printf("%i\n", new_setting);
  if (current_setting != new_setting) phase = 0;

  current_setting = new_setting;

  switch(current_setting) {
    case 0:
      leds.fadeToBlackBy(2);
      break;
    case 1:
      purble();
      break;
    case 2:
      purble();
      sparks(300);
      break;
    case 3:
      sparks(300);
      leds.fadeToBlackBy(4);
      break;
    case 4:
      sparks(300);
      break;
    case 5:
      pink_chase(phase);
      sparks(500);
      break;
    case 6:
      pink_chase(phase);
      break;
    case 7:
      rising_chase(phase);
      sparks(200);
      break;
    default:
      leds.fadeToBlackBy(1);
  }

  send_leds();

}

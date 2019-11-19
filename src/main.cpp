#include <Arduino.h>
#include <vector>

#define HWSERIAL Serial1

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
#define OUR_GREEN CRGB(0,128,0)
#define OUR_SPARKLE CRGB::White

#if defined(FRAME_RATE)
  #define UWAIT ((1E6 / FRAME_RATE))
#else
  #define UWAIT 16666
#endif

CRGBArray<NUM_LEDS> leds;

uint16_t fade_count[NUM_LEDS];

void send_cmd(std::vector<uint8_t> cmd) {
  HWSERIAL.write(0x7e);
  HWSERIAL.write(cmd.size()+1);
  HWSERIAL.write(cmd.data(), cmd.size());
  HWSERIAL.write(0xef);
}

void mp3_init() {
  HWSERIAL.begin(9600);
  HWSERIAL.setTX(1);
  std::vector<uint8_t> set_device = {9, 1};
  std::vector<uint8_t> set_volume = {6, 0x14};
  send_cmd(set_device);
  send_cmd(set_volume);
}

void mp3_play_track(uint8_t track) {
  send_cmd({0x12, 1, track});
  send_cmd({0x0d});
}




void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  pinMode(18, OUTPUT);
  digitalWrite(18, HIGH);
  pinMode(PIN_A3, INPUT);

  Serial.begin(115200);
  mp3_init();
  delay(125);
  // FastLED.addLeds<NEOPIXEL, 2>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip).setDither(DISABLE_DITHER);
  FastLED.addLeds<OCTOWS2811>(leds, NUM_LEDS_PER_STRIP);
  FastLED.setCorrection(TypicalLEDStrip);

  FastLED.setMaxPowerInVoltsAndMilliamps(5,20000);

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
void pink_chase(elapsedMillis t_milli) {

  int offset = t_milli / MS_PER_PIX;


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
#define WIDTH 65
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
      return (strip_loc - HEIGHT) * (NUM_LEDS_PER_STRIP-HEIGHT-WIDTH-HEIGHT)/(WIDTH) + HEIGHT;
    case RIGHT:
      return NUM_LEDS_PER_STRIP - strip_loc;
  }
  return 0;
}

#define PORTAL_SPACING 20
#define PORTAL_SPEED   .1  // Seconds per portal

void portale(elapsedMillis t_milli) {
  int portal_distance = floor(fmodf((t_milli / 1000.0) / PORTAL_SPEED, PORTAL_SPACING));
  // Serial.printf("portal distance %i", portal_distance);
  for (int i = 0; i < leds.len; i++) {
    uint8_t portal_no = (i/NUM_LEDS_PER_STRIP);
    if (portal_no == portal_distance) {
      leds[i] = CRGB::Black;
    } else {
      uint32_t color = pow(20+i, 3);
      leds[i] = CHSV(color % 256, 255, 32);
    }
  }
}




#define SPACING   20.0     // Distance between chases

void rising_chase(elapsedMillis t_milli, float rise_rate) {
  for (int i = 0; i < leds.len; i++) {

    if (get_pix_side(i) == TOP) {
    leds[i].nscale8(0);
    } else {
    uint16_t height = get_height(i);   // how many pixels up from the ground

    auto q = fmodf((2*rise_rate*t_milli/1000.0-(height-HEIGHT)), SPACING);
    auto brightness = max(0, abs(q-SPACING)-SPACING+1);

    leds[i] = OUR_GREEN;
    leds[i].nscale8_video(brightness*255);


    }
  }
}

typedef enum {
  RISE_IN,
  RISING_GR,
  RISING_SPARKS,
  SPARKS,
  FADEOUT
} phase_e;

phase_e get_phase(elapsedMillis t_milli){
  if (t_milli < 5000) return RISE_IN;
  if (t_milli < 7000) return RISING_GR;
  if (t_milli < 12000) return RISING_SPARKS;
  if (t_milli < 20000) return SPARKS;
  return FADEOUT;
}

#define MAX_RR 20
void norm_sequence(elapsedMillis t_milli) {

  phase_e phase = get_phase(t_milli);

  float rise_rate =  map((int)t_milli, 0.0, 12000, 5, MAX_RR);
  switch (phase)
  {
  case RISE_IN:
    rising_chase(t_milli, rise_rate);
    leds.nblend(CRGB::Black, (255*(5000-t_milli)/5000.0));
    break;
  case RISING_GR:
    rising_chase(t_milli, rise_rate);
    break;
   case RISING_SPARKS:
    rising_chase(t_milli, rise_rate);
    sparks(200);
    break;
  case SPARKS:
    rising_chase(t_milli, MAX_RR);
    sparks(map((int)t_milli, 12000, 20000, 200, 10000));
    break;

  default:
    portale(t_milli);
    break;
  }



}


elapsedMillis t_milli;

void loop() {
  static uint8_t current_setting;

  static uint8_t new_setting;
  new_setting = map(analogRead(PIN_A3), 0, 1023, 0, 9);
  // Serial.printf("%i\n", new_setting);
  if (current_setting != new_setting) t_milli = 0;

  current_setting = new_setting;

  switch(current_setting) {
    case 0:
      leds.fadeToBlackBy(2);
      break;
    case 1:
      purble();
      sparks(50);
      break;
    case 2:
      portale(t_milli);
      break;
    case 3:
      mp3_play_track(2);
      norm_sequence(t_milli);
      break;


    default:
      leds.fadeToBlackBy(1);
  }

  send_leds();

}

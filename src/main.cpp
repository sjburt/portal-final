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
#define FRAME_RATE 60

#define OUR_PURPLE CRGB(CRGB::Purple).nscale8_video(64)
#define OUR_CYAN CRGB(CRGB::Cyan).nscale8_video(64)
#define OUR_GREEN CRGB(0,255,0);
#define OUR_SPARKLE CRGB::White * .75;

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
  std::vector<uint8_t> set_volume = {6, 0x10};
  // send_cmd({0x0c});
  send_cmd({0x0e});
  send_cmd(set_volume);
  send_cmd(set_device);
}

void mp3_play_track(uint8_t track) {
  send_cmd({0x12, 1, track});
}




void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  pinMode(18, OUTPUT);
  digitalWrite(18, HIGH);
  analogReadAveraging(32);
  pinMode(PIN_A3, INPUT);

  mp3_init();
  delay(125);
  // FastLED.addLeds<NEOPIXEL, 2>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip).setDither(DISABLE_DITHER);
  FastLED.addLeds<OCTOWS2811>(leds, NUM_LEDS_PER_STRIP);
  FastLED.setCorrection(TypicalLEDStrip);

  FastLED.setMaxPowerInVoltsAndMilliamps(5,40000);

  randomSeed(0);
}


elapsedMicros framecounter;

void send_leds() {
  FastLED.show();
  // digitalWriteFast(LED_BUILTIN, LOW);
  uint32_t micros_to_go = UWAIT - framecounter;
  uint32_t delay = micros_to_go / 1000;

  FastLED.delay(delay);
  framecounter -= (delay*1000);
  // digitalWriteFast(LED_BUILTIN, HIGH);
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

#define HEIGHT 115
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

#define PORTAL_SPACING 10
#define PORTAL_SPEED   .2  // Seconds per portal

void portale(elapsedMillis t_milli) {
  int portal_distance = floor(fmodf((t_milli / 1000.0) / PORTAL_SPEED, PORTAL_SPACING));
  static uint8_t p_seed[NUM_STRIPS];
  // Serial.printf("portal distance %i", portal_distance);
  for (int i = 0; i < leds.len; i++) {
    uint8_t portal_no = (i/NUM_LEDS_PER_STRIP);
    if (portal_no == portal_distance) {
      leds[i] = CRGB::White;
      p_seed[portal_no]++;
    } else {
      uint32_t color = pow(20+i+256, 3);
      leds[i] = CHSV(color % 256, 255, 128);
    }
  }
}


#define SPACING   10.0     // Distance between chases

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

void set_portal(int portal) {

  for (int i = 0; i < leds.len; i++) {
    if (portal == (i/NUM_LEDS_PER_STRIP)) {
      leds[i] = CRGB::DarkCyan;
    }
  }
}

typedef enum {
  PHASE_OUT,
  RISE_IN,
  RISING_GR,
  RISING_SPARKS,
  SPARKS,
  PORTAL,
  BLOOP,
} phase_e;

#define START_RISE 1000
#define FULL_PWR_RISE 4000
#define SPARKY_RISE 7000
#define FULL_ON 11200
#define ENTER 15000

phase_e get_phase(elapsedMillis t_milli){

  if (t_milli > 3000 && t_milli < 3050) return BLOOP;
  if (t_milli > 4400 && t_milli < 4450) return BLOOP;
  if (t_milli > 5700 && t_milli < 5750) return BLOOP;
  if (t_milli > 6100 && t_milli < 6200) return BLOOP;
  // if (t_milli > 7000 && t_milli < 7100) return BLOOP;
  // if (t_milli > 8000 && t_milli < 8100) return BLOOP;
  // if (t_milli > 8400 && t_milli < 8500) return BLOOP;

  if (t_milli < START_RISE) return PHASE_OUT;
  if (t_milli < FULL_PWR_RISE) return RISE_IN;
  if (t_milli < SPARKY_RISE) return RISING_GR;
  if (t_milli < FULL_ON) return RISING_SPARKS;
  if (t_milli < ENTER) return SPARKS;
  return PORTAL;
}

#define MAX_RR 20
void norm_sequence(elapsedMillis t_milli) {

  phase_e phase = get_phase(t_milli);
  static int portal_to_light;
  float rise_rate =  map((int)t_milli, (START_RISE * 1.0), SPARKY_RISE, 5, MAX_RR);
  switch (phase)
  {
  case PHASE_OUT: {
    // purble();
    leds.fadeToBlackBy(5);
    break;
  }
  case RISE_IN: {
    rising_chase(t_milli, rise_rate);
    uint8_t blend = map((int)t_milli, START_RISE*1.0, FULL_PWR_RISE, 255,0);
    leds.nblend(CRGB::Black, blend);
    break;
  }
  case RISING_GR:
    rising_chase(t_milli, rise_rate);
    break;
   case RISING_SPARKS:
    rising_chase(t_milli, rise_rate);
    sparks(200);
    break;
  case SPARKS:
    rising_chase(t_milli, MAX_RR);
    sparks(map((int)t_milli, (FULL_ON * 1.0), ENTER, 200, 10000));
    break;
  case BLOOP:
    set_portal(portal_to_light++);
    portal_to_light %= NUM_STRIPS;
  case PORTAL:
    portale(t_milli);
    break;
  default:
    purble();
  }
}


elapsedMillis t_milli;

void loop() {
  static uint8_t current_setting;
  static bool started = false;
  static uint8_t new_setting;
  new_setting = map(analogRead(PIN_A3), 0, 1023, 0, 9);

  if (current_setting != new_setting) { 
    t_milli = 0;
    started = true;
  }

  current_setting = new_setting;
  digitalWriteFast(LED_BUILTIN, LOW);
  switch(current_setting) {
    case 2:
      leds.fadeToBlackBy(1);
      break;
    case 1:
      purble();
      break;
    case 0:
      if (started) {
        mp3_play_track(1);
        started = false;
      }
      norm_sequence(t_milli);
      digitalWriteFast(LED_BUILTIN, HIGH);
      break;
    default:
      purble();
  }

  send_leds();
}

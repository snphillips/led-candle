#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#include <Adafruit_IS31FL3731.h>
#include "animationFlicker.h"
#include "animationNormal.h"

// ── Hardware configuration ────────────────────────────────────────────────
#define LED_DRIVER_ADDR  0x74
#define SDA_PIN          0
#define SCL_PIN          1

// ── IS31FL3731 register constants ─────────────────────────────────────────
#define REG_CMD          0xFD  // Command register (selects active page)
#define REG_FUNC_PAGE    0x0B  // Function register page
#define REG_PICTURE_DISP 0x01  // Picture Display register
#define REG_PWM_START    0x24  // First PWM data register
#define REG_PWM_END      0xB4  // Last PWM register (exclusive)
#define ANIM_EOD_MARKER  0x90  // Any byte >= this signals end-of-data

// ── Matrix dimensions ─────────────────────────────────────────────────────
#define MATRIX_WIDTH   9
#define MATRIX_HEIGHT  16

// ── Distance threshold ────────────────────────────────────────────────────
#define FLICKER_DISTANCE_MM  1200  // Switch to flicker below this distance

// ── Brightness ─────────────────────────────────────────────────────────────
// Global brightness scale applied to every PWM value before it's sent to
// the LED driver, as a percentage (0-100). Lower = dimmer. The IS31FL3731
// has no master brightness register, so this scales each byte in software.
#define BRIGHTNESS_PERCENT  55

// ── Objects ───────────────────────────────────────────────────────────────
Adafruit_VL53L0X lox       = Adafruit_VL53L0X();
Adafruit_IS31FL3731 ledmatrix = Adafruit_IS31FL3731();

// ── Animation state ───────────────────────────────────────────────────────
enum AnimationType { ANIM_NORMAL, ANIM_FLICKER };

static uint8_t            page      = 0;
static AnimationType      animation = ANIM_NORMAL;
static bool flickerPlaying = false;  // True while flicker runs to completion
static const uint8_t     *ptr       = animationNormal;
static uint8_t            img[MATRIX_WIDTH * MATRIX_HEIGHT];

// ── Low-level I2C helpers ─────────────────────────────────────────────────

// Begin an I2C write to the LED driver, targeting the given register.
// NOTE: leaves the transmission open — caller must write data then
// call Wire.endTransmission().
void beginRegisterWrite(uint8_t reg) {
  Wire.beginTransmission(LED_DRIVER_ADDR);
  Wire.write(reg);
}

// Select one of the IS31FL3731 frame pages (0–7) or REG_FUNC_PAGE.
void pageSelect(uint8_t n) {
  beginRegisterWrite(REG_CMD);
  Wire.write(n);
  Wire.endTransmission();
}

// Scale a raw 0-255 PWM value by BRIGHTNESS_PERCENT. Applied at
// transmission time so the source animation data itself stays untouched.
inline uint8_t scaleBrightness(uint8_t value) {
  return (uint16_t)value * BRIGHTNESS_PERCENT / 100;
}

// ── Animation ─────────────────────────────────────────────────────────────

// Switches the active animation and resets the data pointer to the
// beginning of the new animation. Call whenever the animation type changes.
void switchAnimation(AnimationType next) {
  animation = next;
  ptr       = (next == ANIM_FLICKER) ? animationFlicker : animationNormal;
}

// Returns true when the animation has just completed one full cycle.
bool runAnimation(const uint8_t *animationData) {
  bool cycleComplete = false;

  pageSelect(REG_FUNC_PAGE);
  beginRegisterWrite(REG_PICTURE_DISP);
  Wire.write(page);
  Wire.endTransmission();

  page ^= 1;

  uint8_t a = pgm_read_byte(ptr++);
  if (a >= ANIM_EOD_MARKER) {
    ptr          = animationData;
    a            = pgm_read_byte(ptr++);
    cycleComplete = true;             // ← EOD reached: one full cycle done
  }

  uint8_t x1 = a >> 4,   y1 = a & 0x0F;
  uint8_t b  = pgm_read_byte(ptr++);
  uint8_t x2 = b >> 4,   y2 = b & 0x0F;

  for (uint8_t x = x1; x <= x2; x++) {
    for (uint8_t y = y1; y <= y2; y++) {
      img[(x << 4) + y] = pgm_read_byte(ptr++);
    }
  }

  pageSelect(page);
  beginRegisterWrite(REG_PWM_START);

  uint8_t i = 0, byteCounter = 1;
  for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
    for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
      Wire.write(scaleBrightness(img[i++]));
      if (++byteCounter >= 32) {
        Wire.endTransmission();
        beginRegisterWrite(REG_PWM_START + i);
        byteCounter = 1;
      }
    }
  }
  Wire.endTransmission();

  return cycleComplete;
}

// ── Setup ─────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // LED_BUILTIN is used as a "flicker triggered" indicator in loop() —
  // must be configured as an output or digitalWrite() has no effect.
  pinMode(LED_BUILTIN, OUTPUT);

  // Initialise I2C with explicit pins at 400 KHz (IS31FL3731 maximum).
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  // NOTE: the previous manual IS31FL3731 init sequence (function-register
  // clear + per-page LED enable/PWM zeroing) has been removed. It was
  // fully redundant with ledmatrix.begin() below, which performs the same
  // setup through the Adafruit library, and contained a chunking bug that
  // could leave stray bits set in the blink-control registers (0x12–0x23).
  // If you need manual, library-independent init for some reason, restore
  // it carefully and make sure the chunk-boundary register address is
  // computed the same way runAnimation() does it (i.e. using the *post-
  // increment* index, not the raw loop variable).

  // ── VL53L0X init ──────────────────────────────────────────────────────
  if (!lox.begin()) {
    Serial.println(F("Failed to init VL53L0X"));
    while (1);
  }

  // ── LED matrix init (handles function-register clear, LED enable,
  //    and PWM/blink register zeroing internally) ─────────────────────
  if (!ledmatrix.begin(LED_DRIVER_ADDR)) {
    Serial.println(F("Failed to init LED matrix"));
    while (1);
  }

  ledmatrix.clear();
}

// ── Main loop ─────────────────────────────────────────────────────────────
void loop() {
  // Read the sensor every loop regardless — this paces the frame rate.
  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false);

  // If flicker is mid-cycle, keep running it and ignore the sensor result.
  if (flickerPlaying) {
    bool done = runAnimation(animationFlicker);
    if (done) {
      digitalWrite(LED_BUILTIN, LOW);
      flickerPlaying = false;
      switchAnimation(ANIM_NORMAL);
    }
    return;
  }

  // Sensor is only acted on when no flicker cycle is in progress.
  if (measure.RangeStatus == 4) return;

  if (measure.RangeMilliMeter <= FLICKER_DISTANCE_MM) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    switchAnimation(ANIM_FLICKER);
    flickerPlaying = true;
  } else {
    digitalWrite(LED_BUILTIN, LOW);
    if (animation != ANIM_NORMAL) switchAnimation(ANIM_NORMAL);
    runAnimation(animationNormal);
  }
}

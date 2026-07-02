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
      Wire.write(img[i++]);
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

  // Initialise I2C with explicit pins at 400 KHz (IS31FL3731 maximum).
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  // ── IS31FL3731 manual init ────────────────────────────────────────────
  // Clear Function Registers (leave Shutdown bit set — bit 10 of 13).
  pageSelect(REG_FUNC_PAGE);
  beginRegisterWrite(0x00);
  for (uint8_t i = 0; i < 13; i++) Wire.write(i == 10 ? 1 : 0);
  Wire.endTransmission();

  // Enable all LEDs and clear blink/PWM registers on both frame pages.
  for (uint8_t p = 0; p < 2; p++) {
    pageSelect(p);
    beginRegisterWrite(0x00);
    for (uint8_t i = 0; i < 18; i++) Wire.write(0xFF); // Enable all 144 LEDs

    uint8_t byteCounter = 19;
    for (uint8_t i = 18; i < REG_PWM_END; i++) {
      Wire.write(0); // Zero blink & PWM registers
      if (++byteCounter >= 32) {
        byteCounter = 1;
        Wire.endTransmission();
        beginRegisterWrite(i);
      }
    }
    Wire.endTransmission();
  }

  // ── VL53L0X init ──────────────────────────────────────────────────────
  if (!lox.begin()) {
    Serial.println(F("Failed to init VL53L0X"));
    while (1);
  }

  // ── LED matrix Adafruit init ──────────────────────────────────────────
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
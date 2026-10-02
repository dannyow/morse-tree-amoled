// Morse: a Morse code decoder tree. Touch anywhere on the screen to key: a short touch is a
// dot, a long one a dash, and a tone sounds while the finger is down. The tree on the screen
// follows the letter being keyed: dot nodes (circles) light green, dash nodes (rectangles)
// light red, the antenna lights yellow while keyed. The path clears after a pause.
#include <Arduino.h>
#include <Wire.h>
#include "platform/board.h"
#include "platform/font5x7.h"
#include "platform/hw/power.h"
#include "platform/hw/display.h"
#include "platform/hw/audio.h"
#include "platform/hw/touch.h"
#include "platform/hw/imu.h"
#include "platform/hw/i2c_lock.h"


#define RGB(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
static const uint16_t BG    = SWAP16(RGB(4, 8, 18));
static const uint16_t LINE  = SWAP16(RGB(190, 200, 215));
static const uint16_t OFFC  = SWAP16(RGB(52, 62, 40));
static const uint16_t GREEN = SWAP16(RGB(40, 230, 60));
static const uint16_t RED   = SWAP16(RGB(240, 50, 40));
static const uint16_t YEL   = SWAP16(RGB(255, 215, 40));
static const uint16_t TXT   = SWAP16(RGB(170, 180, 195));
static const uint16_t PADLN  = SWAP16(RGB(96, 106, 124));
static const uint16_t PADOFF = SWAP16(RGB(52, 58, 70));          // the centre circle: gray...
static const uint16_t PADON  = SWAP16(RGB(104, 112, 128));       // ...a little lighter while touched
static const uint16_t WHITE = SWAP16(RGB(240, 240, 240));

// ---- Timing and sound -------------------------------------------------------------------
static const uint32_t DOT_MAX_MS    = 200;    // a touch shorter than this is a dot, longer a dash
static const uint32_t UNIT_MS       = 300;    // one Morse time unit; the gaps below use the standard 3 and 7
static const uint32_t LETTER_GAP_MS = 3 * UNIT_MS;   // pause after the last symbol: the letter is done, the tree clears
static const uint32_t WORD_GAP_MS   = 7 * UNIT_MS;   // longer pause: the next letter starts a new word
static const uint32_t TEXT_RESET_MS = 5000;   // pause after the last symbol that clears the letters
static const int      MAX_TEXT      = 18;     // letters shown on the top line before the oldest drops off
static const char*    INSTRUCTION   = "TAP TO SEND MORSE";
static const float    TONE_LEVEL    = 0.6f;
static const float    TONE_HZ       = 1000.0f;
static const int      VOLUME_START  = 20;     // 0..100, linear in dB; the speaker gets loud
static const int      VOLUME_MAX    = 60;
static const int      VOLUME_STEP   = 5;

// ---- The tree: letters A..Z, root = the antenna -----------------------------------------
static const int ROOT = 26;
static const char* const CODE[26] = {
  ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---", "-.-", ".-..", "--",
  "-.", "---", ".--.", "--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--.."};

// Positions from the reference photo, in its own units; PX/PY scale them onto the panel.
// Columns and rows are snapped so every connecting line is straight.
static const int16_t POS[27][2] = {
  {505, 798}, {344, 1185}, {218, 925}, {344, 1053}, {505, 410}, {601, 667}, {218, 540},
  {793, 410}, {601, 410},  {505, 1185}, {218, 798}, {697, 798}, {218, 410}, {344, 798},
  {92, 410},  {601, 1053}, {92, 540},   {601, 798}, {697, 410}, {344, 410}, {601, 535},
  {697, 535}, {505, 1053}, {218, 1053}, {92, 798},  {218, 667}, {428, 410}};
// Where each letter is written (centre), again in photo units.
static const int16_t LBL[26][2] = {
  {452, 830}, {285, 1188}, {272, 897}, {402, 1022}, {505, 352}, {660, 667}, {282, 543},
  {793, 352}, {601, 352},  {560, 1187}, {218, 745}, {755, 800}, {218, 352}, {402, 765},
  {92, 352},  {660, 1050}, {92, 600},   {603, 860}, {697, 352}, {344, 352}, {546, 543},
  {745, 543}, {455, 1088}, {147, 1057}, {92, 745},  {272, 670}};

static const int S_PCT = 44, OX = 67, OY = 15;                        // photo units -> pixels
static inline int PX(int x) { return OX + ((x - 45) * S_PCT) / 100; }
static inline int PY(int y) { return OY + ((y - 205) * S_PCT) / 100; }
// The two halves of the tree are pushed apart to leave a lane in the middle for the key pad.
static const int SPLIT = 50;
static inline int PXS(int x) { return PX(x) + (x < 428 ? -SPLIT : x > 428 ? SPLIT : 0); }

struct Shape { int16_t cx, cy, hw, hh; bool circle; int16_t r; };   // r: corner radius of a rectangle
struct Edge { int16_t x0, y0, x1, y1; };
static Shape  gNode[26], gPad;
static Edge   gEdge[26];
static int16_t gLblX[26], gLblY[26];
static int    gChild[27][2];                                          // [node][0 dot, 1 dash] -> letter or -1
static int    gInstrX;
static int    gAntTop, gAntBot, gRootX, gRootY, gAntHW;

static void treeInit() {
  for (int i = 0; i < 27; i++) gChild[i][0] = gChild[i][1] = -1;
  for (int i = 0; i < 26; i++) {
    const char* c = CODE[i];
    int n = (int)strlen(c), parent = ROOT;
    if (n > 1) for (int j = 0; j < 26; j++) if ((int)strlen(CODE[j]) == n - 1 && strncmp(CODE[j], c, n - 1) == 0) parent = j;
    gChild[parent][c[n - 1] == '-' ? 1 : 0] = i;

    int cx = PXS(POS[i][0]), cy = PY(POS[i][1]), px = PXS(POS[parent][0]), py = PY(POS[parent][1]);
    bool vertical = (px == cx);                                       // rectangle follows the line it hangs from
    bool dash = c[n - 1] == '-';
    gNode[i] = dash ? Shape{(int16_t)cx, (int16_t)cy, (int16_t)(vertical ? 10 : 19), (int16_t)(vertical ? 19 : 9), false, 3}
                    : Shape{(int16_t)cx, (int16_t)cy, 14, 14, true, 0};
    gEdge[i] = {(int16_t)px, (int16_t)py, (int16_t)cx, (int16_t)cy};
    gLblX[i] = PXS(LBL[i][0]) - 5; gLblY[i] = PY(LBL[i][1]) - 7;       // 5x7 glyph at scale 2 is 10x14
  }
  gPad = Shape{(int16_t)PX(POS[ROOT][0]), (int16_t)((PY(430) + PY(1150)) / 2), 48, 48, true, 0};
  gRootX = PX(POS[ROOT][0]); gRootY = PY(POS[ROOT][1]);
  gInstrX = (LCD_W - ((int)strlen(INSTRUCTION) * 18 - 3)) / 2;       // scale 3: 18 px per character
  gAntTop = PY(300); gAntBot = PY(346); gAntHW = 19;
}

// ---- Decoder state: shared between the touch task, serial and the renderer --------------
static portMUX_TYPE gMux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t gLit = 0;                                    // bit i = letter i on the keyed path
static volatile int      gCur = ROOT;
static volatile bool     gKeyDown = false;
static volatile uint32_t gLastEvent = 0, gDownAt = 0;
static volatile bool     gSkipRelease = false;                        // a cancel while a finger is down: its release is not a symbol
static char gText[MAX_TEXT + 1];                                      // decoded letters, newest last; guarded by gMux
static int  gTextLen = 0;

static void tone(bool on) {
  AudioMix m = {};
  m.tone = on ? TONE_LEVEL : 0.0f; m.toneHz = TONE_HZ;
  audioSetMix(m);
}

// Walk one step down the tree; a step that leaves the tree (a fifth symbol, say) clears it.
static void feed(int sym, uint32_t t) {
  int now, was;
  portENTER_CRITICAL(&gMux);
  was = gCur;
  now = gChild[was][sym];
  if (now < 0) { gCur = ROOT; gLit = 0; }
  else { gCur = now; gLit |= 1u << now; }
  gLastEvent = t;
  portEXIT_CRITICAL(&gMux);
  if (now < 0) Serial.printf("%s -> outside the tree, cleared\n", sym ? "dash" : "dot");
  else Serial.printf("%s -> %c\n", sym ? "dash" : "dot", 'A' + now);
}

static void onEdge(bool down, uint32_t t) {
  if (down) { gDownAt = t; gKeyDown = true; gSkipRelease = false; tone(true); return; }
  gKeyDown = false; tone(false);
  if (gSkipRelease) { gSkipRelease = false; return; }
  uint32_t held = (t - gDownAt) + 10;                                 // the last frame ends about 10 ms after it starts
  feed(held > DOT_MAX_MS ? 1 : 0, t);
  Serial.printf("  held %lu ms\n", (unsigned long)held);
}

// Appends to the top line, dropping the oldest character when it is full. Hold gMux.
static void pushText(char c) {
  if (gTextLen == MAX_TEXT) { memmove(gText, gText + 1, MAX_TEXT - 1); gTextLen--; }
  gText[gTextLen++] = c; gText[gTextLen] = 0;
}

// Runs in the touch task, so it cannot cut in between a press and its release. Timed from the
// last symbol: the letter is committed to the top line and the tree clears (letter gap), later
// a space follows (word gap), and after a longer pause the letters clear and the instruction
// returns.
static void onTick(uint32_t now) {
  if (gKeyDown) return;
  int32_t idle = (int32_t)(now - gLastEvent);
  if (gCur != ROOT && idle > (int32_t)LETTER_GAP_MS) {
    int was;
    portENTER_CRITICAL(&gMux);
    was = gCur; gCur = ROOT; gLit = 0;
    pushText('A' + was);
    portEXIT_CRITICAL(&gMux);
    Serial.printf("letter %c\n", 'A' + was);
  } else if (gCur == ROOT && gTextLen > 0 && gText[gTextLen - 1] != ' ' && idle > (int32_t)WORD_GAP_MS) {
    portENTER_CRITICAL(&gMux);
    pushText(' ');
    portEXIT_CRITICAL(&gMux);
    Serial.println("word gap");
  } else if (gTextLen > 0 && idle > (int32_t)TEXT_RESET_MS) {
    portENTER_CRITICAL(&gMux);
    gTextLen = 0; gText[0] = 0;
    portEXIT_CRITICAL(&gMux);
    Serial.println("letters cleared");
  }
}

// ---- Rendering: spans per row, no framebuffer ---------------------------------------------
static inline void fill(uint16_t* row, int xa, int xb, uint16_t c) {
  if (xa < 0) xa = 0;
  if (xb >= LCD_W) xb = LCD_W - 1;
  for (int x = xa; x <= xb; x++) row[x] = c;
}
static int isqrt(int v) { int r = 0; while ((r + 1) * (r + 1) <= v) r++; return r; }

// Horizontal extent of `s`, grown by `grow` pixels, on row y. False when the row misses it.
static bool span(const Shape& s, int y, int grow, int& xa, int& xb) {
  int hh = s.hh + grow, hw = s.hw + grow, dy = abs(y - s.cy);
  if (dy > hh) return false;
  int inset = 0;
  if (s.circle) { int h = isqrt(hw * hw - dy * dy); xa = s.cx - h; xb = s.cx + h; return true; }
  int cr = s.r + grow, d = hh - dy;                                     // rounded corners
  if (cr > 0 && d < cr) { int e = cr - d; inset = cr - isqrt(cr * cr - e * e); }
  xa = s.cx - hw + inset; xb = s.cx + hw - inset;
  return true;
}

static void text(uint16_t* row, int y, int x0, int y0, int sc, const char* s, uint16_t col) {
  if (y < y0 || y >= y0 + 7 * sc) return;
  int gy = (y - y0) / sc;
  for (int i = 0; s[i]; i++) {
    const uint8_t* g = font5x7Glyph(s[i]);
    for (int c = 0; c < 5; c++) if ((g[c] >> gy) & 1)
      for (int k = 0; k < sc; k++) { int x = x0 + (i * 6 + c) * sc + k; if (x >= 0 && x < LCD_W) row[x] = col; }
  }
}

static void render(void*, uint16_t* dst, int y0, int rows) {
  uint32_t lit = gLit; bool key = gKeyDown;
  char top[MAX_TEXT + 1];
  portENTER_CRITICAL(&gMux); memcpy(top, gText, sizeof top); portEXIT_CRITICAL(&gMux);
  // A blinking dot after the letters says "the next letter goes here"; after a word gap it is an
  // underscore, "a new word starts here". Hidden while a letter is being keyed.
  bool cursor = top[0] && gCur == ROOT && !key && ((millis() / 400) & 1);
  for (int r = 0; r < rows; r++) {
    int y = y0 + r; uint16_t* row = dst + r * LCD_W;
    for (int x = 0; x < LCD_W; x++) row[x] = BG;

    if (top[0]) {                                                     // the letters keyed so far...
      text(row, y, 72, PY(215), 3, top, WHITE);
      if (cursor) text(row, y, 72 + (int)strlen(top) * 18, PY(215), 3, top[strlen(top) - 1] == ' ' ? "_" : ".", TXT);
    } else text(row, y, gInstrX, PY(215), 3, INSTRUCTION, TXT);       // ...or the instruction when idle

    { int xa, xb;                                                     // centre circle: lightens while touched
      if (span(gPad, y, 0, xa, xb)) { fill(row, xa, xb, PADLN); if (span(gPad, y, -2, xa, xb)) fill(row, xa, xb, key ? PADON : PADOFF); }
    }

    for (int i = 0; i < 26; i++) {                                    // connecting lines, 3 px wide
      const Edge& e = gEdge[i];
      if (e.y0 == e.y1) { if (y >= e.y0 - 1 && y <= e.y0 + 1) fill(row, min(e.x0, e.x1), max(e.x0, e.x1), LINE); }
      else if (y >= min(e.y0, e.y1) && y <= max(e.y0, e.y1)) fill(row, e.x0 - 1, e.x0 + 1, LINE);
    }

    if (y >= gAntTop && y <= gRootY) {                                // antenna: triangle on a stem
      if (y <= gAntBot) {
        int hw = gAntHW * (gAntBot - y) / (gAntBot - gAntTop);
        fill(row, gRootX - hw, gRootX + hw, LINE);
        if (y >= gAntTop + 2 && hw >= 3) fill(row, gRootX - hw + 3, gRootX + hw - 3, key ? YEL : BG);
      } else fill(row, gRootX - 1, gRootX + 1, key ? YEL : LINE);
    }

    for (int i = 0; i < 26; i++) {
      int xa, xb;
      if (!span(gNode[i], y, 0, xa, xb)) continue;
      fill(row, xa, xb, LINE);
      if (span(gNode[i], y, -2, xa, xb)) fill(row, xa, xb, ((lit >> i) & 1) ? (gNode[i].circle ? GREEN : RED) : OFFC);
    }

    for (int i = 0; i < 26; i++) { char s[2] = {(char)('A' + i), 0}; text(row, y, gLblX[i], gLblY[i], 2, s, TXT); }
  }
}

// ---- Side keys: KEY louder, BOOT quieter (held keys repeat), both together cancel -----------
static int gVolume = VOLUME_START;
static uint32_t gBeepUntil = 0;
static const uint32_t CHORD_MS = 60;          // after the first key goes down, wait this long for the other

static void volumeStep(int dir, uint32_t now) {
  int v = constrain(gVolume + dir * VOLUME_STEP, 0, VOLUME_MAX);
  if (v == gVolume) return;
  gVolume = v;
  audioVolume(gVolume);
  Serial.printf("volume %d\n", gVolume);
  tone(true); gBeepUntil = now + 120;                                 // a short beep at the new level
}

// Drops the keyed path and the letters on the top line, without waiting for the pauses.
static void cancelAll() {
  portENTER_CRITICAL(&gMux);
  gCur = ROOT; gLit = 0; gTextLen = 0; gText[0] = 0;
  gSkipRelease = gKeyDown;
  portEXIT_CRITICAL(&gMux);
  Serial.println("cancelled");
}

// A second finger on the screen: stop the tone and cancel, like both side keys together.
static void onTwoFingers(uint32_t t) {
  (void)t;
  tone(false);
  cancelAll();
  Serial.println("two fingers");
}

static void pollKeys(uint32_t now) {
  static bool up, dn, chord; static int pend = 0; static uint32_t pendAt, nextAt;
  bool u = digitalRead(PIN_KEY_IO10) == LOW, d = digitalRead(PIN_KEY_BOOT) == LOW;
  if (u && d) {
    if (!chord) { chord = true; pend = 0; cancelAll(); }
  } else if (chord) {
    if (!u && !d) chord = false;                                      // quiet until both keys are up again
  } else {
    int dir = u ? 1 : d ? -1 : 0;
    if (dir && !(up || dn)) { pend = dir; pendAt = now + CHORD_MS; }  // first contact: give the other key a moment
    if (pend && (!dir || (int32_t)(now - pendAt) >= 0)) { volumeStep(pend, now); pend = 0; nextAt = now + 400; }
    else if (!pend && dir && (int32_t)(now - nextAt) >= 0) { volumeStep(dir, now); nextAt = now + 150; }   // held: repeat
  }
  up = u; dn = d;
  if (gBeepUntil && (int32_t)(now - gBeepUntil) >= 0) { gBeepUntil = 0; if (!gKeyDown) tone(false); }
}

// ---- Orientation: the screen follows gravity so the top of the picture is the top of the module
// ROT[i] = panel rotation (MADCTL) for the module held with orientation i, i.e. with the edge
// the sensor reads as "up" (hwImuAccelRaw gives the up vector) at the top:
//   0: speaker edge up (ax < 0)   1: ay > 0   2: ax > 0   3: ay < 0
// Measured on the bench with the speaker edge up: 0x90 puts the top of the picture up, 0xF0
// puts it on the right, 0x50 down (and so 0x30 left). The sensor reads +x toward the bottom
// of the screen and +y toward its left. All four orientations are confirmed on the bench.
static const uint8_t ROT[4] = {0x90, 0x30, 0x50, 0xF0};
static int gOrient = 0;                                               // boots as 0x90, the old fixed orientation

static int orientationOf(float ax, float ay) {
  if (fabsf(ax) > fabsf(ay)) return ax < 0 ? 0 : 2;
  return ay > 0 ? 1 : 3;
}

static void pollOrientation(uint32_t now) {
  static uint32_t last = 0, since = 0; static int cand = 0; static float fx = 0, fy = 0, fz = 1; static bool have = false;
  if (now - last < 100) return;
  last = now;
  float ax, ay, az;
  i2cLock(); bool ok = hwImuAccelRaw(&ax, &ay, &az); i2cUnlock();
  if (!ok) return;
  if (!have) { fx = ax; fy = ay; fz = az; have = true; }
  fx += 0.3f * (ax - fx); fy += 0.3f * (ay - fy); fz += 0.3f * (az - fz);
  if (fabsf(fz) > 0.8f || fmaxf(fabsf(fx), fabsf(fy)) < 0.6f) { since = 0; return; }   // lying flat, or no clear "up": keep what we have
  int o = orientationOf(fx, fy);
  if (o == gOrient) { since = 0; return; }
  if (!since || o != cand) { cand = o; since = now; return; }
  if (now - since < 600) return;                                      // must hold steady; a knock or a key press must not flip it
  gOrient = o; since = 0;
  hwDisplayMadctl(ROT[gOrient]);
  Serial.printf("orientation %d -> MADCTL 0x%02X (x=%.2f y=%.2f z=%.2f)\n", gOrient, ROT[gOrient], fx, fy, fz);
}

static void die(const char* what) { Serial.printf("FATAL: %s\n", what); while (true) delay(1000); }

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println("\n=== morse boot ===");
  treeInit();
  pinMode(PIN_TP_RESET, OUTPUT);
  pinMode(PIN_KEY_IO10, INPUT_PULLUP);
  pinMode(PIN_KEY_BOOT, INPUT_PULLUP);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);        // the one I2C init; drivers reuse the bus
  Wire.setClock(400000);
  if (!hwPowerInit()) die("power");            // PMU first: it powers the panel
  hwPowerPanelReset();
  digitalWrite(PIN_TP_RESET, LOW); delay(20); digitalWrite(PIN_TP_RESET, HIGH); delay(50);
  if (!hwDisplayInit()) die("display");
  hwDisplayMadctl(ROT[gOrient]);
  hwDisplayBrightness(200);
  if (!hwAudioInit()) Serial.println("audio: DISABLED");
  audioVolume(gVolume);
  if (!hwImuInit()) Serial.println("imu: DISABLED (no auto-rotate)");
  if (!hwTouchInit(onEdge, onTick, onTwoFingers)) die("touch");
  Serial.println("morse: ready. Touch to key, two fingers or both side keys cancel");
}

void loop() {
  uint32_t now = millis();
  pollKeys(now);
  pollOrientation(now);
  hwDisplayPushFrame(render, nullptr);         // one QSPI transaction per frame (see display.h)
}

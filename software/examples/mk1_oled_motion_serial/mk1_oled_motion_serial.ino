#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_PWMServoDriver.h>

// ================================================================
//  PERRITO ROBOT Mk.I  —  ESP32-C6 + PCA9685 + OLED SH1106
// ================================================================

// ===== OLED =====
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ===== PCA9685 =====
Adafruit_PWMServoDriver pca = Adafruit_PWMServoDriver(0x40);

#define SERVOMIN 110   // pulso mínimo (~0°)
#define SERVOMAX 520   // pulso máximo (~180°)

// ── Canales PCA9685 ──────────────────────────────────────────────
// Ajusta estos números según cómo conectaste cada servo al PCA9685
#define PATA_FL 1//1   // Front-Left  (delantera izquierda)
#define PATA_FR 0//0   // Front-Right (delantera derecha)
#define PATA_BL 3   // Back-Left   (trasera izquierda)
#define PATA_BR 2   // Back-Right  (trasera derecha)

// ── Inversión por servo (true = servo montado físicamente al revés) ──
bool invertido[4] = {false, true, false, true};

// ── Offset de calibración en grados (ajusta si alguna pata no queda recta) ──
int offsetServo[4] = {0, 0, 0, 0};

// ── Ángulos de posición ──────────────────────────────────────────
#define ANGULO_NEUTRO   90   // pata recta / reposo
#define ANGULO_ADELANTE 60   // pata empujada hacia adelante
#define ANGULO_ATRAS   120   // pata empujada hacia atrás

float anguloActual[4] = {90, 90, 90, 90};
float anguloObjetivo[4] = {90, 90, 90, 90};

const float VELOCIDAD = 3.0; // qué tan rápido alcanza el objetivo

// ================================================================
//  ESTADO GLOBAL
// ================================================================
String emocionActual = "happy";
String marchaActual  = "stop";   // "stop" | "walk" | "trot"
int    animFrame     = 0;
int    marchaStep    = 0;        // fase actual del ciclo de marcha

// ================================================================
//  OJOS  (centrados verticalmente — sin boca)
// ================================================================
const int eyeLX  = 38;
const int eyeRX  = 90;
const int eyeY   = 32;   // centro vertical en pantalla de 64px
const int eyeW   = 15;
const int eyeH   = 12;
const int pupilR = 5;

int lookX = 0;
int lookY = 0;

int blinkState   = 0;
int blinkCounter = 0;

// ================================================================
//  HELPERS — SERVO
// ================================================================
uint16_t anguloToPWM(int angulo) {
  return map(angulo, 0, 180, SERVOMIN, SERVOMAX);
}

void moverServo(int canal, int angulo) {
  if (invertido[canal]) angulo = 180 - angulo;
  angulo += offsetServo[canal];
  angulo  = constrain(angulo, 0, 180);
  pca.setPWM(canal, 0, anguloToPWM(angulo));
}

void todosNeutro() {
  for (int i = 0; i < 4; i++) moverServo(i, ANGULO_NEUTRO);
}

void actualizarServosSuave() {
  for (int i = 0; i < 4; i++) {
    float diff = anguloObjetivo[i] - anguloActual[i];

    if (abs(diff) > 0.5) {
      anguloActual[i] += diff / VELOCIDAD;
    }

    moverServo(i, (int)anguloActual[i]);
  }
}

// ================================================================
//  MARCHA
//
//  Walk  — una pata a la vez (más lento, más estable)
//  Trot  — pares diagonales FL+BR y FR+BL alternados (más rápido)
//
//  Cada fase dura MARCHA_MS milisegundos.
//  Columnas: [FL, FR, BL, BR]
// ================================================================
unsigned long ultimoPasoMarcha = 0;
const unsigned long MARCHA_MS  = 200;  // ms por fase — baja para ir más rápido

/*const int secuenciaWalk[4][4] = {
  {ANGULO_ADELANTE, ANGULO_NEUTRO,    ANGULO_NEUTRO,    ANGULO_NEUTRO},
  {ANGULO_NEUTRO,   ANGULO_NEUTRO,    ANGULO_NEUTRO,    ANGULO_ADELANTE},
  {ANGULO_NEUTRO,   ANGULO_ADELANTE,  ANGULO_NEUTRO,    ANGULO_NEUTRO},
  {ANGULO_NEUTRO,   ANGULO_NEUTRO,    ANGULO_ADELANTE,  ANGULO_NEUTRO},
};*/
const int secuenciaWalk[4][4] = {
  {120, 60, 60, 60},
  {60, 120, 60, 60},
  {60, 60, 120, 60},
  {70, 70, 60, 120}
};

const int secuenciaTrot[4][4] = {
  {ANGULO_ADELANTE, ANGULO_NEUTRO,    ANGULO_NEUTRO,    ANGULO_ADELANTE},
  {ANGULO_NEUTRO,   ANGULO_NEUTRO,    ANGULO_NEUTRO,    ANGULO_NEUTRO},
  {ANGULO_NEUTRO,   ANGULO_ADELANTE,  ANGULO_ADELANTE,  ANGULO_NEUTRO},
  {ANGULO_NEUTRO,   ANGULO_NEUTRO,    ANGULO_NEUTRO,    ANGULO_NEUTRO},
};

void actualizarMarcha() {
  if (marchaActual == "stop") return;

  unsigned long ahora = millis();
  if (ahora - ultimoPasoMarcha < MARCHA_MS) return;
  ultimoPasoMarcha = ahora;

  const int (*seq)[4] = (marchaActual == "trot") ? secuenciaTrot : secuenciaWalk;

  anguloObjetivo[PATA_FL] = seq[marchaStep][0];
  anguloObjetivo[PATA_FR] = seq[marchaStep][1];
  anguloObjetivo[PATA_BL] = seq[marchaStep][2];
  anguloObjetivo[PATA_BR] = seq[marchaStep][3];

  marchaStep = (marchaStep + 1) % 4;
}

// ================================================================
//  POSES Y ACCIONES
// ================================================================

// ---------- NEUTRO ----------
void poseNeutra() {

  marchaActual = "stop";

  anguloObjetivo[PATA_FL] = ANGULO_NEUTRO;
  anguloObjetivo[PATA_FR] = ANGULO_NEUTRO;
  anguloObjetivo[PATA_BL] = ANGULO_NEUTRO;
  anguloObjetivo[PATA_BR] = ANGULO_NEUTRO;

  emocionActual = "happy";
}

// ---------- SENTARSE ----------
void sentarse() {

  marchaActual = "stop";

  // delanteras un poco hacia atrás
  anguloObjetivo[PATA_FL] = 60;
  anguloObjetivo[PATA_FR] = 60;

  // traseras hacia adelante
  anguloObjetivo[PATA_BL] = 155;
  anguloObjetivo[PATA_BR] = 155;

  emocionActual = "happy";
}

// ---------- DORMIR ----------
void dormir() {

  marchaActual = "stop";

  // delanteras extendidas
  anguloObjetivo[PATA_FL] = 165;
  anguloObjetivo[PATA_FR] = 165;

  // traseras estiradas atrás
  anguloObjetivo[PATA_BL] = 15;
  anguloObjetivo[PATA_BR] = 15;

  emocionActual = "sleepy";
}

// ---------- SALUDO ----------
void saludar() {

  marchaActual = "stop";
  sentarse();
  unsigned long t0 = millis();

  while (millis() - t0 < 1200) {
    actualizarServosSuave();
    mostrarEmocion();
  }

  emocionActual = "wink";

  // ------------------------------------------------
  // 1. Primero apoyar el cuerpo
  // ------------------------------------------------

  // pata frontal derecha soporta peso
  anguloObjetivo[PATA_FR] = 100;

  // patas traseras estabilizan
  anguloObjetivo[PATA_BL] = 70;
  anguloObjetivo[PATA_BR] = 140;

  // la pata que saludará aún NO se mueve
  anguloObjetivo[PATA_FL] = 110;

  // esperar a que el cuerpo se estabilice
  t0 = millis();

  while (millis() - t0 < 900) {
    actualizarServosSuave();
    mostrarEmocion();
  }

  // ------------------------------------------------
  // 2. Levantar lentamente la pata izquierda
  // ------------------------------------------------

  anguloObjetivo[PATA_FL] = 160;

  t0 = millis();

  while (millis() - t0 < 700) {
    actualizarServosSuave();
    mostrarEmocion();
  }

  // ------------------------------------------------
  // 3. Movimiento de saludo
  // ------------------------------------------------

  for (int i = 0; i < 3; i++) {

    // subir
    anguloObjetivo[PATA_FL] = 170;

    t0 = millis();

    while (millis() - t0 < 350) {
      actualizarServosSuave();
      mostrarEmocion();
    }

    // bajar un poco
    anguloObjetivo[PATA_FL] = 135;

    t0 = millis();

    while (millis() - t0 < 350) {
      actualizarServosSuave();
      mostrarEmocion();
    }
  }

  // ------------------------------------------------
  // 4. Regresar a neutro
  // ------------------------------------------------

  poseNeutra();
}

// ================================================================
//  HELPERS — OLED
// ================================================================
void drawEye(int cx, int cy, int aperturaH, int px, int py) {
  if (aperturaH <= 1) {
    u8g2.drawLine(cx - eyeW, cy,   cx + eyeW, cy);
    u8g2.drawLine(cx - eyeW, cy+1, cx + eyeW, cy+1);
    return;
  }
  for (int dy = -aperturaH; dy <= aperturaH; dy++) {
    float ratio = 1.0f - ((float)(dy*dy)) / ((float)(aperturaH*aperturaH));
    int hw = (int)(eyeW * sqrt(ratio));
    u8g2.drawLine(cx - hw, cy + dy, cx + hw, cy + dy);
  }
  u8g2.setDrawColor(0);
  int pcx = cx + px, pcy = cy + py;
  for (int dy = -pupilR; dy <= pupilR; dy++)
    for (int dx = -pupilR; dx <= pupilR; dx++)
      if (dx*dx + dy*dy <= pupilR*pupilR)
        u8g2.drawPixel(pcx + dx, pcy + dy);
  u8g2.setDrawColor(1);
  u8g2.drawPixel(pcx - 2, pcy - 2);
}

void drawEyebrowL(int tipo) {
  int bx = eyeLX, by = eyeY - eyeH - 6;
  switch (tipo) {
    case 0:
      u8g2.drawLine(bx-11, by,   bx+11, by);
      u8g2.drawLine(bx-11, by+1, bx+11, by+1);
      break;
    case 1:
      u8g2.drawLine(bx-11, by+3, bx,    by);
      u8g2.drawLine(bx,    by,   bx+11, by+3);
      u8g2.drawLine(bx-11, by+4, bx,    by+1);
      u8g2.drawLine(bx,    by+1, bx+11, by+4);
      break;
    case 2:
      u8g2.drawLine(bx-11, by-2, bx+11, by+5);
      u8g2.drawLine(bx-11, by-1, bx+11, by+6);
      break;
    case 3:
      u8g2.drawLine(bx-11, by-5, bx+11, by-5);
      u8g2.drawLine(bx-11, by-4, bx+11, by-4);
      break;
    case 4:
      u8g2.drawLine(bx-11, by+4, bx+11, by-1);
      u8g2.drawLine(bx-11, by+5, bx+11, by);
      break;
  }
}

void drawEyebrowR(int tipo) {
  int bx = eyeRX, by = eyeY - eyeH - 6;
  switch (tipo) {
    case 0:
      u8g2.drawLine(bx-11, by,   bx+11, by);
      u8g2.drawLine(bx-11, by+1, bx+11, by+1);
      break;
    case 1:
      u8g2.drawLine(bx-11, by+3, bx,    by);
      u8g2.drawLine(bx,    by,   bx+11, by+3);
      u8g2.drawLine(bx-11, by+4, bx,    by+1);
      u8g2.drawLine(bx,    by+1, bx+11, by+4);
      break;
    case 2:
      u8g2.drawLine(bx-11, by+5, bx+11, by-2);
      u8g2.drawLine(bx-11, by+6, bx+11, by-1);
      break;
    case 3:
      u8g2.drawLine(bx-11, by-5, bx+11, by-5);
      u8g2.drawLine(bx-11, by-4, bx+11, by-4);
      break;
    case 4:
      u8g2.drawLine(bx-11, by-1, bx+11, by+4);
      u8g2.drawLine(bx-11, by,   bx+11, by+5);
      break;
  }
}

// ================================================================
//  PARPADEO
// ================================================================
void actualizarParpadeo() {
  blinkCounter++;
  if      (blinkCounter > 60 && blinkCounter < 63) blinkState = 1;
  else if (blinkCounter < 66)                       blinkState = 2;
  else if (blinkCounter < 69)                       blinkState = 3;
  else                                              blinkState = 0;
  if (blinkCounter > 120) blinkCounter = 0;
}

int calcApertura(int base, bool parpadear) {
  if (!parpadear) return base;
  if (blinkState == 1) return base / 2;
  if (blinkState == 2) return 1;
  if (blinkState == 3) return base / 2;
  return base;
}

// ================================================================
//  EMOCIONES  (solo ojos + cejas, sin boca)
// ================================================================
void ojosHappy() {
  int ap = calcApertura(eyeH, true);
  u8g2.clearBuffer();
  drawEyebrowL(1); drawEyebrowR(1);
  drawEye(eyeLX, eyeY, ap, lookX, lookY);
  drawEye(eyeRX, eyeY, ap, lookX, lookY);
  u8g2.sendBuffer();
}

void ojosAngry() {
  int ap = calcApertura(eyeH - 3, false);
  u8g2.clearBuffer();
  drawEyebrowL(2); drawEyebrowR(2);
  drawEye(eyeLX, eyeY, ap, lookX, lookY);
  drawEye(eyeRX, eyeY, ap, lookX, lookY);
  u8g2.sendBuffer();
}

void ojosSleepy() {
  u8g2.clearBuffer();
  drawEyebrowL(0); drawEyebrowR(0);
  drawEye(eyeLX, eyeY, 3, 0, 0);
  drawEye(eyeRX, eyeY, 3, 0, 0);
  u8g2.setFont(u8g2_font_6x10_tr);
  int zOff = animFrame % 18;
  u8g2.drawStr(100, 40 - zOff,     "z");
  u8g2.drawStr(107, 34 - zOff / 2, "Z");
  u8g2.drawStr(114, 28 - zOff / 3, "Z");
  u8g2.sendBuffer();
}

void ojosSurprised() {
  u8g2.clearBuffer();
  drawEyebrowL(3); drawEyebrowR(3);
  drawEye(eyeLX, eyeY, eyeH + 4, 0, 0);
  drawEye(eyeRX, eyeY, eyeH + 4, 0, 0);
  u8g2.sendBuffer();
}

void ojosWink() {
  int ap = calcApertura(eyeH, false);
  bool cerrado = (animFrame % 30) < 15;
  u8g2.clearBuffer();
  drawEyebrowL(1); drawEyebrowR(1);
  drawEye(eyeRX, eyeY, ap, lookX, lookY);
  if (cerrado) {
    u8g2.drawLine(eyeLX - eyeW, eyeY,   eyeLX + eyeW, eyeY);
    u8g2.drawLine(eyeLX - eyeW, eyeY+1, eyeLX + eyeW, eyeY+1);
  } else {
    drawEye(eyeLX, eyeY, ap, lookX, lookY);
  }
  u8g2.sendBuffer();
}

void ojosAngryWink() {
  u8g2.clearBuffer();
  drawEyebrowL(2); drawEyebrowR(2);
  drawEye(eyeLX, eyeY, eyeH - 2, lookX, lookY);
  u8g2.drawLine(eyeRX - eyeW, eyeY,   eyeRX + eyeW, eyeY);
  u8g2.drawLine(eyeRX - eyeW, eyeY+1, eyeRX + eyeW, eyeY+1);
  u8g2.sendBuffer();
}

// ================================================================
//  MIRADA
// ================================================================
void mirar(int x, int y) {
  lookX = constrain(x, -5, 5);
  lookY = constrain(y, -3, 3);
}

void mostrarEmocion() {
  actualizarParpadeo();
  if      (emocionActual == "happy")      ojosHappy();
  else if (emocionActual == "sleepy")     ojosSleepy();
  else if (emocionActual == "surprised")  ojosSurprised();
  else if (emocionActual == "wink")       ojosWink();
  else if (emocionActual == "angry")      ojosAngry();
  else if (emocionActual == "angrywink")  ojosAngryWink();
  else                                    ojosHappy();
}

// ================================================================
//  SETUP
// ================================================================
void setup() {
  Serial.begin(115200);
  Wire.begin(6, 7);

  u8g2.begin();
  u8g2.setContrast(255);

  pca.begin();
  pca.setPWMFreq(50);

  todosNeutro();

  Serial.println("==============================");
  Serial.println("   PERRITO ROBOT Mk.I  ");
  Serial.println("==============================");
  Serial.println("── EMOCIONES ──");
  Serial.println("  1 = Happy");
  Serial.println("  2 = Sleepy");
  Serial.println("  3 = Surprised");
  Serial.println("  4 = Wink");
  Serial.println("  5 = Angry");
  Serial.println("  6 = Angry Wink (easter egg)");
  Serial.println("── MARCHA ──");
  Serial.println("  w = Walk (una pata a la vez)");
  Serial.println("  t = Trot (diagonal, mas rapido)");
  Serial.println("  s = Stop");
  Serial.println("── POSES ──");
  Serial.println("  p = Pose neutra");
  Serial.println("  d = Dormir");
  Serial.println("  k = Sentarse");
  Serial.println("  h = Saludar");
  Serial.println("==============================");
}

// ================================================================
//  LOOP
// ================================================================
unsigned long ultimoFrame = 0;
const unsigned long FRAME_MS = 40;  // ~25 fps OLED

void loop() {

  // ── 1. SERIAL ────────────────────────────────────────────────
  while (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == '\r' || cmd == '\n') continue;

    switch (cmd) {
      // Emociones
      case '1': emocionActual = "happy";     Serial.println(">> Happy");          break;
      case '2': emocionActual = "sleepy";    Serial.println(">> Sleepy");         break;
      case '3': emocionActual = "surprised"; Serial.println(">> Surprised");      break;
      case '4': emocionActual = "wink";      Serial.println(">> Wink");           break;
      case '5': emocionActual = "angry";     Serial.println(">> Angry");          break;
      case '6': emocionActual = "angrywink"; Serial.println(">> Angry Wink >:)"); break;

      // Marcha
      case 'w': case 'W':
        marchaActual = "walk"; marchaStep = 0;
        Serial.println(">> Walk");
        break;
      case 't': case 'T':
        marchaActual = "trot"; marchaStep = 0;
        Serial.println(">> Trot");
        break;
      case 's': case 'S':
        marchaActual = "stop";
        poseNeutra();
        Serial.println(">> Stop");
        break;
      // POSES
      case 'p': case 'P':
        poseNeutra();
        Serial.println(">> Pose neutra");
        break;
      case 'd': case 'D':
        dormir();
        Serial.println(">> Dormir");
        break;
      case 'k': case 'K':
        sentarse();
        Serial.println(">> Sentarse");
        break;
      case 'h': case 'H':
        saludar();
        Serial.println(">> Saludar");
        break;
      default:
      if (cmd >= 32 && cmd < 127) {
        Serial.print(">> Desconocido: ");
        Serial.println(cmd);
      }
      break;
    }
  }

  // ── 2. MARCHA (timing propio) ─────────────────────────────────
  actualizarMarcha();
  actualizarServosSuave();

  // ── 3. RENDER OLED (~25 fps) ──────────────────────────────────
  unsigned long ahora = millis();
  if (ahora - ultimoFrame < FRAME_MS) return;
  ultimoFrame = ahora;

  animFrame++;

  if (animFrame % 80 == 0) {
    int r = random(0, 5);
    if (r == 0) mirar(-3, 0);
    if (r == 1) mirar(3, 0);
    if (r == 2) mirar(0, -2);
    if (r == 3) mirar(0, 2);
    if (r == 4) mirar(0, 0);
  }

  mostrarEmocion();
}

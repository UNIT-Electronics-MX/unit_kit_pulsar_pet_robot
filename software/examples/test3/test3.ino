#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_PWMServoDriver.h>

// ========== OLED SH1106 DISPLAY CONFIGURATION ==========
// Display OLED 128x64 1.3" I2C SH1106
// Rotation: U8G2_R0=0°, U8G2_R1=90°, U8G2_R2=180°, U8G2_R3=270°
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, /* reset=*/ U8X8_PIN_NONE);

// ========== SERVO CONFIGURATION ==========
Adafruit_PWMServoDriver pca = Adafruit_PWMServoDriver(0x40);

#define SERVOMIN 110
#define SERVOMAX 520

#define S1 0  // front left
#define S2 1  // rear left
#define S3 2  // front right
#define S4 3  // rear right

bool invertido[4] = {true, true, false, false};
int offset[4] = {0, 0, 0, 0};

// Positions
int centro = 90;
int adelante = 160;
int atras = 80;

// State variables
String estadoActual = "Neutro";
String emocionActual = "happy";
int pasosCaminados = 0;
int animFrame = 0;

// ========== SERVO FUNCTIONS ==========
uint16_t anguloToPWM(int angulo) {
  return map(angulo, 0, 180, SERVOMIN, SERVOMAX);
}

void moverServo(int servo, int angulo) {
  if (invertido[servo]) angulo = 180 - angulo;
  angulo += offset[servo];
  angulo = constrain(angulo, 0, 180);
  pca.setPWM(servo, 0, anguloToPWM(angulo));
}

void neutro() {
  estadoActual = "Neutro";
  emocionActual = "happy";
  for (int i = 0; i < 4; i++) {
    moverServo(i, centro);
  }
}

void caminaPaso() {
  estadoActual = "Walking";
  emocionActual = "happy";
  
  // Smooth alternating movement - reduced range
  moverServo(S1, 110);  // front left - forward
  moverServo(S4, 110);  // rear right - forward
  moverServo(S2, 70);   // rear left - backward
  moverServo(S3, 70);   // front right - backward
  delay(250);

  moverServo(S2, 110);  // rear left - forward
  moverServo(S3, 110);  // front right - forward
  moverServo(S1, 70);   // front left - backward
  moverServo(S4, 70);   // rear right - backward
  delay(250);
  
  pasosCaminados++;
}

void sentado() {
  estadoActual = "Sitting";
  emocionActual = "sleepy";
  moverServo(S1, atras);
  moverServo(S3, atras);
  moverServo(S2, adelante);
  moverServo(S4, adelante);
}

void saluda() {
  estadoActual = "Waving";
  emocionActual = "love";
  
  moverServo(S2, atras);
  moverServo(S4, atras);
  moverServo(S1, adelante);
  delay(300);

  for (int i = 0; i < 4; i++) {
    moverServo(S1, adelante);
    delay(200);
    moverServo(S1, centro);
    delay(200);
  }

  neutro();
}

// ========== CRAWL WALK FUNCTIONS (TEST2) ==========

// Complete crawl cycle - smooth movements
void caminaArrastre() {
  estadoActual = "Crawling";
  emocionActual = "happy";
  
  // 1. Front legs centered, rear legs push
  moverServo(S1, centro);
  moverServo(S3, centro);
  moverServo(S2, 110);  // rear backward (soft)
  moverServo(S4, 110);
  delay(350);

  // 2. Front legs stretch forward
  moverServo(S1, 70);   // forward (soft)
  moverServo(S3, 70);
  delay(300);

  // 3. Rear legs return (pulling body forward)
  moverServo(S2, 70);   // forward (soft)
  moverServo(S4, 70);
  delay(350);
  
  pasosCaminados++;
}

// ========== FACE DRAWING FUNCTIONS ==========

// Draw filled circle
void fillCircle(int x0, int y0, int r) {
  for (int y = -r; y <= r; y++) {
    for (int x = -r; x <= r; x++) {
      if (x*x + y*y <= r*r) {
        u8g2.drawPixel(x0 + x, y0 + y);
      }
    }
  }
}

// 😊 HAPPY FACE KAWAII
void dibujarHappy() {
  u8g2.clearBuffer();
  
  // Big kawaii-style eyes
  fillCircle(40, 30, 16);
  fillCircle(88, 30, 16);
  
  // Big sparkles in eyes (white circles)
  u8g2.setDrawColor(0);
  fillCircle(40, 30, 7);
  fillCircle(88, 30, 7);
  // Small additional sparkle
  fillCircle(44, 26, 3);
  fillCircle(92, 26, 3);
  u8g2.setDrawColor(1);
  
  // Tiny kawaii mouth
  u8g2.drawCircle(64, 52, 6, U8G2_DRAW_LOWER_RIGHT | U8G2_DRAW_LOWER_LEFT);
  
  u8g2.sendBuffer();
}

// 😢 SAD FACE KAWAII
void dibujarSad() {
  u8g2.clearBuffer();
  
  // Big sad eyes
  fillCircle(40, 32, 14);
  fillCircle(88, 32, 14);
  
  u8g2.setDrawColor(0);
  fillCircle(40, 36, 6);
  fillCircle(88, 36, 6);
  fillCircle(44, 30, 2);
  fillCircle(92, 30, 2);
  u8g2.setDrawColor(1);
  
  // Small tear
  fillCircle(46, 40, 2);
  
  // Sad eyebrows
  u8g2.drawLine(28, 16, 52, 22);
  u8g2.drawLine(76, 22, 100, 16);
  
  // Tiny sad mouth
  u8g2.drawCircle(64, 56, 5, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_UPPER_LEFT);
  
  u8g2.sendBuffer();
}

// 😠 ANGRY FACE KAWAII
void dibujarAngry() {
  u8g2.clearBuffer();
  
  // Big intense eyes
  fillCircle(40, 32, 13);
  fillCircle(88, 32, 13);
  
  u8g2.setDrawColor(0);
  fillCircle(40, 28, 5);
  fillCircle(88, 28, 5);
  fillCircle(43, 26, 2);
  fillCircle(91, 26, 2);
  u8g2.setDrawColor(1);
  
  // Angry eyebrows (inverted V)
  u8g2.drawLine(26, 14, 48, 22);
  u8g2.drawLine(80, 22, 102, 14);
  u8g2.drawLine(26, 15, 48, 23);
  u8g2.drawLine(80, 23, 102, 15);
  
  // Tiny angry mouth
  u8g2.drawBox(58, 52, 12, 2);
  
  u8g2.sendBuffer();
}

// 😲 SURPRISED FACE KAWAII
void dibujarSurprised() {
  u8g2.clearBuffer();
  
  // VERY big open eyes
  fillCircle(40, 28, 17);
  fillCircle(88, 28, 17);
  
  u8g2.setDrawColor(0);
  fillCircle(40, 28, 8);
  fillCircle(88, 28, 8);
  fillCircle(44, 24, 3);
  fillCircle(92, 24, 3);
  u8g2.setDrawColor(1);
  
  // Tiny surprised mouth (small circle)
  u8g2.drawCircle(64, 54, 4);
  fillCircle(64, 54, 3);
  
  u8g2.sendBuffer();
}

// 😴 SLEEPY FACE KAWAII (with animation)
void dibujarSleepy() {
  u8g2.clearBuffer();
  
  // Longer closed eyes (curved lines)
  u8g2.drawCircle(40, 30, 12, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_UPPER_LEFT);
  u8g2.drawCircle(88, 30, 12, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_UPPER_LEFT);
  u8g2.drawLine(28, 30, 52, 30);
  u8g2.drawLine(76, 30, 100, 30);
  
  // Tiny mouth
  u8g2.drawLine(60, 52, 68, 52);
  
  // "Zzz" animation
  u8g2.setFont(u8g2_font_helvB10_tr);
  int zOffset = (animFrame % 20);
  u8g2.drawStr(100, 20 - zOffset, "Z");
  u8g2.setFont(u8g2_font_7x13_tr);
  u8g2.drawStr(106, 12 - (zOffset/2), "z");
  
  u8g2.sendBuffer();
}

// � CARA GUIÑANDO OJO KAWAII
void dibujarWink() {
  u8g2.clearBuffer();
  
  // Ojo abierto GRANDE
  fillCircle(88, 30, 16);
  u8g2.setDrawColor(0);
  fillCircle(88, 30, 7);
  fillCircle(92, 26, 3);
  u8g2.setDrawColor(1);
  
  // Ojo cerrado (guiño)
  if (animFrame % 10 < 5) {
    // Ojo cerrado feliz
    u8g2.drawCircle(40, 30, 12, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_UPPER_LEFT);
    u8g2.drawLine(28, 30, 52, 30);
  } else {
    // Ojo abierto
    fillCircle(40, 30, 16);
    u8g2.setDrawColor(0);
    fillCircle(40, 30, 7);
    fillCircle(44, 26, 3);
    u8g2.setDrawColor(1);
  }
  
  // Boquita pequeña sonriente
  u8g2.drawCircle(64, 52, 6, U8G2_DRAW_LOWER_RIGHT | U8G2_DRAW_LOWER_LEFT);
  
  u8g2.sendBuffer();
}

// 😍 LOVE FACE KAWAII
void dibujarLove() {
  u8g2.clearBuffer();
  
  // BIG heart-shaped eyes
  u8g2.drawCircle(32, 26, 8);
  u8g2.drawCircle(48, 26, 8);
  u8g2.drawTriangle(26, 30, 54, 30, 40, 42);
  fillCircle(40, 32, 8);
  
  u8g2.drawCircle(80, 26, 8);
  u8g2.drawCircle(96, 26, 8);
  u8g2.drawTriangle(74, 30, 102, 30, 88, 42);
  fillCircle(88, 32, 8);
  
  // Tiny happy mouth
  u8g2.drawCircle(64, 54, 7, U8G2_DRAW_LOWER_RIGHT | U8G2_DRAW_LOWER_LEFT);
  fillCircle(64, 56, 3);
  
  u8g2.sendBuffer();
}

// Main function to display emotion
void mostrarEmocion(String emocion) {
  if (emocion == "happy") {
    dibujarHappy();
  } else if (emocion == "sad") {
    dibujarSad();
  } else if (emocion == "angry") {
    dibujarAngry();
  } else if (emocion == "surprised") {
    dibujarSurprised();
  } else if (emocion == "sleepy") {
    dibujarSleepy();
  } else if (emocion == "wink") {
    dibujarWink();
  } else if (emocion == "love") {
    dibujarLove();
  } else {
    dibujarHappy(); // default
  }
}

void mostrarAnimacionInicio() {
  // Welcome animation - happy face appearing
  for (int i = 0; i < 3; i++) {
    dibujarHappy();
    delay(300);
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    delay(200);
  }
  dibujarHappy();
  delay(500);
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  
  // Initialize I2C (GPIO 6=SDA, GPIO 7=SCL)
  Wire.begin(6, 7);
  
  // Initialize OLED display
  u8g2.begin();
  u8g2.setContrast(255); // Max brightness
  
  // Show startup animation
  mostrarAnimacionInicio();
  
  // Initialize PCA9685
  pca.begin();
  pca.reset();
  pca.setPWMFreq(50);
  delay(20);
  
  // Initial position
  neutro();
  delay(500);
  
  Serial.println("=== PULSAR ROBOT WITH EMOTIONAL FACES ===");
  Serial.println("Commands:");
  Serial.println("  n - Neutral (happy)");
  Serial.println("  c - Walk alternating (happy)");
  Serial.println("  a - Walk crawling (happy)");
  Serial.println("  s - Sit (sleepy)");
  Serial.println("  h - Wave (love)");
  Serial.println("  r - Reset counter");
  Serial.println("");
  Serial.println("Manual emotions:");
  Serial.println("  1 - Happy");
  Serial.println("  2 - Sad");
  Serial.println("  3 - Angry");
  Serial.println("  4 - Surprised");
  Serial.println("  5 - Sleepy");
  Serial.println("  6 - Wink");
  Serial.println("  7 - Love");
}

// ========== LOOP ==========
void loop() {
  
  // Actualizar pantalla con emoción actual
  mostrarEmocion(emocionActual);
  animFrame++;
  
  // Leer comandos por Serial
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    
    switch(cmd) {
      case 'n':
        neutro();
        Serial.println("-> Neutral");
        break;
        
      case 'c':
        Serial.println("-> Walking alternating 3 steps");
        for (int i = 0; i < 3; i++) {
          caminaPaso();
          mostrarEmocion(emocionActual);
          delay(100);
        }
        neutro();
        break;
        
      case 'a':
        Serial.println("-> Crawling 3 cycles");
        for (int i = 0; i < 3; i++) {
          caminaArrastre();
          mostrarEmocion(emocionActual);
          delay(100);
        }
        neutro();
        break;
        
      case 's':
        sentado();
        Serial.println("-> Sitting");
        break;
        
      case 'h':
        saluda();
        Serial.println("-> Waving");
        break;
        
      case 'r':
        pasosCaminados = 0;
        Serial.println("-> Counter reset");
        break;
        
      // Manual emotions
      case '1':
        emocionActual = "happy";
        Serial.println("-> Emotion: Happy");
        break;
      case '2':
        emocionActual = "sad";
        Serial.println("-> Emotion: Sad");
        break;
      case '3':
        emocionActual = "angry";
        Serial.println("-> Emotion: Angry");
        break;
      case '4':
        emocionActual = "surprised";
        Serial.println("-> Emotion: Surprised");
        break;
      case '5':
        emocionActual = "sleepy";
        Serial.println("-> Emotion: Sleepy");
        break;
      case '6':
        emocionActual = "wink";
        Serial.println("-> Emotion: Wink");
        break;
      case '7':
        emocionActual = "love";
        Serial.println("-> Emotion: Love");
        break;
    }
  }
  
  delay(100); // update display every 100ms
}

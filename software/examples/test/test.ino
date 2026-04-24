#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

Adafruit_PWMServoDriver pca = Adafruit_PWMServoDriver(0x40);

#define SERVOMIN 110
#define SERVOMAX 520

#define S1 0  // front left
#define S2 1  // rear left
#define S3 2  // front right
#define S4 3  // rear right

bool invertido[4] = {true, true, false, false};
int offset[4] = {0, 0, 0, 0};

// Base positions
int centro = 90;
int adelante = 160;
int atras = 80;

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
  for (int i = 0; i < 4; i++) {
    moverServo(i, centro);
  }
}

// Walk forward
void caminaPaso() {

  moverServo(S1, adelante);
  moverServo(S4, adelante);
  moverServo(S2, atras);
  moverServo(S3, atras);
  delay(120);

  moverServo(S2, adelante);
  moverServo(S3, adelante);
  moverServo(S1, atras);
  moverServo(S4, atras);
  delay(120);
}

// Sit position
void sentado() {
  moverServo(S1, atras);
  moverServo(S3, atras);

  moverServo(S2, adelante);
  moverServo(S4, adelante);
}

// Wave greeting
void saluda() {

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

// Pet greeting: sit + wave
void pet_saluda() {
  Serial.println("Pet mode (sit + wave)");

  // 1. Sit down
  sentado();
  delay(600);

  // 2. Raise paw
  moverServo(S1, adelante);
  delay(300);

  // 3. Wave
  for (int i = 0; i < 5; i++) {
    moverServo(S1, adelante);
    delay(200);
    moverServo(S1, centro);
    delay(200);
  }

  // 4. Return to neutral
  neutro();
}

// Control mode
char modo = 'N';

void setup() {
  Serial.begin(115200);
  Wire.begin(6, 7);

  pca.begin();
  pca.reset();
  pca.setPWMFreq(50);

  delay(20);

  Serial.println("=== ROBOT PET ===");
  Serial.println("W - Walk");
  Serial.println("S - Sit");
  Serial.println("H - Wave");
  Serial.println("P - Pet greeting (sit + wave)");
  Serial.println("N - Neutral");

  neutro();
}

void loop() {

  if (Serial.available()) {
    modo = Serial.read();
  }

  switch (modo) {

    case 'W':
      caminaPaso();
      break;

    case 'S':
      sentado();
      delay(400);
      break;

    case 'H':
      saluda();
      modo = 'N';
      break;

    case 'P':
      pet_saluda();
      modo = 'N';
      break;

    case 'N':
      neutro();
      delay(200);
      break;
  }
}
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

// Positions - smooth movement
int adelante = 70;   // was 40, now closer to center
int atras = 110;     // was 140, now closer to center
int centro = 90;

uint16_t anguloToPWM(int angulo) {
  return map(angulo, 0, 180, SERVOMIN, SERVOMAX);
}

void moverServo(int servo, int angulo) {
  if (invertido[servo]) angulo = 180 - angulo;
  angulo += offset[servo];
  angulo = constrain(angulo, 0, 180);
  pca.setPWM(servo, 0, anguloToPWM(angulo));
}

// Front legs free (no braking)
void delanterasLibre() {
  moverServo(S1, centro);
  moverServo(S3, centro);
}

// Rear legs push
void empuje() {
  moverServo(S2, atras);
  moverServo(S4, atras);
}

// Rear legs return
void regreso() {
  moverServo(S2, adelante);
  moverServo(S4, adelante);
}

// Reposition front legs
void recolocarDelanteras() {
  moverServo(S1, adelante);
  moverServo(S3, adelante);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(6, 7);

  pca.begin();
  pca.reset();
  pca.setPWMFreq(50);

  delay(20);

  Serial.println("CRAWL WALK - FRONT: S1-S3 / REAR: S2-S4");

  delanterasLibre();
  regreso();
  delay(2000);
}

void loop() {

  // 1. Rear legs push → forward
  empuje();
  delanterasLibre();
  delay(350);  // faster for less "jumping"

  // 2. Front legs advance (no braking)
  recolocarDelanteras();
  delay(300);  // faster

  // 3. Rear legs return
  regreso();
  delay(350);  // faster
}
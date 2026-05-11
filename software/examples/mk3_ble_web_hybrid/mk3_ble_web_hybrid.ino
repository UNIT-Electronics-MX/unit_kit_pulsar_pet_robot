#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_PWMServoDriver.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// BLE UUIDs basados en el ejemplo mk2_bl_motion
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer* bleServer = NULL;
BLECharacteristic* bleCharacteristic = NULL;
bool bleConnected = false;

// OLED
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// PCA9685
Adafruit_PWMServoDriver pca = Adafruit_PWMServoDriver(0x40);
#define SERVOMIN 110
#define SERVOMAX 520

#define PATA_FL 1
#define PATA_FR 0
#define PATA_BL 3
#define PATA_BR 2

bool invertido[4] = {false, true, false, true};
int offsetServo[4] = {0, 0, 0, 0};

#define ANGULO_NEUTRO 90

float anguloActual[4] = {90, 90, 90, 90};
float anguloObjetivo[4] = {90, 90, 90, 90};
const float VELOCIDAD = 3.0;

String emocionActual = "happy";
String marchaActual = "stop";
int marchaStep = 0;
unsigned long ultimoPasoMarcha = 0;
const unsigned long MARCHA_MS = 220;

uint16_t anguloToPWM(int angulo) {
  return map(angulo, 0, 180, SERVOMIN, SERVOMAX);
}

void moverServo(int canal, int angulo) {
  if (invertido[canal]) angulo = 180 - angulo;
  angulo += offsetServo[canal];
  angulo = constrain(angulo, 0, 180);
  pca.setPWM(canal, 0, anguloToPWM(angulo));
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

void poseNeutra() {
  marchaActual = "stop";
  for (int i = 0; i < 4; i++) {
    anguloObjetivo[i] = ANGULO_NEUTRO;
  }
}

void sentarse() {
  marchaActual = "stop";
  anguloObjetivo[PATA_FL] = 60;
  anguloObjetivo[PATA_FR] = 60;
  anguloObjetivo[PATA_BL] = 155;
  anguloObjetivo[PATA_BR] = 155;
}

void dormir() {
  marchaActual = "stop";
  anguloObjetivo[PATA_FL] = 165;
  anguloObjetivo[PATA_FR] = 165;
  anguloObjetivo[PATA_BL] = 15;
  anguloObjetivo[PATA_BR] = 15;
}

void saludar() {
  marchaActual = "stop";
  sentarse();
  unsigned long t0 = millis();

  while (millis() - t0 < 500) {
    actualizarServosSuave();
  }

  for (int i = 0; i < 3; i++) {
    anguloObjetivo[PATA_FL] = 170;
    t0 = millis();
    while (millis() - t0 < 250) {
      actualizarServosSuave();
    }

    anguloObjetivo[PATA_FL] = 130;
    t0 = millis();
    while (millis() - t0 < 250) {
      actualizarServosSuave();
    }
  }

  poseNeutra();
}

const int secuenciaWalk[4][4] = {
  {120, 60, 60, 60},
  {60, 120, 60, 60},
  {60, 60, 120, 60},
  {70, 70, 60, 120}
};

void actualizarMarcha() {
  if (marchaActual != "walk") return;

  unsigned long ahora = millis();
  if (ahora - ultimoPasoMarcha < MARCHA_MS) return;
  ultimoPasoMarcha = ahora;

  anguloObjetivo[PATA_FL] = secuenciaWalk[marchaStep][0];
  anguloObjetivo[PATA_FR] = secuenciaWalk[marchaStep][1];
  anguloObjetivo[PATA_BL] = secuenciaWalk[marchaStep][2];
  anguloObjetivo[PATA_BR] = secuenciaWalk[marchaStep][3];

  marchaStep = (marchaStep + 1) % 4;
}

void mostrarPantallaEstado() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.drawStr(2, 10, "PULSAR C6 BLE MODE");
  u8g2.drawStr(2, 24, "Control: Web BLE");

  String ble = bleConnected ? "BLE: ON" : "BLE: WAIT";
  u8g2.drawStr(2, 38, ble.c_str());

  String mode = "CMD:" + marchaActual + " " + emocionActual;
  u8g2.drawStr(2, 52, mode.c_str());

  u8g2.drawStr(2, 63, "Srv:4fafc201...914b");

  u8g2.sendBuffer();
}

String aplicarComando(const String& cmdIn) {
  String cmd = cmdIn;
  cmd.toLowerCase();

  if (cmd == "happy") {
    emocionActual = "happy";
    return "happy";
  }
  if (cmd == "sleepy") {
    emocionActual = "sleepy";
    return "sleepy";
  }
  if (cmd == "surprised") {
    emocionActual = "surprised";
    return "surprised";
  }
  if (cmd == "wink") {
    emocionActual = "wink";
    return "wink";
  }
  if (cmd == "angry") {
    emocionActual = "angry";
    return "angry";
  }

  if (cmd == "standup" || cmd == "stand") {
    poseNeutra();
    return "standup";
  }
  if (cmd == "sit" || cmd == "sentarse") {
    sentarse();
    return "sit";
  }
  if (cmd == "sleep" || cmd == "dormir") {
    dormir();
    return "sleep";
  }
  if (cmd == "wave" || cmd == "saludar") {
    saludar();
    return "wave";
  }
  if (cmd == "walk") {
    marchaActual = "walk";
    marchaStep = 0;
    return "walk";
  }
  if (cmd == "stop") {
    poseNeutra();
    return "stop";
  }

  return "unknown:" + cmd;
}

void responderBLE(const String& text) {
  if (!bleConnected || bleCharacteristic == NULL) return;
  bleCharacteristic->setValue(text.c_str());
  bleCharacteristic->notify();
}

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    (void)pServer;
    bleConnected = true;
    Serial.println("BLE client connected");
  }

  void onDisconnect(BLEServer* pServer) {
    (void)pServer;
    bleConnected = false;
    Serial.println("BLE client disconnected");
    BLEDevice::startAdvertising();
    Serial.println("BLE advertising restarted");
  }
};

class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) {
    String value = String((const char*)pChar->getValue().data());
    if (value.length() == 0) return;

    String cmd = value;
    String result = aplicarComando(cmd);

    Serial.print("BLE CMD: ");
    Serial.println(cmd);
    responderBLE("ok:" + result);
  }
};

void setupBLE() {
  BLEDevice::init("PulsarC6_BLE");
  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new MyServerCallbacks());

  BLEService* service = bleServer->createService(SERVICE_UUID);

  bleCharacteristic = service->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  bleCharacteristic->addDescriptor(new BLE2902());
  bleCharacteristic->setCallbacks(new CommandCallbacks());
  bleCharacteristic->setValue("ready");

  service->start();

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  adv->setMinPreferred(0x06);
  adv->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("BLE ready: PulsarC6_BLE");
  Serial.println("Service UUID: " SERVICE_UUID);
  Serial.println("Char UUID: " CHARACTERISTIC_UUID);
}

void setupRobot() {
  Wire.begin(6, 7);
  u8g2.begin();
  u8g2.setContrast(255);

  pca.begin();
  pca.setPWMFreq(50);

  for (int i = 0; i < 4; i++) {
    anguloActual[i] = ANGULO_NEUTRO;
    anguloObjetivo[i] = ANGULO_NEUTRO;
    moverServo(i, ANGULO_NEUTRO);
  }
}

void setup() {
  Serial.begin(115200);
  delay(600);

  setupRobot();
  setupBLE();

  Serial.println("----------------------------");
  Serial.println("Pulsar C6 BLE Only");
  Serial.println("Conecta desde una pagina docs usando Web Bluetooth");
  Serial.println("Comandos: happy sleepy surprised wink angry standup sit sleep wave walk stop");
  Serial.println("----------------------------");
}

void loop() {
  static String serialBuffer = "";
  while (Serial.available()) {
    char c = (char)Serial.read();

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      serialBuffer.trim();
      if (serialBuffer.length() > 0) {
        String result = aplicarComando(serialBuffer);
        Serial.println("SERIAL CMD => " + result);
        responderBLE("serial:" + result);
      }
      serialBuffer = "";
      continue;
    }

    serialBuffer += c;
  }

  actualizarMarcha();
  actualizarServosSuave();

  static unsigned long lastDraw = 0;
  if (millis() - lastDraw > 150) {
    mostrarPantallaEstado();
    lastDraw = millis();
  }
}

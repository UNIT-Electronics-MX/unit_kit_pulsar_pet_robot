/*
 * Test BLE Básico para ESP32-C3/S3
 * Prueba mínima para verificar que BLE funciona
 */

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("✅ Cliente BLE conectado!");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("❌ Cliente BLE desconectado");
      // Reiniciar advertising
      BLEDevice::startAdvertising();
      Serial.println("🔄 Advertising reiniciado");
    }
};

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n========================================");
  Serial.println("TEST BLE BÁSICO - ESP32");
  Serial.println("========================================");
  
  // Imprimir información del chip
  Serial.print("Chip: ");
  Serial.println(ESP.getChipModel());
  Serial.print("Revisión: ");
  Serial.println(ESP.getChipRevision());
  Serial.print("Núcleos: ");
  Serial.println(ESP.getChipCores());
  
  Serial.println("\nInicializando BLE...");
  
  // Crear dispositivo BLE
  BLEDevice::init("ESP32_TEST");
  Serial.println("✅ BLEDevice::init() completado");
  
  // Crear servidor BLE
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  Serial.println("✅ BLE Server creado");

  // Crear servicio BLE
  BLEService *pService = pServer->createService(SERVICE_UUID);
  Serial.println("✅ Servicio BLE creado");

  // Crear característica
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setValue("Hola BLE");
  Serial.println("✅ Característica creada");

  // Iniciar servicio
  pService->start();
  Serial.println("✅ Servicio iniciado");

  // Configurar advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  Serial.println("✅ Advertising configurado");
  
  // Iniciar advertising
  BLEDevice::startAdvertising();
  Serial.println("✅ Advertising INICIADO");
  
  Serial.println("\n========================================");
  Serial.println("BLE LISTO - Buscando como: ESP32_TEST");
  Serial.println("========================================");
  Serial.println("UUID Servicio: " + String(SERVICE_UUID));
  Serial.println("========================================\n");
}

void loop() {
  if (deviceConnected) {
    // Enviar contador cada segundo
    static unsigned long lastTime = 0;
    static int counter = 0;
    
    if (millis() - lastTime > 1000) {
      String msg = "Counter: " + String(counter++);
      pCharacteristic->setValue(msg.c_str());
      pCharacteristic->notify();
      Serial.println("📤 Enviado: " + msg);
      lastTime = millis();
    }
  }
  
  delay(100);
}

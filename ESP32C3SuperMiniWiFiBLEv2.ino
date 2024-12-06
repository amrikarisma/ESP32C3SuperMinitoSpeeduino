#include <WiFi.h>
#include <WiFiClient.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define UART_BAUD 115200
#define packTimeout 1
#define bufferSize 8192

const char *ssid = "EMUBlue² By Bolino08C";
const char *pw = "123456789";
IPAddress ip(192, 168, 1, 80);
IPAddress netmask(255, 255, 255, 0);
const int port = 2000;

WiFiServer server(port);
WiFiClient client;
BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic = NULL;
BLECharacteristic *pRxCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t buf1[bufferSize];
uint16_t i1 = 0;
uint8_t buf2[bufferSize];
uint16_t i2 = 0;
bool wifiConnected = false; 

#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_TX "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_RX "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};

class MyCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        String rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0) {
            Serial1.write((uint8_t*)rxValue.c_str(), rxValue.length());
        }
    }
};

void setup() {
    Serial.begin(UART_BAUD);
    Serial1.begin(UART_BAUD, SERIAL_8N1, 1, 0);  // UART1 ESP32-C3 SuperMini (RX sur GPIO1=>PA9, TX sur GPIO0=>PA10) dialogue STM32 Ok

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(ip, ip, netmask);
    WiFi.softAP(ssid, pw);
    server.begin();

    BLEDevice::init("EMUBlue²_Bolino08c_BLE");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);
    pTxCharacteristic = pService->createCharacteristic(
                            CHARACTERISTIC_UUID_TX,
                            BLECharacteristic::PROPERTY_NOTIFY
                        );
    pTxCharacteristic->addDescriptor(new BLE2902());
    pRxCharacteristic = pService->createCharacteristic(
                            CHARACTERISTIC_UUID_RX,
                            BLECharacteristic::PROPERTY_WRITE
                        );
    pRxCharacteristic->setCallbacks(new MyCallbacks());
    pService->start();
    pServer->getAdvertising()->start();

    pinMode(8, OUTPUT);  // Configurer la LED intégrée
    digitalWrite(8, HIGH);  // Éteindre la LED par défaut
}

void loop() {
    if (!client.connected()) {
        client = server.available();
        if (client) {
            wifiConnected = true; 
            digitalWrite(8, LOW);  // Allumer la LED pour indiquer la connexion WiFi
        } else {
            wifiConnected = false; 
            digitalWrite(8, HIGH);  // Éteindre la LED pour indiquer la déconnexion WiFi
        }
        return;
    }

    // Gérer les données reçues par WiFi
    if (client.available()) {
        while (client.available()) {
            buf1[i1] = (uint8_t)client.read();
            if (i1 < bufferSize - 1) i1++;
        }
        Serial1.write(buf1, i1);
        if (deviceConnected) {
            pTxCharacteristic->setValue(buf1, i1);
            pTxCharacteristic->notify();
        }
        i1 = 0;
    }

    // Gérer les données reçues par UART1
    if (Serial1.available()) {
        while (1) {
            if (Serial1.available()) {
                buf2[i2] = (char)Serial1.read();
                if (i2 < bufferSize - 1) i2++;
            } else {
                delay(packTimeout);
                if (!Serial1.available()) {
                    break;
                }
            }
        }
        client.write((char*)buf2, i2);
        if (deviceConnected) {
            pTxCharacteristic->setValue(buf2, i2);
            pTxCharacteristic->notify();
        }
        i2 = 0;
    }

    // Gérer la connexion BLE
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); 
        pServer->startAdvertising(); 
        oldDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }
}

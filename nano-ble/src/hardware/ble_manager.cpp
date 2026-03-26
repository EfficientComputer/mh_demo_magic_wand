#include "ble_manager.h"
#include <Arduino.h>

// Static UUID definitions
const char *BLEManager::SERVICE_UUID = "12345678-1234-5678-1234-56789abcdef0";
const char *BLEManager::WRITE_CHAR_UUID = "12345678-1234-5678-1234-56789abcdef1";
const char *BLEManager::ACK_CHAR_UUID = "12345678-1234-5678-1234-56789abcdef2";
const char *BLEManager::DATA_OUT_CHAR_UUID = "12345678-1234-5678-1234-56789abcdef3";

BLEManager::BLEManager()
    : _service(SERVICE_UUID),
      _writeChar(WRITE_CHAR_UUID, BLEWrite | BLEWriteWithoutResponse, 20),
      _ackChar(ACK_CHAR_UUID, BLENotify, 1),
      _dataOutChar(DATA_OUT_CHAR_UUID, BLENotify, 161),
      _writeHandler(nullptr),
      _connectHandler(nullptr),  // REVIEW: Never set, always nullptr
      _disconnectHandler(nullptr),  // REVIEW: Never set, always nullptr
      _connected(false),
      _lastHeartbeatMs(0),
      _heartbeatIntervalMs(5000) {}

bool BLEManager::begin(const char *localName, const char *deviceName) {
    if (!BLE.begin()) {
        Serial.println("BLE start failed");
        return false;
    }
    BLE.setLocalName(localName);
    BLE.setDeviceName(deviceName);

    _service.addCharacteristic(_writeChar);
    _service.addCharacteristic(_ackChar);
    _service.addCharacteristic(_dataOutChar);
    BLE.addService(_service);

    BLE.setAdvertisedService(_service);
    BLE.setAdvertisedServiceUuid(SERVICE_UUID);

    uint8_t zero = 0;
    _ackChar.writeValue(&zero, 1);

    BLE.advertise();
    Serial.print("Advertising as ");
    Serial.print(localName);
    Serial.print(" @ ");
    Serial.println(BLE.address());
    return true;
}

void BLEManager::poll() {
    if (!_connected) {
        BLEDevice central = BLE.central();
        if (!central) {
            delay(10);
            return;
        }
        _central = central;
        _connected = true;
        _lastHeartbeatMs = millis();
        Serial.print("Connected: ");
        Serial.println(_central.address());
        if (_connectHandler) {  // REVIEW: Always nullptr, never called
            _connectHandler(_central);
        }
    }

    if (_connected) {
        if (!_central.connected()) {
            handleDisconnect();
            return;
        }

        if (_writeChar.written()) {
            int len = _writeChar.valueLength();
            if (len > 0) {
                uint8_t buf[20];
                if (len > (int)sizeof(buf)) len = sizeof(buf);
                _writeChar.readValue(buf, len);
                if (_writeHandler) {
                    _writeHandler(buf, len);
                }
            }
        }

        unsigned long now = millis();
        if (now - _lastHeartbeatMs >= _heartbeatIntervalMs) {
            sendHeartbeat();
            _lastHeartbeatMs = now;
        }

        delay(1); // yield
    }
}

void BLEManager::sendAck(uint8_t v) {
    _ackChar.writeValue(&v, 1);
}

void BLEManager::sendHeartbeat() {
    uint8_t hb = 0xFF; // heartbeat marker
    sendAck(hb);
}

void BLEManager::sendData(const uint8_t *data, uint8_t len) {
    if (!data) return;
    if (len == 0) return;
    if (len > DATA_OUT_CAPACITY) len = DATA_OUT_CAPACITY;
    if (!_connected) return; // do not attempt when not connected
    _dataOutChar.writeValue(data, len);
    Serial.print("DATA_OUT: ");
    for (uint8_t i = 0; i < len; i++) {
        Serial.print(data[i]);
        if (i + 1 < len) Serial.print(',');
    }
    Serial.println();
}

void BLEManager::handleDisconnect() {
    if (_disconnectHandler && _central) {  // REVIEW: _disconnectHandler always nullptr, never called
        _disconnectHandler(_central);
    }
    _connected = false;
    Serial.println("Disconnected.");
    BLE.advertise();
}

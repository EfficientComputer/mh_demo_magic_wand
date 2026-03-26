#pragma once
#include <ArduinoBLE.h>
#include <stdint.h>

// BLEManager: Encapsulates BLE setup, advertising, connection handling,
// heartbeat notifications, write characteristic dispatch, ACK sending and
// generic data notifications. Designed to keep main.cpp focused on
// application logic (gesture parsing & execution) rather than protocol glue.
class BLEManager {
public:
    typedef void (*WriteHandler)(const uint8_t *data, int len);
    typedef void (*DeviceHandler)(BLEDevice device);

    // DATA_OUT_CAPACITY for final predictions packet: [count][pred0]...[predN-1]. Retain larger size (161) for backward compatibility.
static const uint8_t DATA_OUT_CAPACITY = 161; // variable-length: [count][predictions...]

    BLEManager();

    bool begin(const char *localName, const char *deviceName);
    void poll();

    bool isConnected() const { return _connected; }

    void setWriteHandler(WriteHandler h) { _writeHandler = h; }
    // DEAD CODE: Handler setters never called in codebase
    // void setConnectHandler(DeviceHandler h) { _connectHandler = h; }
    // void setDisconnectHandler(DeviceHandler h) { _disconnectHandler = h; }

    void setHeartbeatInterval(unsigned long ms) { _heartbeatIntervalMs = ms; }

    void sendAck(uint8_t v);
    void sendHeartbeat();

    // sendData: Sends 'len' bytes from 'data' over the dataOut characteristic.
    // Length is clipped to DATA_OUT_CAPACITY. No action if len==0 or data==nullptr.
    void sendData(const uint8_t *data, uint8_t len);

private:
    static const char *SERVICE_UUID;
    static const char *WRITE_CHAR_UUID;    // Central -> Peripheral
    static const char *ACK_CHAR_UUID;      // Peripheral -> Central (notify ACK + heartbeat)
    static const char *DATA_OUT_CHAR_UUID; // Peripheral -> Central (notify data bytes)

    BLEService _service;
    BLECharacteristic _writeChar;
    BLECharacteristic _ackChar;
    BLECharacteristic _dataOutChar;

    WriteHandler _writeHandler;
    // REVIEW: Handlers never set externally, always nullptr
    DeviceHandler _connectHandler;
    DeviceHandler _disconnectHandler;

    BLEDevice _central; // last connected central (valid only when _connected true)

    bool _connected;
    unsigned long _lastHeartbeatMs;
    unsigned long _heartbeatIntervalMs;

    void handleDisconnect();
};

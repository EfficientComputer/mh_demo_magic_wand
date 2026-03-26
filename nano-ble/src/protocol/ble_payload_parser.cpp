#include "ble_payload_parser.h"
#include <Arduino.h>
 
static const uint8_t RESET_COMMAND_BYTE = 0x00;
static const uint8_t GO_COMMAND_BYTE = 0x01;
 
uint8_t parseGestureGameInputPayload(const uint8_t *buf, int len, GestureGameInputPayload &out) {
  out.count = 0;
  out.auxSeconds = 0;
  if (len == 1 && buf[0] == RESET_COMMAND_BYTE) {
    return ACK_RESET_OK;
  }
  if (len == 1 && buf[0] == GO_COMMAND_BYTE) {
    return ACK_GO_OK;
  }
  if (len < 3) {
    return ACK_ERR_PARSE;
  }

  if (len == 1 && buf[0] == GO_COMMAND_BYTE) {
    return ACK_GO_OK;
  }
  if (len < 3) {
    return ACK_ERR_PARSE;
  }

  uint8_t count = buf[0];
  if (count == 0 || count > GGI_MAX_ITEMS) {
    return ACK_ERR_COUNT;
  }
  if (len != (count + 2)) {
    return ACK_ERR_LEN;
  }
  uint8_t auxSeconds = buf[count + 1];
  if (auxSeconds < GGI_MIN_AUX_SEC || auxSeconds > GGI_MAX_AUX_SEC) {
    return ACK_ERR_AUX;
  }
  for (uint8_t i = 0; i < count; ++i) {
    uint8_t v = buf[i + 1];
    if (v >= kGestureCount) {
      return ACK_ERR_VALUE;
    }
    out.values[i] = v;
  }
  out.count = count;
  out.auxSeconds = auxSeconds;
  return count;
}

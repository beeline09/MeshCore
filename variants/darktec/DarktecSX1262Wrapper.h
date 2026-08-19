#pragma once

#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include "DarktecBoard.h"

class DarktecSX1262Wrapper : public CustomSX1262Wrapper {
  DarktecBoard& _dt;
public:
  DarktecSX1262Wrapper(CustomSX1262& radio, DarktecBoard& board)
    : CustomSX1262Wrapper(radio, board), _dt(board) {}

  int recvRaw(uint8_t* bytes, int sz) override {
    int len = RadioLibWrapper::recvRaw(bytes, sz);
    if (len > 0) {
      _dt.onLoRaPacketReceived();
    }
    return len;
  }

  void loop() override {
    RadioLibWrapper::loop();
    _dt.setLoRaReceiving(isReceivingPacket());
  }
};

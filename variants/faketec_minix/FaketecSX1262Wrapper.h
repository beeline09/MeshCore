#pragma once

#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include "PromicroBoard.h"

/**
 * Feeds receive events to the board so the status LED can show LoRa traffic.
 * Transmit is already covered by MainBoard::onBeforeTransmit()/onAfterTransmit().
 */
class FaketecSX1262Wrapper : public CustomSX1262Wrapper {
  PromicroBoard& _pb;

public:
  FaketecSX1262Wrapper(CustomSX1262& radio, PromicroBoard& board)
      : CustomSX1262Wrapper(radio, board), _pb(board) {}

  int recvRaw(uint8_t* bytes, int sz) override {
    int len = RadioLibWrapper::recvRaw(bytes, sz);
#ifdef STATUS_LED_LORA_ACTIVITY
    if (len > 0) _pb.onLoRaPacketReceived();
#endif
    return len;
  }

  void loop() override {
    RadioLibWrapper::loop();
#ifdef STATUS_LED_LORA_ACTIVITY
    _pb.updateStatusLed();   // expires the pulse; this runs on every dispatcher iteration
#endif
  }
};

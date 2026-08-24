#include "Arduino.h"

#if defined(WITH_PING_BOT) && defined(PIN_BUZZER)

#include <helpers/ui/buzzer.h>

/**
 * Свой genericBuzzer для роли ping_bot: мелодия только при старте.
 * Штатный helpers/ui/buzzer.cpp в этой роли не компилируется — UITask::notify()
 * зовёт play() на каждое сообщение, а трогать examples/ нельзя.
 */

void genericBuzzer::begin() {
#ifdef PIN_BUZZER_EN
  pinMode(PIN_BUZZER_EN, OUTPUT);
  digitalWrite(PIN_BUZZER_EN, HIGH);
#endif
  quiet(false);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
}

void genericBuzzer::play(const char* melody) {
  (void)melody;  // сообщения, ack и прочие события UI — без звука
}

bool genericBuzzer::isPlaying() {
  return rtttl::isPlaying();
}

void genericBuzzer::loop() {
  if (!rtttl::done()) rtttl::play();
}

void genericBuzzer::startup() {
  if (_is_quiet) return;
  if (isPlaying()) rtttl::stop();
  rtttl::begin(PIN_BUZZER, startup_song);
}

void genericBuzzer::shutdown() {
  // выключение тоже без звука: пиликаем только при первом включении
}

void genericBuzzer::quiet(bool buzzer_state) {
  _is_quiet = buzzer_state;
#ifdef PIN_BUZZER_EN
  digitalWrite(PIN_BUZZER_EN, _is_quiet ? LOW : HIGH);
#endif
}

bool genericBuzzer::isQuiet() {
  return _is_quiet;
}

#endif

#include <gtest/gtest.h>

#include <DarktecSocModel.h>

using namespace DarktecSocModel;

// Li-ion по умолчанию: таблица full = 4158 мВ, term = capacity/30.
static Config liion(uint16_t cap = 4000) {
  return configFor(DarktecSoc::Chem::LiIon, 1, cap);
}

TEST(DarktecSocModelTest, CoulombCounterChargesAndDischarges) {
  Config c = liion();
  RestPolicy rest = restPolicy();
  State st;
  reset(st, c, 3600, 0);  // ≈50 %

  // +200 мА в течение часа → +5 %
  for (uint32_t t = 0; t < 60; t++) update(st, c, rest, 3600, 200, t * 60000 + 60000);
  EXPECT_NEAR(st.soc_x100, 5500, 10);
  EXPECT_EQ(percent(st), 55);

  // −400 мА час → −10 %
  for (uint32_t t = 0; t < 60; t++) update(st, c, rest, 3500, -400, 3600000 + t * 60000 + 60000);
  EXPECT_NEAR(st.soc_x100, 4500, 10);
}

TEST(DarktecSocModelTest, CoulombClampsAtZeroAndHundred) {
  Config c = liion();
  RestPolicy rest = restPolicy();
  State st;
  reset(st, c, 3006, 0);  // 0 %
  update(st, c, rest, 3006, -4000, 60000);
  EXPECT_EQ(st.soc_x100, 0);

  reset(st, c, 4158, 0);  // 100 %
  update(st, c, rest, 4158, 2000, 60000);
  EXPECT_EQ(st.soc_x100, 10000);
}

TEST(DarktecSocModelTest, RestDriftCorrectionBounded) {
  Config c = liion();
  RestPolicy rest = restPolicy();
  State st;
  reset(st, c, 3760, 0);  // по НАШЕЙ измеренной таблице 3760 мВ ≈ 67 %
  update(st, c, rest, 3760, 0, 0);   // первый тик — якорим время
  st.soc_x100 = 5000;                // расходимся с таблицей на 17 %

  // Покой (|I| < 10 мА). Первые 5 минут — только набираем dwell, дальше —
  // дрейф со скоростью ≤ 500 x100/час (за 6 мин ≤ 0.5 %).
  for (uint32_t t = 250; t <= 6 * 60000; t += 250) {
    update(st, c, rest, 3760, 0, t);
    if (t < 5 * 60000) ASSERT_EQ(st.soc_x100, 5000) << "t=" << t;
  }
  ASSERT_GT(st.soc_x100, 5000);
  EXPECT_LE(st.soc_x100 - 5000, 60);
}

TEST(DarktecSocModelTest, LtoTwoCellsDividesPackVoltage) {
  Config c = configFor(DarktecSoc::Chem::LTO, 2, 4000);
  State st;
  // Пакет 4.72 В = 2.36 В на ячейку → середина таблицы LTO (50 %).
  reset(st, c, 4720, 0);
  EXPECT_NEAR(st.soc_x100, 5000, 600);
}

TEST(ChargeAutomatonTest, EntersChargingOnCurrentAndIgnoresNominal) {
  ChargeSm sm;
  Config c = liion();
  ChargePolicy pol = chargePolicy();
  updateCharge(sm, c, pol, 3700, 2000, 0);
  EXPECT_EQ(sm.state, ChargeState::Charging);
  // Малый номинал после перепайки (например 300 мА) — детекция та же.
  ChargeSm sm2;
  updateCharge(sm2, c, pol, 3700, 300, 0);
  EXPECT_EQ(sm2.state, ChargeState::Charging);
}

TEST(ChargeAutomatonTest, HysteresisExitOnlyBelowExitMa) {
  ChargeSm sm;
  Config c = liion();
  ChargePolicy pol = chargePolicy();
  updateCharge(sm, c, pol, 3700, 2000, 0);
  ASSERT_EQ(sm.state, ChargeState::Charging);
  // 20 мА > exit(15) — ещё заряжается
  updateCharge(sm, c, pol, 3700, 20, 60000);
  EXPECT_EQ(sm.state, ChargeState::Charging);
  // 5 мА < exit и не у CV-порога → idle
  updateCharge(sm, c, pol, 3700, 5, 120000);
  EXPECT_EQ(sm.state, ChargeState::Idle);
}

TEST(ChargeAutomatonTest, FullRequiresDwellNearCvAndTerm) {
  ChargeSm sm;
  Config c = liion();  // term = 4000/30 ≈ 133 мА
  ChargePolicy pol = chargePolicy();
  updateCharge(sm, c, pol, 3700, 2000, 0);
  ASSERT_EQ(sm.state, ChargeState::Charging);

  // У CV-порога, ток < C/30: условие full, но dwell 3 минуты ещё не прошли.
  uint32_t t = 0;
  for (; t <= 120000; t += 250) updateCharge(sm, c, pol, 4150, 100, t);
  EXPECT_EQ(sm.state, ChargeState::Charging);
  for (; t <= 200000; t += 250) updateCharge(sm, c, pol, 4150, 100, t);
  EXPECT_EQ(sm.state, ChargeState::Full);
}

TEST(ChargeAutomatonTest, FullExitsOnNewChargeOrVoltageSag) {
  ChargeSm sm;
  Config c = liion();
  ChargePolicy pol = chargePolicy();
  // Сразу ставим full длительной выдержкой.
  uint32_t t = 0;
  for (; t <= 200000; t += 250) updateCharge(sm, c, pol, 4150, 100, t);
  ASSERT_EQ(sm.state, ChargeState::Full);

  // Напряжение ощутимо просело (сняли зарядку) → idle.
  updateCharge(sm, c, pol, 4100, 0, 200250);
  EXPECT_EQ(sm.state, ChargeState::Idle);

  // Из idle новая зарядка (CC-фаза, ток > C/30) → charging.
  updateCharge(sm, c, pol, 4100, 1500, 200500);
  EXPECT_EQ(sm.state, ChargeState::Charging);

  // Из full CV-подпитка (ток между exit и C/30) НЕ выбрасывает из full.
  ChargeSm sm3;
  uint32_t t3 = 0;
  for (; t3 <= 200000; t3 += 250) updateCharge(sm3, c, pol, 4150, 100, t3);
  ASSERT_EQ(sm3.state, ChargeState::Full);
  updateCharge(sm3, c, pol, 4150, 100, 200250);
  EXPECT_EQ(sm3.state, ChargeState::Full);
}

TEST(ChargeAutomatonTest, FullExitsWhenCvFeedGoneBelowExit) {
  ChargeSm sm;
  Config c = liion();
  ChargePolicy pol = chargePolicy();
  // Ставим full длительной выдержкой (подпитка 100 мА, у CV-порога).
  uint32_t t = 0;
  for (; t <= 200000; t += 250) updateCharge(sm, c, pol, 4150, 100, t);
  ASSERT_EQ(sm.state, ChargeState::Full);

  // Кабель вынули: ток ~0, но V ещё не просел (релаксация полного
  // аккумулятора медленная — на этом «вилка» висела до ребута).
  // Первые gone_ms держим full (гистерезис от шума/переходных процессов).
  updateCharge(sm, c, pol, 4150, 0, 200250);
  EXPECT_EQ(sm.state, ChargeState::Full);
  updateCharge(sm, c, pol, 4140, 0, 205000);   // 4.75 с из 10 с
  EXPECT_EQ(sm.state, ChargeState::Full);

  // Прошли gone_ms — idle, значок гаснет без перезагрузки.
  updateCharge(sm, c, pol, 4140, 0, 210300);   // 10.05 с
  EXPECT_EQ(sm.state, ChargeState::Idle);

  // Подключили снова — новая CC-фаза возвращает charging.
  updateCharge(sm, c, pol, 4140, 1500, 210500);
  EXPECT_EQ(sm.state, ChargeState::Charging);
}

TEST(ChargeAutomatonTest, CvTailStaysChargingEvenBelowExitMa) {
  // В хвосте CV ток 40 мА < exit(15)? нет — 40 > 15. Возьмём 10 мА при
  // V у порога: near_full — остаёмся charging, несмотря на ток ниже exit.
  ChargeSm sm;
  Config c = liion();
  ChargePolicy pol = chargePolicy();
  updateCharge(sm, c, pol, 4150, 2000, 0);
  ASSERT_EQ(sm.state, ChargeState::Charging);
  updateCharge(sm, c, pol, 4150, 10, 60000);
  EXPECT_EQ(sm.state, ChargeState::Charging);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

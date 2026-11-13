// BoatControl — нативные юнит-тесты чистой логики (SPEC п.3, п.6, п.7).
// Требуется [env:native] с компилятором C++ на PC.
// Запуск: pio test -e native

#include <unity.h>
#include "config.h"
#include "throttle.h"
#include "steering.h"
#include "button.h"

void setUp(void) {}
void tearDown(void) {}

// ---------- Throttle::map_pulse ----------

static void test_map_zero_is_neutral(void) {
    TEST_ASSERT_EQUAL_UINT16(RC_PULSE_NEUTRAL, Throttle::map_pulse(0));
    TEST_ASSERT_EQUAL_UINT16(RC_PULSE_NEUTRAL,
                             Throttle::map_pulse(THR_DEADBAND_ADC));
}

static void test_map_forward_full(void) {
    // полный ход «от себя» = упор вперёд = 1000 мкс
    TEST_ASSERT_EQUAL_UINT16(RC_PULSE_NEUTRAL - RC_PULSE_SPAN,
                             Throttle::map_pulse(THR_SPAN_ADC));
    // и за пределами хода — не переполняется
    TEST_ASSERT_EQUAL_UINT16(RC_PULSE_NEUTRAL - RC_PULSE_SPAN,
                             Throttle::map_pulse(THR_SPAN_ADC + 500));
}

static void test_map_reverse_full(void) {
    TEST_ASSERT_EQUAL_UINT16(RC_PULSE_NEUTRAL + RC_PULSE_SPAN,
                             Throttle::map_pulse(-THR_SPAN_ADC));
    TEST_ASSERT_EQUAL_UINT16(RC_PULSE_NEUTRAL + RC_PULSE_SPAN,
                             Throttle::map_pulse(-THR_SPAN_ADC - 500));
}

static void test_map_half(void) {
    int32_t half = (THR_SPAN_ADC + THR_DEADBAND_ADC) / 2;
    uint16_t p = Throttle::map_pulse(half);
    TEST_ASSERT_TRUE(p < RC_PULSE_NEUTRAL);           // вперёд
    TEST_ASSERT_INT_WITHIN(10, RC_PULSE_NEUTRAL - RC_PULSE_SPAN / 2, p);
}

// ---------- Throttle::percent / sign_of ----------

static void test_percent_bounds(void) {
    TEST_ASSERT_EQUAL_UINT8(0, Throttle::percent(RC_PULSE_NEUTRAL));
    TEST_ASSERT_EQUAL_UINT8(100, Throttle::percent(RC_PULSE_NEUTRAL - RC_PULSE_SPAN));
    TEST_ASSERT_EQUAL_UINT8(100, Throttle::percent(RC_PULSE_NEUTRAL + RC_PULSE_SPAN));
}

static void test_sign_of(void) {
    TEST_ASSERT_EQUAL_INT(-1, Throttle::sign_of(1200));
    TEST_ASSERT_EQUAL_INT(0, Throttle::sign_of(RC_PULSE_NEUTRAL));
    TEST_ASSERT_EQUAL_INT(1, Throttle::sign_of(1800));
}

// ---------- Throttle::slew_to: rate limit ----------

static void test_slew_respects_rate_up(void) {
    Throttle t;
    // цель — полный вперёд (1000). За один такт 20 мс проходимо не более
    // RC_PULSE_SPAN * THR_RATE_UP_PCT * 20 / 10000 мкс.
    uint16_t max_step = (uint16_t)((uint32_t)RC_PULSE_SPAN * THR_RATE_UP_PCT * 20 / 10000UL);
    t.slew_to(RC_PULSE_NEUTRAL - RC_PULSE_SPAN, 20);
    int32_t moved = RC_PULSE_NEUTRAL - t.out();
    TEST_ASSERT_TRUE(moved > 0);
    TEST_ASSERT_TRUE(moved <= max_step);
}

static void test_slew_reaches_target_finite_time(void) {
    Throttle t;
    for (int i = 0; i < 200; i++) t.slew_to(RC_PULSE_NEUTRAL - RC_PULSE_SPAN, 20);
    TEST_ASSERT_EQUAL_UINT16(RC_PULSE_NEUTRAL - RC_PULSE_SPAN, t.out());
}

// ---------- Throttle::slew_to: смена знака через нейтраль + пауза ----------

static void test_slew_sign_change_pauses_at_neutral(void) {
    Throttle t;
    for (int i = 0; i < 200; i++) t.slew_to(RC_PULSE_NEUTRAL - RC_PULSE_SPAN, 20);
    TEST_ASSERT_EQUAL_UINT16(RC_PULSE_NEUTRAL - RC_PULSE_SPAN, t.out());

    // цель — назад: сначала обязательный возврат в нейтраль
    t.slew_to(RC_PULSE_NEUTRAL + RC_PULSE_SPAN, 20);
    TEST_ASSERT_EQUAL_UINT16(RC_PULSE_NEUTRAL, t.out());

    // вся пауза THR_ZERO_PASS_MS — стоим крестом в нейтрали
    uint32_t waited = 0;
    while (waited < THR_ZERO_PASS_MS) {
        t.slew_to(RC_PULSE_NEUTRAL + RC_PULSE_SPAN, 20);
        TEST_ASSERT_EQUAL_UINT16(RC_PULSE_NEUTRAL, t.out());
        waited += 20;
    }
    // после паузы газ начинает расти в обратную сторону
    t.slew_to(RC_PULSE_NEUTRAL + RC_PULSE_SPAN, 20);
    TEST_ASSERT_TRUE(t.out() > RC_PULSE_NEUTRAL);
}

// ---------- Throttle::slew_to: cap (возврат из круиза) ----------

static void test_slew_cap_limits_step(void) {
    Throttle t;
    for (int i = 0; i < 200; i++) t.slew_to(RC_PULSE_NEUTRAL - RC_PULSE_SPAN, 20);
    uint16_t before = t.out();
    const uint16_t cap = 5;
    // цель остаётся вперёд (1200) — cap ограничивает шаг сброса
    t.slew_to(RC_PULSE_NEUTRAL - 300, 20, cap);
    TEST_ASSERT_EQUAL_UINT16(before + cap, t.out());
}

static void test_slew_to_neutral_is_immediate_cut(void) {
    Throttle t;
    for (int i = 0; i < 200; i++) t.slew_to(RC_PULSE_NEUTRAL - RC_PULSE_SPAN, 20);
    // цель ровно нейтраль: мгновенный сброс газа (безопасная сторона,
    // rate limit и cap на «стоп» не распространяются)
    t.slew_to(RC_PULSE_NEUTRAL, 20, 5);
    TEST_ASSERT_EQUAL_UINT16(RC_PULSE_NEUTRAL, t.out());
}

// ---------- Throttle::force ----------

static void test_force_breaks_hold(void) {
    Throttle t;
    for (int i = 0; i < 200; i++) t.slew_to(RC_PULSE_NEUTRAL - RC_PULSE_SPAN, 20);
    // смена знака запускает паузу в нейтрали
    t.slew_to(RC_PULSE_NEUTRAL + RC_PULSE_SPAN, 20);
    t.slew_to(RC_PULSE_NEUTRAL + RC_PULSE_SPAN, 20);
    TEST_ASSERT_EQUAL_UINT16(RC_PULSE_NEUTRAL, t.out());
    // force() обнуляет остаток паузы
    t.force(RC_PULSE_NEUTRAL);
    uint16_t before = t.out();
    t.slew_to(RC_PULSE_NEUTRAL - RC_PULSE_SPAN, 20);
    TEST_ASSERT_TRUE(t.out() < before);  // газ пошёл сразу, без новой паузы
}

// ---------- Steering::map_pulse: карта без сглаживания ----------

static void test_stb_map_neutral_and_deadband(void) {
    TEST_ASSERT_EQUAL_UINT16(RC_PULSE_NEUTRAL, Steering::map_pulse(0));
    TEST_ASSERT_EQUAL_UINT16(RC_PULSE_NEUTRAL, Steering::map_pulse(X_DEADBAND_ADC));
    TEST_ASSERT_EQUAL_UINT16(RC_PULSE_NEUTRAL, Steering::map_pulse(-X_DEADBAND_ADC));
}

static void test_stb_map_full_ends(void) {
    // полный ход вправо = упор 2000, влево = 1000; за пределами хода — не больше
    TEST_ASSERT_EQUAL_UINT16(STB_PULSE_MAX, Steering::map_pulse(X_SPAN_ADC));
    TEST_ASSERT_EQUAL_UINT16(STB_PULSE_MAX, Steering::map_pulse(X_SPAN_ADC + 500));
    TEST_ASSERT_EQUAL_UINT16(STB_PULSE_MIN, Steering::map_pulse(-X_SPAN_ADC));
    TEST_ASSERT_EQUAL_UINT16(STB_PULSE_MIN, Steering::map_pulse(-X_SPAN_ADC - 500));
}

// ---------- Steering::slew_to: ограничение скорости хода сервы ----------

static void test_stb_slew_step_limited(void) {
    Steering s;
    // цель — полный вправо (2000). За такт 20 мс проходимо не более
    // (STB_PULSE_MAX-STB_PULSE_MIN)*20/STB_SLEW_FULL_MS мкс.
    const uint16_t max_step =
        (uint16_t)((uint32_t)(STB_PULSE_MAX - STB_PULSE_MIN) * 20UL / STB_SLEW_FULL_MS);
    s.slew_to(STB_PULSE_MAX, 20);
    uint16_t moved = (uint16_t)(s.out() - RC_PULSE_NEUTRAL);
    TEST_ASSERT_TRUE(moved > 0);
    TEST_ASSERT_TRUE(moved <= max_step);
}

static void test_stb_slew_full_span_takes_two_seconds(void) {
    Steering s;
    // от упора до упора: 1000 -> 2000 за STB_SLEW_FULL_MS (ровно 100 тактов по 20 мс)
    s.force(STB_PULSE_MIN);
    for (int i = 0; i < 100; i++) s.slew_to(STB_PULSE_MAX, 20);
    TEST_ASSERT_EQUAL_UINT16(STB_PULSE_MAX, s.out());

    // и в обратную сторону (возврат через нейтраль) — тот же темп
    s.force(STB_PULSE_MAX);
    for (int i = 0; i < 100; i++) s.slew_to(STB_PULSE_MIN, 20);
    TEST_ASSERT_EQUAL_UINT16(STB_PULSE_MIN, s.out());
}

static void test_stb_slew_not_early(void) {
    Steering s;
    // раньше времени цель достигнута быть не может: 50 тактов = половина хода
    s.force(RC_PULSE_NEUTRAL);
    for (int i = 0; i < 50; i++) s.slew_to(STB_PULSE_MAX, 20);
    TEST_ASSERT_TRUE(s.out() < STB_PULSE_MAX);
    uint16_t halfway = (uint16_t)(RC_PULSE_NEUTRAL + (STB_PULSE_MAX - RC_PULSE_NEUTRAL) / 2);
    TEST_ASSERT_INT_WITHIN(2, halfway, s.out());
}

static void test_stb_slew_small_dt_still_moves(void) {
    Steering s;
    // dt меньше одного шага по формуле: минимум 1 мкс за такт, иначе серва встанет
    s.force(RC_PULSE_NEUTRAL);
    uint16_t prev = s.out();
    for (int i = 0; i < 5; i++) {
        s.slew_to(STB_PULSE_MAX, 1);
        TEST_ASSERT_TRUE(s.out() > prev);
        prev = s.out();
    }
}

static void test_stb_slew_deadband_target_held(void) {
    Steering s;
    // цель = текущее значение: шаг нулевой, выход не дрожит
    s.force(RC_PULSE_NEUTRAL);
    s.slew_to(RC_PULSE_NEUTRAL, 20);
    TEST_ASSERT_EQUAL_UINT16(RC_PULSE_NEUTRAL, s.out());
}

// ---------- Button ----------
// Время во всех тестах монотонно: один временной отсчёт на всю сцену.

static void test_button_short_press(void) {
    Button b;
    bool short_seen = false;
    uint32_t t = 0;
    for (; t < BTN_DEBOUNCE_MS + 60; t += LOOP_PERIOD_MS) b.update(true, t);
    for (; t < BTN_DEBOUNCE_MS + 300; t += LOOP_PERIOD_MS) {
        b.update(false, t);
        if (b.short_press()) short_seen = true;
    }
    TEST_ASSERT_TRUE(short_seen);
}

static void test_button_debounce_filters_bounce(void) {
    Button b;
    // импульс короче BTN_DEBOUNCE_MS не должен породить событие
    bool short_seen = false;
    uint32_t t = 0;
    for (; t < BTN_DEBOUNCE_MS - 5; t += LOOP_PERIOD_MS) {
        b.update(true, t);
        if (b.short_press()) short_seen = true;
    }
    for (; t < 1000; t += LOOP_PERIOD_MS) {
        b.update(false, t);
        if (b.short_press()) short_seen = true;
    }
    TEST_ASSERT_FALSE(short_seen);
}

static void test_button_long_press(void) {
    Button b;
    bool long_seen = false;
    for (uint32_t t = 0; t < BTN_LONG_MS + 100; t += LOOP_PERIOD_MS) {
        b.update(true, t);
        if (b.long_press()) long_seen = true;
    }
    TEST_ASSERT_TRUE(long_seen);
    // длинное — однократный фронт, повторного нет
    b.update(true, BTN_LONG_MS + 120);
    TEST_ASSERT_FALSE(b.long_press());
}

static void test_button_long_no_short_on_release(void) {
    Button b;
    for (uint32_t t = 0; t < BTN_LONG_MS + 100; t += LOOP_PERIOD_MS) {
        b.update(true, t);
    }
    bool short_seen = false;
    for (uint32_t t = BTN_LONG_MS + 100; t < BTN_LONG_MS + 400; t += LOOP_PERIOD_MS) {
        b.update(false, t);
        if (b.short_press()) short_seen = true;
    }
    TEST_ASSERT_FALSE(short_seen);
}

static void test_button_stuck(void) {
    Button b;
    bool stuck_seen = false;
    for (uint32_t t = 0; t < BTN_STUCK_MS + 100; t += LOOP_PERIOD_MS) {
        b.update(true, t);
        if (b.stuck()) stuck_seen = true;
    }
    TEST_ASSERT_TRUE(stuck_seen);
}

int main(void) {
    UNITY_BEGIN();
    // map
    RUN_TEST(test_map_zero_is_neutral);
    RUN_TEST(test_map_forward_full);
    RUN_TEST(test_map_reverse_full);
    RUN_TEST(test_map_half);
    RUN_TEST(test_percent_bounds);
    RUN_TEST(test_sign_of);
    // slew
    RUN_TEST(test_slew_respects_rate_up);
    RUN_TEST(test_slew_reaches_target_finite_time);
    RUN_TEST(test_slew_sign_change_pauses_at_neutral);
    RUN_TEST(test_slew_cap_limits_step);
    RUN_TEST(test_slew_to_neutral_is_immediate_cut);
    RUN_TEST(test_force_breaks_hold);
    // steering: карта + ограничение скорости хода сервы
    RUN_TEST(test_stb_map_neutral_and_deadband);
    RUN_TEST(test_stb_map_full_ends);
    RUN_TEST(test_stb_slew_step_limited);
    RUN_TEST(test_stb_slew_full_span_takes_two_seconds);
    RUN_TEST(test_stb_slew_not_early);
    RUN_TEST(test_stb_slew_small_dt_still_moves);
    RUN_TEST(test_stb_slew_deadband_target_held);
    // button
    RUN_TEST(test_button_short_press);
    RUN_TEST(test_button_debounce_filters_bounce);
    RUN_TEST(test_button_long_press);
    RUN_TEST(test_button_long_no_short_on_release);
    RUN_TEST(test_button_stuck);
    return UNITY_END();
}

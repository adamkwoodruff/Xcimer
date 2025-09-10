#include "IGBT.h"
#include "PowerState.h"

void init_igbt_pwm() {
    pinMode(PWM_IGBT_HI, OUTPUT);
    analogWrite(PWM_IGBT_HI, 0);
}

void update_igbt_pwm() {
    // Simple fault logic: if fault asserted, force duty to zero
    if (PowerState::igbtFault) {
        analogWrite(PWM_IGBT_HI, 0);
        return;
    }

    float duty = PowerState::setCurrent * PID_TO_DUTY_SCALE;
    if (duty < 0.0f) duty = 0.0f;
    if (duty > IGBT_PWM_MAX_DUTY) duty = IGBT_PWM_MAX_DUTY;

    uint32_t maxCount = (1u << IGBT_PWM_RESOLUTION_BITS) - 1u;
    analogWrite(PWM_IGBT_HI, (uint32_t)(duty * maxCount));
}


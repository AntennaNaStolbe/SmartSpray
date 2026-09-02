#include "motor.h"
#include "config.h"
#include "settings.h"

GyverMotor2<GM2::DIR_PWM> motor(DIG_PIN, PWM_PIN);

static bool isMotorRun = false;
// Current press force. Initially — default from config.h; overridden from settings.
static uint8_t motorPower = MOTOR_POWER;

void motorInit() {
  motor.runSpeed(0);
}

void motorSetPower(uint8_t p) {
  motorPower = (p < SETTINGS_MOTOR_POWER_MIN) ? SETTINGS_MOTOR_POWER_MIN : p;
}

uint8_t motorGetPower() {
  return motorPower;
}

void spray() {
  if (isMotorRun) {
    DEBUG_PRINTLN("[SPRAY] Already running, ignored");
    return;
  }

  DEBUG_PRINTLN("[SPRAY] Spray started");
  isMotorRun = true;

  // Smooth ramp-up to target power, safe for low values.
  uint8_t ramp[4];
  for (int i = 0; i < 4; i++) {
    int step = (int)motorPower - 10 * (4 - i);   // -40, -30, -20, -10
    ramp[i] = (step > 0) ? (uint8_t)step : 0;
  }
  for (int i = 0; i < 4; i++) {
    motor.runSpeed(ramp[i]);
    delay(70);
  }
  motor.runSpeed(motorPower); delay(400);
  motor.runSpeed(0);

  isMotorRun = false;
  DEBUG_PRINTLN("[SPRAY] Finished");
}

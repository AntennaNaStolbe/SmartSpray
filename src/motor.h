#ifndef MOTOR_H
#define MOTOR_H

#include <GyverMotor2.h>
#include "config.h"

extern GyverMotor2<GM2::DIR_PWM> motor;

void motorInit();
void spray();
// Control press force (0..255). Value is saved/restored from settings.
void motorSetPower(uint8_t power);
uint8_t motorGetPower();

#endif

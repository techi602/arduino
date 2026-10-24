#pragma once

// Copy these values into a local secrets.h file for a private network.
#if defined(__has_include)
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

// Leave empty to disable token authentication. Set a long random value in
// secrets.h before operating the robot on an untrusted network.
#ifndef CONTROL_TOKEN
#define CONTROL_TOKEN ""
#endif

#ifdef ESP32
#ifndef PWMA
#define PWMA 25
#endif
#ifndef PWMB
#define PWMB 26
#endif
#ifndef DIRA
#define DIRA 27
#endif
#ifndef DIRB
#define DIRB 14
#endif
#ifndef POWER_PIN
#define POWER_PIN 33
#endif
#else
#ifndef PWMA
#define PWMA D6
#endif
#ifndef PWMB
#define PWMB D4
#endif
#ifndef DIRA
#define DIRA D7
#endif
#ifndef DIRB
#define DIRB D5
#endif
#ifndef POWER_PIN
#define POWER_PIN D8
#endif
#endif

#define MOTOR_MIN_SPEED 10
#define MOTOR_MAX_SPEED 100
#define MOTOR_COMMAND_TIMEOUT_MS 500UL
#define WIFI_CONNECT_TIMEOUT_MS 15000UL
#define DEVICE_HOSTNAME "t100"
#pragma once

#ifndef MAIN_H_
#define MAIN_H_

//#include <avr/io.h> // all the standard AVR functions
#define __DELAY_BACKWARD_COMPATIBLE__ // this enables uint32 to be used in sleep functions

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstdint>
#include "esphome/core/hal.h"
#include <stdio.h>
#include <inttypes.h>
#include "dw3000_uart.h"
#include "dw3000_port.h"
#include "dw3000_device_api.h"
#include "dw3000_shared_functions.h"

#define _BV(n) (1 << n) // sets 1 at position of BIT "n"
#define __INLINE inline

using byte = uint8_t;
using boolean = bool;

#ifndef bitSet
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#endif

#ifndef bitClear
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#endif

#ifndef bitRead
#define bitRead(value, bit) (((value) >> (bit)) & 0x1U)
#endif

#ifndef delay
#define delay(ms) esphome::delay(ms)
#endif

#ifndef delayMicroseconds
#define delayMicroseconds(us) esphome::delayMicroseconds(us)
#endif

#ifndef millis
#define millis() esphome::millis()
#endif

#endif /* MAIN_H_ */

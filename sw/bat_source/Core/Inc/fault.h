/*
 * fault.h
 *
 *  Created on: Oct 22, 2026
 *      Author: ahorat
 */

#ifndef INC_FAULT_H_
#define INC_FAULT_H_

#include <stdint.h>

void fault_halt(const char *name, uint32_t addr) __attribute__((noreturn));

#endif /* INC_FAULT_H_ */

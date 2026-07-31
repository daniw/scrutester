#ifndef INC_PROTECTION_H_
#define INC_PROTECTION_H_

#include <stdint.h>
#include "statemachine.h"

typedef enum {
    PROTECTION_LEVEL_OK = 0,
    PROTECTION_LEVEL_WARNING,
    PROTECTION_LEVEL_ERROR
} protection_level_t;

/* One bit per monitored source. OVP/OCP are ERROR-only (no warning tier). */
typedef enum {
    PROTECTION_SRC_TEMP_SEC     = (1u << 0),
    PROTECTION_SRC_TEMP_TRAFO   = (1u << 1),
    PROTECTION_SRC_TEMP_CURRENT = (1u << 2),
    PROTECTION_SRC_TEMP_PRIM    = (1u << 3),
    PROTECTION_SRC_OVP          = (1u << 4),
    PROTECTION_SRC_OCP          = (1u << 5),
} protection_source_t;

void protection_init(void);
void protection_update(statemachine_modes_t mode);   /* call once per ~100ms statemachine tick */

protection_level_t protection_get_worst_level(void);
uint16_t protection_get_warning_mask(void);
uint16_t protection_get_error_mask(void);
const char *protection_source_name(protection_source_t src);

#endif /* INC_PROTECTION_H_ */

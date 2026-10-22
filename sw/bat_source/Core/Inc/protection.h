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
    PROTECTION_SRC_LIMIT_V      = (1u << 6), /* per-mode limit, latched */
    PROTECTION_SRC_LIMIT_I      = (1u << 7), /* per-mode limit, latched */
    PROTECTION_SRC_BMS_STALE    = (1u << 8), /* ERROR in CHARGE, else WARNING */
} protection_source_t;

void protection_init(void);
void protection_update(statemachine_modes_t mode);   /* call once per ~100ms statemachine tick */

protection_level_t protection_get_worst_level(void);
uint16_t protection_get_warning_mask(void);
uint16_t protection_get_error_mask(void);
const char *protection_source_name(protection_source_t src);
const char *protection_error_text(void);
void protection_clear_limits(void);

#endif /* INC_PROTECTION_H_ */

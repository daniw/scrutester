/* Host stub: raw encoder count set by the test. */
#include <stdint.h>
extern uint16_t stub_tim_cnt;
static inline uint16_t tim_encoder_read(void) { return stub_tim_cnt; }
static inline void tim_encoder_reset(uint8_t value) { stub_tim_cnt = value; }

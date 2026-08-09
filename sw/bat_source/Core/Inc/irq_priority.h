/**
 ******************************************************************************
 * @file    irq_priority.h
 * @brief   Central NVIC preemption-priority policy for all peripheral IRQs.
 *
 * @details
 * Priority grouping: HAL_Init() (stm32g4xx_hal.c) unconditionally calls
 * HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4) -- this project never
 * overrides it (grepped, no other HAL_NVIC_SetPriorityGrouping() call
 * exists). NVIC_PRIORITYGROUP_4 dedicates all priority bits to preemption
 * (0 subpriority bits), and __NVIC_PRIO_BITS is 4 on STM32G4
 * (stm32g474xx.h), so the usable range is 0..15, lower numeric value = HIGHER
 * priority. Every HAL_NVIC_SetPriority() call's subpriority argument is
 * therefore always 0 -- there is no subpriority field to use.
 *
 * For reference, TICK_INT_PRIORITY (stm32g4xx_hal_conf.h) leaves SysTick at
 * 15, the lowest priority, so it never competes with any peripheral IRQ
 * below.
 *
 * Tiers (least-loaded first numerically = most urgent):
 *
 *   IRQ_PRIO_CTRL_LOOP (0): the ~25 kHz ADC control-loop path. This is
 *   DMA1_Channel2_IRQn specifically, not "all of DMA1": hdma_adc1.Instance =
 *   DMA1_Channel2 (adc.c, HAL_ADC_MspInit()), and HAL_ADC_ConvCpltCallback()
 *   in adc.c only acts (adc_convert_fast_data() + ctrl_main_ctrl(), i.e. the
 *   duty-cycle update) when hadc == &hadc1. No other DMA channel or
 *   peripheral IRQ carries that path, so no other IRQ needs this tier.
 *
 *   IRQ_PRIO_EXT_ADC (4): everything else that is fast measurement
 *   acquisition, but not the control-loop step itself --
 *     - the remaining ADC DMA channels (DMA1_Channel1/ADC5,
 *       DMA1_Channel3/ADC2, DMA1_Channel4/ADC3, DMA1_Channel5/ADC4): these
 *       feed adc_data.raw.* (v_in/v_out/v_term/v_hv/i_iso/i_bat/temps/...)
 *       that adc_convert_fast_data() reads every control-loop tick, so they
 *       should not be starved by low-urgency work, but their own HAL
 *       callback does nothing time-critical (HAL_ADC_ConvCpltCallback()
 *       only branches on hadc1) -- they just need to not be blocked for
 *       long.
 *     - the ADS131M04 external-ADC path: SPI3_IRQn (the SPI3 transfer
 *       itself), DMA1_Channel6/DMA1_Channel7 (hdma_spi3_rx/tx, spi.c), and
 *       EXTI2_IRQn (the ADS131 DRDY pin, ads131m04.c) -- all three are one
 *       logical pipeline (DRDY -> SPI3 DMA transfer -> completion), so they
 *       share a tier rather than being split across "DMA" vs "peripheral"
 *       buckets.
 *
 *   IRQ_PRIO_AUX (8): everything else -- TIM2_IRQn (state machine tick,
 *   millisecond-scale, does not need to preempt short measurement
 *   transfers), TIM6_DAC_IRQn (1 Hz/50% duty square wave for the 1 A
 *   resistance test), I2C4_EV_IRQn/I2C4_ER_IRQn plus their DMA channels
 *   (DMA1_Channel8/DMA2_Channel1, i2c.c), and SPI4_IRQn plus its DMA channel
 *   (DMA2_Channel2, spi.c) for the LCD. A peripheral's own IRQ and its DMA
 *   channel(s) are kept at the same tier throughout this policy (see
 *   IRQ_PRIO_EXT_ADC above) since they are the same logical transfer and
 *   splitting them serves no purpose.
 *
 * USART2 and QUADSPI (usart.c, quadspi.c) are polled in this project --
 * neither registers a DMA channel, an IRQ handler or an NVIC priority -- so
 * they are not assigned a tier here.
 *
 * Numeric values are spaced (0, 4, 8) rather than packed (0, 1, 2) to leave
 * headroom for inserting an intermediate tier later without renumbering
 * everything else.
 *
 ******************************************************************************
 */
#ifndef __IRQ_PRIORITY_H__
#define __IRQ_PRIORITY_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Tier 0 (highest): the ADC1 control-loop DMA completion (DMA1_Channel2). */
#define IRQ_PRIO_CTRL_LOOP   0U

/* Tier 1: other ADC DMA channels + the ADS131M04 external-ADC pipeline
 * (SPI3 IRQ, its RX/TX DMA channels, and the DRDY EXTI2 line). */
#define IRQ_PRIO_EXT_ADC     4U

/* Tier 2: state machine tick, DAC square wave, I2C4 (+ DMA), SPI4/LCD
 * (+ DMA). */
#define IRQ_PRIO_AUX         8U

/* Priority grouping is NVIC_PRIORITYGROUP_4 (set by HAL_Init()): all 4
 * implemented priority bits are preemption bits, 0 are subpriority bits.
 * Every HAL_NVIC_SetPriority() call in this project must therefore pass 0
 * for the subpriority argument. */
#define IRQ_SUBPRIO_NONE     0U

#ifdef __cplusplus
}
#endif

#endif /* __IRQ_PRIORITY_H__ */

/**
 * SDK-1.1M Lab 1: blink the green user LED on PD13.
 *
 * This starter project contains the STM32F427 startup code but no generated
 * HAL/CMSIS files, so the small GPIO and SysTick setup below uses the device
 * registers directly. The MCU remains on its reset-default 16 MHz HSI clock.
 */

#include <stdint.h>

#define REG32(address) (*(volatile uint32_t *)(address))

#define RCC_AHB1ENR REG32(0x40023830UL)

#define GPIOD_MODER   REG32(0x40020C00UL)
#define GPIOD_OTYPER  REG32(0x40020C04UL)
#define GPIOD_OSPEEDR REG32(0x40020C08UL)
#define GPIOD_PUPDR   REG32(0x40020C0CUL)
#define GPIOD_BSRR    REG32(0x40020C18UL)

#define SYST_CSR REG32(0xE000E010UL)
#define SYST_RVR REG32(0xE000E014UL)
#define SYST_CVR REG32(0xE000E018UL)

#define GREEN_LED_PIN 13U
#define GREEN_LED_MASK (1UL << GREEN_LED_PIN)
#define BLINK_INTERVAL_MS 500U
#define CORE_CLOCK_HZ 16000000UL

static volatile uint32_t system_millis;

static void gpio_init(void)
{
    const uint32_t pin_mode_shift = GREEN_LED_PIN * 2U;

    /* Enable the GPIOD peripheral clock before accessing its registers. */
    RCC_AHB1ENR |= (1UL << 3U);
    (void)RCC_AHB1ENR;

    /* Start with the LED off, then configure PD13 as a low-speed output. */
    GPIOD_BSRR = GREEN_LED_MASK << 16U;
    GPIOD_MODER &= ~(3UL << pin_mode_shift);
    GPIOD_OTYPER &= ~GREEN_LED_MASK;
    GPIOD_OSPEEDR &= ~(3UL << pin_mode_shift);
    GPIOD_PUPDR &= ~(3UL << pin_mode_shift);
    GPIOD_MODER |= (1UL << pin_mode_shift);
}

static void systick_init(void)
{
    /* One interrupt per millisecond using the reset-default 16 MHz HSI. */
    SYST_RVR = (CORE_CLOCK_HZ / 1000UL) - 1UL;
    SYST_CVR = 0UL;
    SYST_CSR = 7UL; /* clock source = processor clock, interrupt + counter */
}

void SysTick_Handler(void)
{
    ++system_millis;
}

static uint32_t millis(void)
{
    return system_millis;
}

static void green_led_set(int on)
{
    /* BSRR updates one output atomically: upper half resets, lower sets. */
    GPIOD_BSRR = on ? GREEN_LED_MASK : (GREEN_LED_MASK << 16U);
}

int main(void)
{
    uint32_t last_toggle = 0UL;
    int led_on = 0;

    gpio_init();
    systick_init();

    for (;;) {
        const uint32_t now = millis();
        if ((uint32_t)(now - last_toggle) >= BLINK_INTERVAL_MS) {
            last_toggle = now;
            led_on = !led_on;
            green_led_set(led_on);
        }
    }
}

#include "board.h"

static const int assignment_part = 1;
volatile uint32_t system_millis;

void SysTick_Handler(void)
{
    ++system_millis;
}

static void gpio_init(void)
{
    *RCC_AHB1ENR |= (1U << 2U) | (1U << 3U);
    (void)*RCC_AHB1ENR;

    *GPIOD_BSRR = (uint32_t)LED_MASK << 16U;
    for (uint32_t pin = GREEN_PIN; pin <= RED_PIN; ++pin) {
        *GPIOD_MODER &= ~(3U << (pin * 2U));
        *GPIOD_OTYPER &= ~(1U << pin);
        *GPIOD_OSPEEDR &= ~(3U << (pin * 2U));
        *GPIOD_PUPDR &= ~(3U << (pin * 2U));
        *GPIOD_MODER |= (1U << (pin * 2U));
    }

    *GPIOC_MODER &= ~(3U << (BUTTON_PIN * 2U));
    *GPIOC_PUPDR &= ~(3U << (BUTTON_PIN * 2U));
    *GPIOC_PUPDR |= (1U << (BUTTON_PIN * 2U));
}

static void systick_init(void)
{
    *SYST_RVR = (CORE_CLOCK_HZ / 1000U) - 1U;
    *SYST_CVR = 0U;
    *SYST_CSR = 7U;
}

int main(void)
{
    gpio_init();
    systick_init();

    if (assignment_part == 1) {
        part1_run();
    } else {
        part2_run();
    }

    for (;;) {
    }
}

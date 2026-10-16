/* Part 1: four resumable garland modes selected by the PC15 button. */
#include <stdint.h>

#define REG32(address) (*(volatile uint32_t *)(address))
#define RCC_AHB1ENR REG32(0x40023830UL)
#define GPIOD_MODER REG32(0x40020C00UL)
#define GPIOD_OTYPER REG32(0x40020C04UL)
#define GPIOD_OSPEEDR REG32(0x40020C08UL)
#define GPIOD_PUPDR REG32(0x40020C0CUL)
#define GPIOC_IDR REG32(0x40020810UL)
#define GPIOC_MODER REG32(0x40020800UL)
#define GPIOC_PUPDR REG32(0x4002080CUL)
#define GPIOD_BSRR REG32(0x40020C18UL)
#define SYST_CSR REG32(0xE000E010UL)
#define SYST_RVR REG32(0xE000E014UL)
#define SYST_CVR REG32(0xE000E018UL)

#define GREEN (1UL << 13U)
#define YELLOW (1UL << 14U)
#define RED (1UL << 15U)
#define LEDS (GREEN | YELLOW | RED)
#define BUTTON (1UL << 15U)
#define CLOCK_HZ 16000000UL

typedef struct {
    uint32_t states[8];
    uint8_t length;
    uint8_t index;
    uint16_t period_ms;
    uint32_t changed_at;
} GarlandMode;

static volatile uint32_t ticks_ms;
static uint8_t active_mode;
static uint8_t button_candidate;
static uint8_t button_state;
static uint32_t button_changed_at;

static GarlandMode modes[4] = {
    {{GREEN, RED, YELLOW, GREEN}, 4, 0, 200, 0},
    {{RED, YELLOW, GREEN, RED}, 4, 0, 500, 0},
    {{GREEN | YELLOW, RED, GREEN | RED, YELLOW}, 4, 0, 350, 0},
    {{RED | YELLOW, GREEN, GREEN | YELLOW, RED}, 4, 0, 800, 0}
};

void part1_systick_handler(void) { ++ticks_ms; }

static void gpio_init(void)
{
    RCC_AHB1ENR |= (1UL << 2U) | (1UL << 3U);
    (void)RCC_AHB1ENR;

    GPIOD_BSRR = LEDS << 16U;
    for (uint32_t pin = 13U; pin <= 15U; ++pin) {
        GPIOD_MODER &= ~(3UL << (pin * 2U));
        GPIOD_MODER |= (1UL << (pin * 2U));
        GPIOD_OTYPER &= ~(1UL << pin);
        GPIOD_OSPEEDR &= ~(3UL << (pin * 2U));
        GPIOD_PUPDR &= ~(3UL << (pin * 2U));
    }

    GPIOC_MODER &= ~(3UL << 30U);
    GPIOC_PUPDR &= ~(3UL << 30U);
    GPIOC_PUPDR |= (1UL << 30U); /* button pull-up */
}

static void systick_init(void)
{
    SYST_RVR = CLOCK_HZ / 1000UL - 1UL;
    SYST_CVR = 0UL;
    SYST_CSR = 7UL;
}

static void show_state(uint32_t state)
{
    GPIOD_BSRR = LEDS << 16U; /* turn all LEDs off */
    GPIOD_BSRR = state;       /* then turn on the requested LED(s) */
}

static void select_next_mode(void)
{
    active_mode = (uint8_t)((active_mode + 1U) % 4U);
    modes[active_mode].changed_at = ticks_ms;
    show_state(modes[active_mode].states[modes[active_mode].index]);
}

void part1_run(void)
{
    gpio_init();
    systick_init();
    show_state(modes[0].states[0]);

    for (;;) {
        const uint32_t now = ticks_ms;
        const uint8_t pressed = ((GPIOC_IDR & BUTTON) == 0UL) ? 1U : 0U;

        /* Debounce both press and release; one press advances one mode. */
        if (pressed != button_candidate) {
            button_candidate = pressed;
            button_changed_at = now;
        } else if (button_candidate != button_state &&
                   (uint32_t)(now - button_changed_at) >= 30UL) {
            button_state = button_candidate;
            if (button_state != 0U) select_next_mode();
        }

        GarlandMode *mode = &modes[active_mode];
        if ((uint32_t)(now - mode->changed_at) >= mode->period_ms) {
            mode->changed_at += mode->period_ms;
            mode->index = (uint8_t)((mode->index + 1U) % mode->length);
            show_state(mode->states[mode->index]);
        }
    }
}

/*
 * SDK-1.1M garland for STM32F427VITx.
 * Register-level implementation: LEDs PD13..PD15, button PC15,
 * USART6 on PC6/PC7 at 38400 8N1. The reset-default clock is 16 MHz HSI.
 */
#include <stdint.h>

#define REG32(address) (*(volatile uint32_t *)(address))

#define RCC_AHB1ENR   REG32(0x40023830UL)
#define RCC_APB2ENR   REG32(0x40023844UL)
#define GPIOD_MODER   REG32(0x40020C00UL)
#define GPIOD_OTYPER  REG32(0x40020C04UL)
#define GPIOD_OSPEEDR REG32(0x40020C08UL)
#define GPIOD_PUPDR   REG32(0x40020C0CUL)
#define GPIOD_BSRR    REG32(0x40020C18UL)
#define GPIOC_MODER   REG32(0x40020800UL)
#define GPIOC_PUPDR   REG32(0x4002080CUL)
#define GPIOC_IDR     REG32(0x40020810UL)
#define GPIOC_AFRL    REG32(0x40020820UL)
#define SYSCFG_EXTICR4 REG32(0x40013814UL)
#define EXTI_IMR      REG32(0x40013C00UL)
#define EXTI_RTSR     REG32(0x40013C08UL)
#define EXTI_FTSR     REG32(0x40013C0CUL)
#define EXTI_PR       REG32(0x40013C14UL)
#define NVIC_ISER1    REG32(0xE000E104UL)
#define NVIC_ICER1    REG32(0xE000E184UL)
#define USART6_SR     REG32(0x40011400UL)
#define USART6_DR     REG32(0x40011404UL)
#define USART6_BRR    REG32(0x40011408UL)
#define USART6_CR1    REG32(0x4001140CUL)
#define USART6_CR2    REG32(0x40011410UL)
#define USART6_CR3    REG32(0x40011414UL)
#define SYST_CSR      REG32(0xE000E010UL)
#define SYST_RVR      REG32(0xE000E014UL)
#define SYST_CVR      REG32(0xE000E018UL)

#define GREEN_PIN 13U
#define YELLOW_PIN 14U
#define RED_PIN   15U
#define LED_MASK ((1UL << GREEN_PIN) | (1UL << RED_PIN) | (1UL << YELLOW_PIN))
#define BUTTON_PIN 15U
#define CORE_CLOCK_HZ 16000000UL
#define UART_BAUD 38400UL
#define MODE_COUNT 8U
#define USER_MODE_FIRST 4U
#define LINE_CAPACITY 32U

typedef struct {
    char states[8];
    uint8_t length;
    uint8_t index;
    uint16_t period_ms;
    uint32_t changed_at;
    uint8_t configured;
} Sequence;

static volatile uint32_t system_millis;
static volatile uint8_t button_event;
static volatile uint32_t last_button_irq_ms;
static Sequence sequences[MODE_COUNT] = {
    {{'g','n','r','n'}, 4, 0, 200, 0, 1},
    {{'g','y','r','n'}, 4, 0, 500, 0, 1},
    {{'r','n','y','n','g','n'}, 6, 0, 1000, 0, 1},
    {{'g','g','n','y','n','r','n'}, 7, 0, 350, 0, 1}
};
static uint8_t active_mode;
static uint8_t next_user_slot = USER_MODE_FIRST;
static uint8_t interrupts_enabled = 1;
static uint8_t button_latched;
static uint32_t button_changed_at;

static uint32_t millis(void) { return system_millis; }

static uint8_t string_is(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) { ++a; ++b; }
    return (*a == '\0' && *b == '\0') ? 1U : 0U;
}

static uint8_t starts_with(const char *s, const char *prefix)
{
    while (*prefix != '\0' && *s == *prefix) { ++s; ++prefix; }
    return (*prefix == '\0') ? 1U : 0U;
}

void part2_systick_handler(void) { ++system_millis; }

static void uart_putc(char c)
{
    while ((USART6_SR & (1UL << 7U)) == 0UL) { }
    USART6_DR = (uint32_t)(uint8_t)c;
}

static void uart_write(const char *s)
{
    while (*s != '\0') {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

static void uart_write_number(uint8_t value)
{
    uart_putc((char)('0' + value));
}

static void gpio_init(void)
{
    RCC_AHB1ENR |= (1UL << 2U) | (1UL << 3U); /* GPIOC, GPIOD */
    (void)RCC_AHB1ENR;

    GPIOD_BSRR = LED_MASK << 16U;
    for (uint32_t pin = GREEN_PIN; pin <= RED_PIN; ++pin) {
        GPIOD_MODER &= ~(3UL << (pin * 2U));
        GPIOD_OTYPER &= ~(1UL << pin);
        GPIOD_OSPEEDR &= ~(3UL << (pin * 2U));
        GPIOD_PUPDR &= ~(3UL << (pin * 2U));
        GPIOD_MODER |= (1UL << (pin * 2U));
    }

    /* PC15 button is active low; use internal pull-up. */
    GPIOC_MODER &= ~(3UL << (BUTTON_PIN * 2U));
    GPIOC_PUPDR &= ~(3UL << (BUTTON_PIN * 2U));
    GPIOC_PUPDR |= (1UL << (BUTTON_PIN * 2U));
}

static void uart_init(void)
{
    /* USART6 uses PC6=TX and PC7=RX, alternate function 8. */
    RCC_AHB1ENR |= (1UL << 2U);
    RCC_APB2ENR |= (1UL << 5U); /* USART6 clock */
    (void)RCC_APB2ENR;

    GPIOC_MODER &= ~((3UL << 12U) | (3UL << 14U));
    GPIOC_MODER |= (2UL << 12U) | (2UL << 14U);
    GPIOC_AFRL &= ~((15UL << 24U) | (15UL << 28U));
    GPIOC_AFRL |= (8UL << 24U) | (8UL << 28U);
    GPIOC_PUPDR &= ~((3UL << 12U) | (3UL << 14U));
    GPIOC_PUPDR |= (1UL << 14U); /* RX pull-up */

    USART6_CR1 = 0UL;
    USART6_CR2 = 0UL; /* one stop bit */
    USART6_CR3 = 0UL; /* no flow control */
    USART6_BRR = (CORE_CLOCK_HZ + (UART_BAUD / 2UL)) / UART_BAUD;
    USART6_CR1 = (1UL << 13U) | (1UL << 3U) | (1UL << 2U); /* UE, TE, RE */
}

static void systick_init(void)
{
    SYST_RVR = (CORE_CLOCK_HZ / 1000UL) - 1UL;
    SYST_CVR = 0UL;
    SYST_CSR = 7UL;
}

static void button_interrupt_init(void)
{
    RCC_APB2ENR |= (1UL << 14U); /* SYSCFG clock */
    SYSCFG_EXTICR4 &= ~(15UL << 12U);
    SYSCFG_EXTICR4 |= (2UL << 12U); /* EXTI15 from port C */
    EXTI_RTSR &= ~(1UL << BUTTON_PIN);
    EXTI_FTSR |= (1UL << BUTTON_PIN); /* falling edge: press */
    EXTI_PR = (1UL << BUTTON_PIN);
    EXTI_IMR |= (1UL << BUTTON_PIN);
    NVIC_ISER1 = (1UL << 8U); /* EXTI15_10 is IRQ 40 */
}

void EXTI15_10_IRQHandler(void)
{
    if ((EXTI_PR & (1UL << BUTTON_PIN)) != 0UL) {
        EXTI_PR = (1UL << BUTTON_PIN);
        const uint32_t now = millis();
        if ((uint32_t)(now - last_button_irq_ms) >= 150UL) {
            last_button_irq_ms = now;
            button_event = 1U;
        }
    }
}

static void all_leds_off(void) { GPIOD_BSRR = LED_MASK << 16U; }

static void show_state(char state)
{
    uint32_t pin = 0UL;
    if (state == 'g') pin = GREEN_PIN;
    else if (state == 'r') pin = RED_PIN;
    else if (state == 'y') pin = YELLOW_PIN;
    all_leds_off();
    if (pin != 0UL) GPIOD_BSRR = (1UL << pin);
}

static void select_mode(uint8_t mode)
{
    if (mode >= MODE_COUNT || sequences[mode].configured == 0U) {
        uart_write("No such sequence.\n");
        return;
    }
    active_mode = mode;
    sequences[active_mode].changed_at = millis();
    show_state(sequences[active_mode].states[sequences[active_mode].index]);
    uart_write("Active sequence ");
    uart_write_number((uint8_t)(mode + 1U));
    uart_write(".\n");
}

static void next_mode(void)
{
    uint8_t candidate = active_mode;
    for (uint8_t checked = 0; checked < MODE_COUNT; ++checked) {
        candidate = (uint8_t)((candidate + 1U) % MODE_COUNT);
        if (sequences[candidate].configured != 0U) {
            select_mode(candidate);
            return;
        }
    }
}

static void toggle_interrupt_mode(uint8_t enabled)
{
    interrupts_enabled = enabled;
    if (enabled != 0U) {
        EXTI_PR = (1UL << BUTTON_PIN);
        EXTI_IMR |= (1UL << BUTTON_PIN);
        NVIC_ISER1 = (1UL << 8U);
        uart_write("Button EXTI interrupts on.\n");
    } else {
        EXTI_IMR &= ~(1UL << BUTTON_PIN);
        NVIC_ICER1 = (1UL << 8U);
        uart_write("Button EXTI interrupts off; button is polled.\n");
    }
}

static uint8_t uart_getc(char *out)
{
    if ((USART6_SR & (1UL << 5U)) == 0UL) return 0U;
    *out = (char)(USART6_DR & 0xFFUL);
    return 1U;
}

static uint8_t parse_mode_number(const char *s, uint8_t *mode)
{
    if (s[0] < '1' || s[0] > '8' || s[1] != '\0') return 0U;
    *mode = (uint8_t)(s[0] - '1');
    return 1U;
}

static uint8_t valid_sequence(const char *s, uint8_t *length)
{
    uint8_t n = 0U;
    while (s[n] != '\0') {
        const char c = s[n];
        if (c != 'g' && c != 'r' && c != 'y' && c != 'n') return 0U;
        if (++n > 8U) return 0U;
    }
    if (n < 2U) return 0U;
    *length = n;
    return 1U;
}

static uint8_t parse_period(const char *s, uint16_t *period)
{
    if (string_is(s, "fast") != 0U || string_is(s, "200") != 0U) { *period = 200U; return 1U; }
    if (string_is(s, "medium") != 0U || string_is(s, "500") != 0U) { *period = 500U; return 1U; }
    if (string_is(s, "slow") != 0U || string_is(s, "1000") != 0U) { *period = 1000U; return 1U; }
    return 0U;
}

static void process_command(char *line, uint8_t *awaiting_period,
                            char *pending_states, uint8_t *pending_length)
{
    if (*awaiting_period != 0U) {
        uint16_t period;
        if (parse_period(line, &period) == 0U) {
            uart_write("Use fast/medium/slow or 200/500/1000.\nPeriod: ");
            return;
        }
        Sequence *sequence = &sequences[next_user_slot];
        for (uint8_t i = 0; i < *pending_length; ++i) sequence->states[i] = pending_states[i];
        sequence->length = *pending_length;
        sequence->index = 0U;
        sequence->period_ms = period;
        sequence->changed_at = millis();
        sequence->configured = 1U;
        uart_write("Saved as sequence ");
        uart_write_number((uint8_t)(next_user_slot + 1U));
        uart_write(".\n");
        next_user_slot = (uint8_t)(USER_MODE_FIRST +
                         ((next_user_slot - USER_MODE_FIRST + 1U) % 4U));
        *awaiting_period = 0U;
        return;
    }

    if (starts_with(line, "new ") != 0U) {
        uint8_t length;
        if (valid_sequence(&line[4], &length) == 0U) {
            uart_write("Sequence must contain 2-8 chars: g, r, y, n.\n");
            return;
        }
        for (uint8_t i = 0; i < length; ++i) pending_states[i] = line[4U + i];
        pending_states[length] = '\0';
        *pending_length = length;
        *awaiting_period = 1U;
        uart_write("Period? fast (200), medium (500), slow (1000) ms: ");
        return;
    }
    if (string_is(line, "set interrupts on") != 0U) { toggle_interrupt_mode(1U); return; }
    if (string_is(line, "set interrupts off") != 0U) { toggle_interrupt_mode(0U); return; }
    if (starts_with(line, "set ") != 0U) {
        uint8_t mode;
        if (parse_mode_number(&line[4], &mode) != 0U) select_mode(mode);
        else uart_write("Use: set 1..8\n");
        return;
    }
    uart_write("Commands: new gryn, set 1..8, set interrupts on|off\n");
}

void part2_run(void)
{
    char line[LINE_CAPACITY];
    char pending_states[9];
    uint8_t line_length = 0U;
    uint8_t awaiting_period = 0U;
    uint8_t pending_length = 0U;
    uint8_t previous_button_pressed = 0U;

    gpio_init();
    uart_init();
    systick_init();
    button_interrupt_init();
    uart_write("Garland ready. Commands: new gryn, set 1..8, set interrupts on|off\n");
    select_mode(0U);

    for (;;) {
        char received;
        if (uart_getc(&received) != 0U) {
            if (received == '\r' || received == '\n') {
                if (received == '\r') uart_write("\n");
                if (line_length != 0U) {
                    line[line_length] = '\0';
                    process_command(line, &awaiting_period, pending_states, &pending_length);
                    line_length = 0U;
                }
            } else if (received == 8 || received == 127) {
                if (line_length != 0U) {
                    --line_length;
                    uart_write("\b \b");
                }
            } else if (line_length < LINE_CAPACITY - 1U) {
                line[line_length++] = received;
                uart_putc(received);
            }
        }

        const uint32_t now = millis();
        const uint8_t pressed = ((GPIOC_IDR & (1UL << BUTTON_PIN)) == 0UL) ? 1U : 0U;
        if (interrupts_enabled == 0U) {
            if (pressed != previous_button_pressed) {
                previous_button_pressed = pressed;
                button_changed_at = now;
            } else if (pressed != 0U && button_latched == 0U &&
                       (uint32_t)(now - button_changed_at) >= 30UL) {
                button_latched = 1U;
                next_mode();
            } else if (pressed == 0U && button_latched != 0U &&
                       (uint32_t)(now - button_changed_at) >= 30UL) {
                button_latched = 0U;
            }
        } else {
            if (pressed == 0U) button_latched = 0U;
            if (button_event != 0U) {
                button_event = 0U;
                if (button_latched == 0U) {
                    button_latched = 1U;
                    next_mode();
                }
            }
        }

        Sequence *current = &sequences[active_mode];
        if ((uint32_t)(now - current->changed_at) >= current->period_ms) {
            current->changed_at += current->period_ms;
            current->index = (uint8_t)((current->index + 1U) % current->length);
            show_state(current->states[current->index]);
        }
    }
}

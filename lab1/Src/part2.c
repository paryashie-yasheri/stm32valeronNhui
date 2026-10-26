#include "board.h"
#include <string.h>

enum { UART_BAUD = 38400, MODE_COUNT = 8, USER_MODE_FIRST = 4, LINE_CAPACITY = 32 };

typedef struct {
    char states[8];
    int length;
    int index;
    uint32_t period_ms;
    uint32_t changed_at;
} Sequence;

static volatile bool button_event;
static volatile uint32_t last_button_irq_ms;
static Sequence sequences[MODE_COUNT] = {{{'g', 'n', 'r', 'n'}, 4, 0, 200, 0},
                                         {{'g', 'y', 'r', 'n'}, 4, 0, 500, 0},
                                         {{'r', 'n', 'y', 'n', 'g', 'n'}, 6, 0, 1000, 0},
                                         {{'g', 'g', 'n', 'y', 'n', 'r', 'n'}, 7, 0, 350, 0}};
static char pending_states[9];
static bool awaiting_period;
static int pending_length;
static int active_mode;
static int next_user_slot = USER_MODE_FIRST;
static bool interrupts_enabled = true;
static bool button_latched;
static uint32_t button_changed_at;

static void uart_putc(char c)
{
    while ((*USART6_SR & (1U << 7)) == 0) {
    }
    *USART6_DR = (unsigned char)c;
}

static void uart_write(const char *s)
{
    while (*s != '\0') {
        if (*s == '\n') {
            uart_putc('\r');
        }
        uart_putc(*s++);
    }
}

static void uart_write_number(int value)
{
    uart_putc((char)('0' + value));
}

static void uart_init(void)
{
    *RCC_AHB1ENR |= (1U << 2);
    *RCC_APB2ENR |= (1U << 5);
    (void)*RCC_APB2ENR;

    *GPIOC_MODER &= ~((3U << 12) | (3U << 14));
    *GPIOC_MODER |= (2U << 12) | (2U << 14);
    *GPIOC_AFRL &= ~((15U << 24) | (15U << 28));
    *GPIOC_AFRL |= (8U << 24) | (8U << 28);
    *GPIOC_PUPDR &= ~((3U << 12) | (3U << 14));
    *GPIOC_PUPDR |= (1U << 14);

    *USART6_CR1 = 0;
    *USART6_CR2 = 0;
    *USART6_CR3 = 0;
    *USART6_BRR = (CORE_CLOCK_HZ + (UART_BAUD / 2)) / UART_BAUD;
    *USART6_CR1 = (1U << 13) | (1U << 3) | (1U << 2);
}

static void button_interrupt_init(void)
{
    *RCC_APB2ENR |= (1U << 14);
    *SYSCFG_EXTICR4 &= ~(15U << 12);
    *SYSCFG_EXTICR4 |= (2U << 12);
    *EXTI_RTSR &= ~(1U << BUTTON_PIN);
    *EXTI_FTSR |= (1U << BUTTON_PIN);
    *EXTI_PR = (1U << BUTTON_PIN);
    *EXTI_IMR |= (1U << BUTTON_PIN);
    *NVIC_ISER1 = (1U << 8);
}

void EXTI15_10_IRQHandler(void)
{
    if ((*EXTI_PR & (1U << BUTTON_PIN)) != 0) {
        *EXTI_PR = (1U << BUTTON_PIN);
        const uint32_t now = system_millis;
        if ((uint32_t)(now - last_button_irq_ms) >= 150) {
            last_button_irq_ms = now;
            button_event = true;
        }
    }
}

static void show_state(char state)
{
    uint32_t pin = 0;
    if (state == 'g') {
        pin = GREEN_PIN;
    } else if (state == 'r') {
        pin = RED_PIN;
    } else if (state == 'y') {
        pin = YELLOW_PIN;
    }
    *GPIOD_BSRR = (uint32_t)LED_MASK << 16;
    if (pin != 0) {
        *GPIOD_BSRR = (1U << pin);
    }
}

static void select_mode(int mode)
{
    if (mode < 0 || mode >= MODE_COUNT || sequences[mode].length == 0) {
        uart_write("No such sequence.\n");
        return;
    }
    active_mode = mode;
    sequences[active_mode].changed_at = system_millis;
    show_state(sequences[active_mode].states[sequences[active_mode].index]);
    uart_write("Active sequence ");
    uart_write_number(mode + 1);
    uart_write(".\n");
}

static void next_mode(void)
{
    int candidate = active_mode;
    for (int checked = 0; checked < MODE_COUNT; ++checked) {
        candidate = (candidate + 1) % MODE_COUNT;
        if (sequences[candidate].length != 0) {
            select_mode(candidate);
            return;
        }
    }
}

static void set_button_interrupts(bool enabled)
{
    interrupts_enabled = enabled;
    if (enabled) {
        *EXTI_PR = (1U << BUTTON_PIN);
        *EXTI_IMR |= (1U << BUTTON_PIN);
        *NVIC_ISER1 = (1U << 8);
        uart_write("Button EXTI interrupts on.\n");
    } else {
        *EXTI_IMR &= ~(1U << BUTTON_PIN);
        *NVIC_ICER1 = (1U << 8);
        uart_write("Button EXTI interrupts off; button is polled.\n");
    }
}

static bool uart_getc(char *out)
{
    if ((*USART6_SR & (1U << 5)) == 0) {
        return false;
    }
    *out = (char)(*USART6_DR & 0xFFUL);
    return true;
}

static bool parse_mode_number(const char *s, int *mode)
{
    if (s[0] < '1' || s[0] > '8' || s[1] != '\0') {
        return false;
    }
    *mode = s[0] - '1';
    return true;
}

static bool valid_sequence(const char *s, int *length)
{
    int n = 0;
    while (s[n] != '\0') {
        const char c = s[n];
        if (c != 'g' && c != 'r' && c != 'y' && c != 'n') {
            return false;
        }
        if (++n > 8) {
            return false;
        }
    }
    if (n < 2) {
        return false;
    }
    *length = n;
    return true;
}

static bool parse_period(const char *s, int *period)
{
    if (strcmp(s, "fast") == 0 || strcmp(s, "200") == 0) {
        *period = 200;
        return true;
    }
    if (strcmp(s, "medium") == 0 || strcmp(s, "500") == 0) {
        *period = 500;
        return true;
    }
    if (strcmp(s, "slow") == 0 || strcmp(s, "1000") == 0) {
        *period = 1000;
        return true;
    }
    return false;
}

static void process_command(const char *line)
{
    if (awaiting_period) {
        int period;
        if (!parse_period(line, &period)) {
            uart_write("Use fast/medium/slow or 200/500/1000.\nPeriod: ");
            return;
        }
        Sequence *sequence = &sequences[next_user_slot];
        memcpy(sequence->states, pending_states, pending_length);
        sequence->length = pending_length;
        sequence->index = 0;
        sequence->period_ms = period;
        sequence->changed_at = system_millis;
        uart_write("Saved as sequence ");
        uart_write_number(next_user_slot + 1);
        uart_write(".\n");
        ++next_user_slot;
        if (next_user_slot == MODE_COUNT) {
            next_user_slot = USER_MODE_FIRST;
        }
        awaiting_period = false;
        return;
    }

    if (strncmp(line, "new ", 4) == 0) {
        int length;
        if (!valid_sequence(&line[4], &length)) {
            uart_write("Sequence must contain 2-8 chars: g, r, y, n.\n");
            return;
        }
        memcpy(pending_states, line + 4, length);
        pending_states[length] = '\0';
        pending_length = length;
        awaiting_period = true;
        uart_write("Period? fast (200), medium (500), slow (1000) ms: ");
        return;
    }
    if (strcmp(line, "set interrupts on") == 0) {
        set_button_interrupts(true);
        return;
    }
    if (strcmp(line, "set interrupts off") == 0) {
        set_button_interrupts(false);
        return;
    }
    if (strncmp(line, "set ", 4) == 0) {
        int mode;
        if (parse_mode_number(&line[4], &mode)) {
            select_mode(mode);
        } else {
            uart_write("Use: set 1..8\n");
        }
        return;
    }
    uart_write("Commands: new gryn, set 1..8, set interrupts on|off\n");
}

static void read_command(void)
{
    static char line[LINE_CAPACITY];
    static int line_length;

    char received;
    if (uart_getc(&received)) {
        if (received == '\r' || received == '\n') {
            if (received == '\r') {
                uart_write("\n");
            }
            if (line_length != 0) {
                line[line_length] = '\0';
                process_command(line);
                line_length = 0;
            }
        } else if (received == 8 || received == 127) {
            if (line_length != 0) {
                --line_length;
                uart_write("\b \b");
            }
        } else if (line_length < LINE_CAPACITY - 1) {
            line[line_length++] = received;
            uart_putc(received);
        }
    }
}

static void update_button(void)
{
    static bool previous_button_pressed;

    const uint32_t now = system_millis;
    const bool pressed = (*GPIOC_IDR & (1U << BUTTON_PIN)) == 0;
    if (!interrupts_enabled) {
        if (pressed != previous_button_pressed) {
            previous_button_pressed = pressed;
            button_changed_at = now;
        } else if (pressed && !button_latched && (uint32_t)(now - button_changed_at) >= 30) {
            button_latched = true;
            next_mode();
        } else if (!pressed && button_latched && (uint32_t)(now - button_changed_at) >= 30) {
            button_latched = false;
        }
    } else {
        if (!pressed) {
            button_latched = false;
        }
        if (button_event) {
            button_event = false;
            if (!button_latched) {
                button_latched = true;
                next_mode();
            }
        }
    }
}

static void update_sequence(void)
{
    const uint32_t now = system_millis;
    Sequence *current = &sequences[active_mode];
    if ((uint32_t)(now - current->changed_at) >= current->period_ms) {
        current->changed_at += current->period_ms;
        current->index = (current->index + 1) % current->length;
        show_state(current->states[current->index]);
    }
}

void part2_run(void)
{
    uart_init();
    button_interrupt_init();
    uart_write("Garland ready. Commands: new gryn, set 1..8, set interrupts on|off\n");
    select_mode(0);

    for (;;) {
        read_command();
        update_button();
        update_sequence();
    }
}

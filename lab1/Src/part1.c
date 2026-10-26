#include "board.h"

typedef struct {
    uint32_t states[4];
    int length;
    int index;
    uint32_t period_ms;
    uint32_t changed_at;
} GarlandMode;

static int active_mode;
static bool button_candidate;
static bool button_state;
static uint32_t button_changed_at;

static GarlandMode modes[4] = {{{GREEN, RED, YELLOW, GREEN}, 4, 0, 200, 0},
                               {{RED, YELLOW, GREEN, RED}, 4, 0, 500, 0},
                               {{GREEN | YELLOW, RED, GREEN | RED, YELLOW}, 4, 0, 350, 0},
                               {{RED | YELLOW, GREEN, GREEN | YELLOW, RED}, 4, 0, 800, 0}};

static void show_state(uint32_t state)
{
    *GPIOD_BSRR = ((LED_MASK & ~state) << 16) | state;
}

static void select_next_mode(uint32_t now)
{
    active_mode = (active_mode + 1) % 4;
    modes[active_mode].changed_at = now;
    show_state(modes[active_mode].states[modes[active_mode].index]);
}

void part1_run(void)
{
    show_state(modes[0].states[0]);

    for (;;) {
        const uint32_t now = system_millis;
        const bool pressed = (*GPIOC_IDR & (1U << BUTTON_PIN)) == 0;

        if (pressed != button_candidate) {
            button_candidate = pressed;
            button_changed_at = now;
        } else if (button_candidate != button_state && (uint32_t)(now - button_changed_at) >= 30) {
            button_state = button_candidate;
            if (button_state) {
                select_next_mode(now);
            }
        }

        GarlandMode *mode = &modes[active_mode];
        if ((uint32_t)(now - mode->changed_at) >= mode->period_ms) {
            mode->changed_at += mode->period_ms;
            mode->index = (mode->index + 1) % mode->length;
            show_state(mode->states[mode->index]);
        }
    }
}

/* Select the assignment part to run: 1 or 2. */
#define PART 2

#if PART == 1
void part1_run(void);
void part1_systick_handler(void);
#elif PART == 2
void part2_run(void);
void part2_systick_handler(void);
#else
#error "PART must be 1 or 2"
#endif

void SysTick_Handler(void)
{
#if PART == 1
    part1_systick_handler();
#else
    part2_systick_handler();
#endif
}

int main(void)
{
#if PART == 1
    part1_run();
#else
    part2_run();
#endif

    for (;;) { }
}

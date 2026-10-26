#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include <stdint.h>

static volatile uint32_t *const RCC_AHB1ENR = (volatile uint32_t *)0x40023830UL;
static volatile uint32_t *const RCC_APB2ENR = (volatile uint32_t *)0x40023844UL;
static volatile uint32_t *const GPIOD_MODER = (volatile uint32_t *)0x40020C00UL;
static volatile uint32_t *const GPIOD_OTYPER = (volatile uint32_t *)0x40020C04UL;
static volatile uint32_t *const GPIOD_OSPEEDR = (volatile uint32_t *)0x40020C08UL;
static volatile uint32_t *const GPIOD_PUPDR = (volatile uint32_t *)0x40020C0CUL;
static volatile uint32_t *const GPIOD_BSRR = (volatile uint32_t *)0x40020C18UL;
static volatile uint32_t *const GPIOC_MODER = (volatile uint32_t *)0x40020800UL;
static volatile uint32_t *const GPIOC_PUPDR = (volatile uint32_t *)0x4002080CUL;
static volatile uint32_t *const GPIOC_IDR = (volatile uint32_t *)0x40020810UL;
static volatile uint32_t *const GPIOC_AFRL = (volatile uint32_t *)0x40020820UL;
static volatile uint32_t *const SYSCFG_EXTICR4 = (volatile uint32_t *)0x40013814UL;
static volatile uint32_t *const EXTI_IMR = (volatile uint32_t *)0x40013C00UL;
static volatile uint32_t *const EXTI_RTSR = (volatile uint32_t *)0x40013C08UL;
static volatile uint32_t *const EXTI_FTSR = (volatile uint32_t *)0x40013C0CUL;
static volatile uint32_t *const EXTI_PR = (volatile uint32_t *)0x40013C14UL;
static volatile uint32_t *const NVIC_ISER1 = (volatile uint32_t *)0xE000E104UL;
static volatile uint32_t *const NVIC_ICER1 = (volatile uint32_t *)0xE000E184UL;
static volatile uint32_t *const USART6_SR = (volatile uint32_t *)0x40011400UL;
static volatile uint32_t *const USART6_DR = (volatile uint32_t *)0x40011404UL;
static volatile uint32_t *const USART6_BRR = (volatile uint32_t *)0x40011408UL;
static volatile uint32_t *const USART6_CR1 = (volatile uint32_t *)0x4001140CUL;
static volatile uint32_t *const USART6_CR2 = (volatile uint32_t *)0x40011410UL;
static volatile uint32_t *const USART6_CR3 = (volatile uint32_t *)0x40011414UL;
static volatile uint32_t *const SYST_CSR = (volatile uint32_t *)0xE000E010UL;
static volatile uint32_t *const SYST_RVR = (volatile uint32_t *)0xE000E014UL;
static volatile uint32_t *const SYST_CVR = (volatile uint32_t *)0xE000E018UL;

enum {
    GREEN_PIN = 13,
    YELLOW_PIN = 14,
    RED_PIN = 15,
    BUTTON_PIN = 15,
    GREEN = 1U << GREEN_PIN,
    YELLOW = 1U << YELLOW_PIN,
    RED = 1U << RED_PIN,
    LED_MASK = GREEN | YELLOW | RED,
    CORE_CLOCK_HZ = 16000000
};

extern volatile uint32_t system_millis;

void part1_run(void);
void part2_run(void);

#endif

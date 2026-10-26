"""Демонстрация GPIO: вариант 2 лабораторной — гирлянда и кнопка PC15."""
from pathlib import Path
from stm32_gpio import BSRR, IDR, MODER, OTYPER, OSPEEDR, PUPDR, Simulator

LED_PINS = (13, 14, 15)  # green, yellow, red (плата SDK-1.1M)
LED_MASK = sum(1 << pin for pin in LED_PINS)
BUTTON = 15
MODES = (("g", "r", "y", "n"), ("g", "y", "r", "n"), ("r", "n", "y", "n", "g", "n"), ("g", "g", "n", "y", "n", "r", "n"))
PERIODS = (200, 500, 1000, 350)
STATE_PIN = {"g": 13, "y": 14, "r": 15}


def configure(sim: Simulator) -> None:
    c, d = sim.port("C"), sim.port("D")
    # PC15: input + pull-up, therefore unpressed button reads 1.
    c.write(MODER, c.read(MODER) & ~(3 << 30))
    c.write(PUPDR, c.read(PUPDR) | (1 << 30))
    # PD13..15: push-pull outputs, low speed, without pull resistors.
    for pin in LED_PINS:
        d.write(MODER, (d.read(MODER) & ~(3 << (2 * pin))) | (1 << (2 * pin)))
        d.write(OTYPER, d.read(OTYPER) & ~(1 << pin))
        d.write(OSPEEDR, d.read(OSPEEDR) & ~(3 << (2 * pin)))
        d.write(PUPDR, d.read(PUPDR) & ~(3 << (2 * pin)))
    d.write(BSRR, LED_MASK << 16)


def show(sim: Simulator, state: str) -> None:
    d = sim.port("D")
    d.write(BSRR, LED_MASK << 16)  # atomic reset of all LEDs
    if state != "n": d.write(BSRR, 1 << STATE_PIN[state])  # atomic set of one LED
    sim.lines.append(f"{sim.time_ms:06d} ms APP: state={state}; LEDs physical=0x{d.physical_levels() & LED_MASK:04X}")


def run() -> Simulator:
    sim = Simulator(); configure(sim)
    # Presses are active-low, scheduled independently from application polling.
    sim.schedule_input(750, "C", BUTTON, 0); sim.schedule_input(810, "C", BUTTON, 1)
    sim.schedule_input(1700, "C", BUTTON, 0); sim.schedule_input(1760, "C", BUTTON, 1)
    mode = index = 0; changed_at = 0; previous_pressed = False
    show(sim, MODES[mode][index])
    for now in range(0, 2601, 10):
        sim.advance_to(now)
        pressed = not bool(sim.port("C").read(IDR) & (1 << BUTTON))
        if pressed and not previous_pressed:
            mode = (mode + 1) % len(MODES); index = 0; changed_at = now
            sim.lines.append(f"{now:06d} ms APP: PC15 pressed -> mode {mode + 1}")
            show(sim, MODES[mode][index])
        previous_pressed = pressed
        if now - changed_at >= PERIODS[mode]:
            changed_at += PERIODS[mode]; index = (index + 1) % len(MODES[mode])
            show(sim, MODES[mode][index])
    return sim


if __name__ == "__main__":
    simulator = run()
    destination = Path(__file__).with_name("garland.log")
    simulator.save_log(destination)
    print(f"Simulation completed: {destination}")
    print("Final GPIOC.IDR=0x%04X, GPIOD.ODR=0x%04X" % (simulator.port("C").read(IDR), simulator.port("D").read(0x14)))

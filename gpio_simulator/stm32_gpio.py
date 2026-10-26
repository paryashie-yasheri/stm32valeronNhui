"""Регистровая модель GPIO STM32F427 (RM0090, раздел 8.4).

Модель намеренно не содержит EXTI и RCC: тактирование считается поданным
снаружи, как и указано в задании. Время измеряется в миллисекундах и задаётся
классом Simulator.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Optional

PIN_MASK = 0xFFFF

MODER, OTYPER, OSPEEDR, PUPDR, IDR, ODR, BSRR, LCKR, AFRL, AFRH = (
    0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24
)
REGISTER_NAMES = {
    MODER: "MODER", OTYPER: "OTYPER", OSPEEDR: "OSPEEDR", PUPDR: "PUPDR",
    IDR: "IDR", ODR: "ODR", BSRR: "BSRR", LCKR: "LCKR", AFRL: "AFRL", AFRH: "AFRH",
}


class GPIOError(ValueError):
    """Неверный адрес или недопустимая операция с GPIO."""


@dataclass
class GPIOPort:
    """Один 16-разрядный порт GPIOx.

    Внешний уровень ``None`` означает высокоимпедансную внешнюю цепь. Он
    разрешается внутренней подтяжкой. Режимы MODER: 00 input, 01 output,
    10 alternate function, 11 analog.
    """

    name: str
    now: Callable[[], int]
    log: Callable[[str], None]
    moder: int = 0
    otyper: int = 0
    ospeedr: int = 0
    pupdr: int = 0
    odr: int = 0
    lckr: int = 0
    afrl: int = 0
    afrh: int = 0
    _lock_phase: int = 0
    external: list[Optional[int]] = field(default_factory=lambda: [None] * 16)
    alternate_output: int = 0

    def _emit(self, message: str) -> None:
        self.log(f"{self.now():06d} ms {self.name}: {message}")

    @staticmethod
    def _field(value: int, pin: int) -> int:
        return (value >> (2 * pin)) & 0b11

    def _locked_mask(self) -> int:
        return self.lckr & PIN_MASK if (self.lckr & (1 << 16)) else 0

    def _write_2bit(self, old: int, value: int) -> int:
        """Не меняет поля конфигурации закреплённых LCKR выводов."""
        locked = self._locked_mask()
        result = old
        for pin in range(16):
            if not (locked & (1 << pin)):
                result = (result & ~(3 << (2 * pin))) | (value & (3 << (2 * pin)))
        return result & 0xFFFFFFFF

    def _write_af(self, old: int, value: int, first_pin: int) -> int:
        """AFRL/AFRH: по 4 бита AFy на контакт, c учётом LCKR."""
        result = old
        locked = self._locked_mask()
        for local_pin in range(8):
            if not (locked & (1 << (first_pin + local_pin))):
                shift = 4 * local_pin
                result = (result & ~(0xF << shift)) | (value & (0xF << shift))
        return result & 0xFFFFFFFF

    def _line_level(self, pin: int) -> int:
        mode = self._field(self.moder, pin)
        ext = self.external[pin]
        # Output driver is connected in output and alternate-function modes.
        if mode in (1, 2):
            driven = (self.odr if mode == 1 else self.alternate_output) >> pin & 1
            open_drain = (self.otyper >> pin) & 1
            if not open_drain or driven == 0:
                return driven
            # Open drain releases a logical '1'; the line is determined outside.
        if ext is not None:
            return ext
        pull = self._field(self.pupdr, pin)
        return 1 if pull == 1 else 0  # pull-up; pull-down/floating -> 0 in model

    def _idr(self) -> int:
        return sum(self._line_level(pin) << pin for pin in range(16))

    def read(self, offset: int) -> int:
        if offset == MODER: value = self.moder
        elif offset == OTYPER: value = self.otyper
        elif offset == OSPEEDR: value = self.ospeedr
        elif offset == PUPDR: value = self.pupdr
        elif offset == IDR: value = self._idr()
        elif offset == ODR: value = self.odr
        elif offset == BSRR: value = 0  # write-only
        elif offset == LCKR:
            # First and second reads complete the LCKK write/read sequence.
            if self._lock_phase == 3:
                self._lock_phase = 4
            elif self._lock_phase == 4:
                self.lckr |= 1 << 16
                self._lock_phase = 5
            value = self.lckr
        elif offset == AFRL: value = self.afrl
        elif offset == AFRH: value = self.afrh
        else: raise GPIOError(f"{self.name}: неизвестное смещение 0x{offset:X}")
        self._emit(f"READ  {REGISTER_NAMES[offset]:5s} -> 0x{value:08X}")
        return value

    def write(self, offset: int, value: int) -> None:
        value &= 0xFFFFFFFF
        if offset == MODER: self.moder = self._write_2bit(self.moder, value)
        elif offset == OTYPER:
            lock = self._locked_mask(); self.otyper = (self.otyper & lock) | (value & ~lock & PIN_MASK)
        elif offset == OSPEEDR: self.ospeedr = self._write_2bit(self.ospeedr, value)
        elif offset == PUPDR: self.pupdr = self._write_2bit(self.pupdr, value)
        elif offset == ODR:
            lock = self._locked_mask(); self.odr = (self.odr & lock) | (value & ~lock & PIN_MASK)
        elif offset == BSRR:
            # BSy has priority if set and reset are written for same pin.
            set_bits, reset_bits = value & PIN_MASK, (value >> 16) & PIN_MASK
            self.odr = ((self.odr & ~reset_bits) | set_bits) & PIN_MASK
        elif offset == LCKR:
            # RM0090: LCKK=1, LCKK=0, LCKK=1, read, read. Until then no lock.
            if self._lock_phase == 5:
                self._emit(f"WRITE LCKR  ignored (locked); ODR=0x{self.odr:04X}; IDR=0x{self._idr():04X}")
                return
            candidates = value & PIN_MASK
            lckk = bool(value & (1 << 16))
            if self._lock_phase == 0 and lckk:
                self.lckr = candidates; self._lock_phase = 1
            elif self._lock_phase == 1 and not lckk:
                self._lock_phase = 2
            elif self._lock_phase == 2 and lckk and candidates == (self.lckr & PIN_MASK):
                self._lock_phase = 3
            # A new complete sequence is the only way to change the selected pins.
        elif offset == AFRL: self.afrl = self._write_af(self.afrl, value, 0)
        elif offset == AFRH: self.afrh = self._write_af(self.afrh, value, 8)
        elif offset in (IDR,): raise GPIOError(f"{self.name}: {REGISTER_NAMES[offset]} доступен только для чтения")
        else: raise GPIOError(f"{self.name}: неизвестное смещение 0x{offset:X}")
        self._emit(f"WRITE {REGISTER_NAMES[offset]:5s} <- 0x{value:08X}; ODR=0x{self.odr:04X}; IDR=0x{self._idr():04X}")

    def set_input(self, pin: int, level: Optional[int]) -> None:
        if not 0 <= pin < 16 or level not in (None, 0, 1):
            raise GPIOError("pin должен быть 0..15, level — 0, 1 или None")
        self.external[pin] = level
        shown = "Z" if level is None else str(level)
        self._emit(f"PIN   P{self.name[-1]}{pin} external <- {shown}; IDR=0x{self._idr():04X}")

    def physical_levels(self) -> int:
        """Фактические уровни на 16 физических контактах."""
        return self._idr()


class Simulator:
    """Диспетчер модельного времени, портов, внешних событий и журнала."""
    def __init__(self) -> None:
        self.time_ms = 0
        self.lines: list[str] = []
        self.ports = {name: GPIOPort(name, lambda: self.time_ms, self.lines.append)
                      for name in "ABCDEFGHIJK"}
        self._events: list[tuple[int, str, int, Optional[int]]] = []

    def port(self, name: str) -> GPIOPort:
        try: return self.ports[name.upper()]
        except KeyError as exc: raise GPIOError(f"Нет порта GPIO{name}") from exc

    def schedule_input(self, at_ms: int, port: str, pin: int, level: Optional[int]) -> None:
        if at_ms < self.time_ms: raise GPIOError("Нельзя назначить событие в прошлом")
        self._events.append((at_ms, port.upper(), pin, level))
        self._events.sort(key=lambda event: event[0])

    def advance_to(self, target_ms: int) -> None:
        if target_ms < self.time_ms: raise GPIOError("Время не может идти назад")
        while self._events and self._events[0][0] <= target_ms:
            at, port, pin, level = self._events.pop(0)
            self.time_ms = at
            self.port(port).set_input(pin, level)
        self.time_ms = target_ms

    def save_log(self, filename: str | Path) -> None:
        Path(filename).write_text("\n".join(self.lines) + "\n", encoding="utf-8")

import unittest
from stm32_gpio import BSRR, IDR, MODER, ODR, OTYPER, PUPDR, Simulator


class GPIOTests(unittest.TestCase):
    def setUp(self): self.sim = Simulator(); self.p = self.sim.port("A")

    def test_bsrr_sets_and_resets_odr(self):
        self.p.write(BSRR, (1 << 2) | (1 << (16 + 5)))
        self.assertEqual(self.p.read(ODR), 1 << 2)
        self.p.write(BSRR, (1 << 2) | (1 << 18))
        self.assertTrue(self.p.read(ODR) & (1 << 2))  # BS2 wins over BR2

    def test_pull_up_is_read_when_input_floats(self):
        self.p.write(PUPDR, 1 << (2 * 3))
        self.assertTrue(self.p.read(IDR) & (1 << 3))

    def test_external_input_overrides_pull(self):
        self.p.write(PUPDR, 1 << 8)
        self.p.set_input(4, 0)
        self.assertFalse(self.p.read(IDR) & (1 << 4))

    def test_push_pull_output_is_visible_at_idr(self):
        self.p.write(MODER, 1 << (2 * 7))
        self.p.write(BSRR, 1 << 7)
        self.assertTrue(self.p.read(IDR) & (1 << 7))

    def test_open_drain_high_releases_line(self):
        self.p.write(MODER, 1 << 2)
        self.p.write(OTYPER, 1 << 1)
        self.p.write(BSRR, 1 << 1)
        self.p.set_input(1, 0)
        self.assertFalse(self.p.read(IDR) & 2)

    def test_lock_sequence_protects_configuration(self):
        pin = 6; mask = 1 << pin
        self.p.write(MODER, 1 << (2 * pin))
        self.p.write(0x1C, mask | (1 << 16))
        self.p.write(0x1C, mask)
        self.p.write(0x1C, mask | (1 << 16))
        self.p.read(0x1C); self.p.read(0x1C)
        self.p.write(MODER, 0)
        self.assertEqual((self.p.read(MODER) >> (2 * pin)) & 3, 1)


if __name__ == "__main__": unittest.main()

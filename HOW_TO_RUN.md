# HOW_TO_RUN

## Requirements

- STM32CubeIDE installed.
- SDK-1.1M with the STM32F427VITx processor module.
- USB cable connected to the board's **DBG USB** connector.

Select the exercise with `assignment_part` in `lab1/Src/main.c`: `1` or `2` (the default).

- Part 1 runs four LED sequences. Press the PC15 button to change sequences.
- Part 2 adds serial commands and four custom sequence slots. Connect to USART6 (PC6 TX, PC7 RX) at 38400 baud, 8 data bits, no parity, one stop bit, and no flow control.

For part 2, send `set 1` through `set 8` to select a saved sequence. Send `new gryn`, then `fast`, `medium`, or `slow` when prompted. The letters mean green, red, yellow, and off; sequences accept 2–8 letters. Custom sequences occupy slots 5–8 and then wrap around. Use `set interrupts on` or `set interrupts off` to choose interrupt or polling button handling.

Read `main.c` first for startup, then `part1.c` or `part2.c` for the application. `lab1/Inc/board.h` holds hardware addresses and pin constants. A register pointer refers to a hardware control value; reading or writing through it accesses the peripheral. `volatile` makes those accesses happen even when the compiler optimizes the code. `SysTick_Handler` runs every millisecond and increments `system_millis`, allowing the main loop to measure time without stopping to wait.

## Open and build

1. Start STM32CubeIDE and select a workspace.
2. If `lab1` is not already in the Project Explorer, choose **File → Import → General → Existing Projects into Workspace** and select this repository's `lab1` directory.
3. Select `lab1`, then choose **Project → Build Project**. The ELF file is created at `lab1/Debug/lab1.elf`.

## Flash and run

1. Open **Run → Debug Configurations…** and create an **STM32 Cortex-M C/C++ Application** configuration for `lab1`.
2. On the **Debugger** tab, select **ST-LINK (OpenOCD)** and set **Configuration Script** to **User Defined**.
3. Browse to `lab1/SDK1_1_M FTDBG.cfg` and apply the configuration.
4. Click **Debug**. When execution stops at `main`, press **Resume (F8)**. The selected LED sequence should start.

The OpenOCD files `lab1/SDK1_1_M FTDBG.cfg` and `lab1/SDK11M_FT.cfg` configure the SDK-1.1M's built-in FTDI JTAG debugger for STM32F427.

## If the debugger cannot connect

- Check that the cable is plugged into **DBG USB**, not the processor module's USB connector.
- On Linux, grant the logged-in user access to the debugger (`0403:6010`) with a udev rule, then reload udev rules and reconnect the board. The handbook in `README.md`, section 4.2, has the rule and setup steps.
- Confirm the debug configuration uses the SDK-specific OpenOCD script above.

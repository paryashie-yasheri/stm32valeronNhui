# HOW_TO_RUN

## Requirements

- STM32CubeIDE installed.
- SDK-1.1M with the STM32F427VITx processor module.
- USB cable connected to the board's **DBG USB** connector.

The current program blinks the green LED on PD13 every 500 ms.

## Open and build

1. Start STM32CubeIDE and select a workspace.
2. If `lab1` is not already in the Project Explorer, choose **File → Import → General → Existing Projects into Workspace** and select this repository's `lab1` directory.
3. Select `lab1`, then choose **Project → Build Project**. The ELF file is created at `lab1/Debug/lab1.elf`.

## Flash and run

1. Open **Run → Debug Configurations…** and create an **STM32 Cortex-M C/C++ Application** configuration for `lab1`.
2. On the **Debugger** tab, select **ST-LINK (OpenOCD)** and set **Configuration Script** to **User Defined**.
3. Browse to `lab1/SDK1_1_M FTDBG.cfg` and apply the configuration.
4. Click **Debug**. When execution stops at `main`, press **Resume (F8)**. The green LED should blink.

The OpenOCD files `lab1/SDK1_1_M FTDBG.cfg` and `lab1/SDK11M_FT.cfg` configure the SDK-1.1M's built-in FTDI JTAG debugger for STM32F427.

## If the debugger cannot connect

- Check that the cable is plugged into **DBG USB**, not the processor module's USB connector.
- On Linux, grant the logged-in user access to the debugger (`0403:6010`) with a udev rule, then reload udev rules and reconnect the board. The handbook in `README.md`, section 4.2, has the rule and setup steps.
- Confirm the debug configuration uses the SDK-specific OpenOCD script above.

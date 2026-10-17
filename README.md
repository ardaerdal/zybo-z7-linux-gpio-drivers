# Zybo Z7 Embedded Linux GPIO Drivers

This repository contains a series of small Embedded Linux projects developed for the **Digilent Zybo Z7** platform.

The project was created as part of my process of learning Embedded Linux, Linux kernel development, memory-mapped I/O, Device Tree-based hardware access, and Linux platform drivers.

The modules are intentionally organized as a progression. They are not intended to be perfect, production-ready Linux drivers. Each module represents a step in my learning process and introduces additional concepts compared to the previous one.

## Project Overview

The project starts with direct hardware access from userspace and gradually moves toward custom Linux kernel platform drivers.

The main hardware components used throughout the project are:

- Zybo Z7 board
- Xilinx Zynq-7000
- AXI GPIO peripherals
- Switches
- Push buttons
- LEDs

The software side mainly involves:

- C
- Embedded Linux
- Linux kernel modules
- Linux platform drivers
- Device Tree
- Memory-mapped I/O
- UIO
- PetaLinux
- AXI GPIO

## Module 1 — Userspace UIO Access

The first module controls an LED from a switch using a userspace application.

The AXI GPIO peripherals are exposed through `/dev/uio` devices and mapped into the application's virtual address space using `mmap()`.

The program:

- Maps the switch and LED AXI GPIO registers
- Configures SW0 as an input
- Configures LD0 as an output
- Reads the state of SW0
- Updates LD0 according to the switch state
- Detects a rising edge on SW0

This module was mainly used to understand memory-mapped hardware access from Linux userspace before moving the same concepts into kernel space.

## Module 2 — Basic LED Platform Driver

The second module moves hardware access into the Linux kernel.

It introduces a basic Linux platform driver that controls LD0 through an AXI GPIO peripheral.

The module introduces concepts such as:

- Platform drivers
- `probe()` and `remove()`
- Device Tree matching
- Managed resource mapping
- `readl()` and `writel()`
- AXI GPIO DATA and TRI registers

The driver obtains the AXI GPIO memory resource from the Device Tree and configures LD0 as an output.

## Module 3 — Switch and LED Platform Driver

The third module expands the platform driver to work with two AXI GPIO resources:

- Switch GPIO
- LED GPIO

The driver maps both resources from the Device Tree, configures SW0 as an input and LD0 as an output, and transfers the current switch state to the LED.

At this stage, the switch is read when the driver is probed. Continuous monitoring is introduced in the following module.

## Module 4 — Periodic Switch Polling

The fourth module introduces continuous switch monitoring.

Instead of reading SW0 only during `probe()`, the driver uses Linux kernel delayed work to periodically poll the switch state.

The main concepts introduced in this module are:

- `struct delayed_work`
- `INIT_DELAYED_WORK()`
- `schedule_delayed_work()`
- `container_of()`
- Periodic GPIO polling
- Tracking previous hardware state
- Safe cancellation of delayed work during driver removal

SW0 is polled every 50 ms and LD0 is updated when the switch state changes.

## Module 5 — Multiple Switches, Buttons, and LEDs

The final module extends the polling driver to support multiple GPIO inputs and outputs.

It works with:

- SW0–SW3
- BTN0–BTN3
- LD0–LD3

The switch inputs are read from the first AXI GPIO channel, while the buttons are read from the second channel.

The LED state is determined from the switch and button states, and bit masks are used to modify only the relevant GPIO bits while preserving unrelated outputs.

This module combines the concepts introduced in the previous stages into a larger Embedded Linux GPIO driver.

## Learning Progression

The repository roughly follows this progression:

```text
Userspace UIO + mmap()
        |
        v
Basic Linux Platform Driver
        |
        v
Multiple AXI GPIO Resources
        |
        v
Kernel Delayed-Work Polling
        |
        v
Multiple Switches, Buttons, and LEDs
```

The purpose of keeping the individual modules is to show this progression rather than only presenting the final implementation.

## Notes

This repository documents a learning project.

The code was written while learning Embedded Linux and Linux device-driver development, so some implementations may be simpler than what would normally be used in a production kernel driver.

For example, later modules use periodic polling to monitor GPIO inputs. A production implementation may use different mechanisms depending on the hardware and system requirements.

The goal of the repository is therefore not to provide a production-ready driver, but to document the development process and the concepts learned along the way.

## License

This project is licensed under the GNU General Public License v2.0 only.

See the `LICENSE` file for details.

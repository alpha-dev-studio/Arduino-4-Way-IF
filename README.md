# FourWayIf – 4-Way Interface for BLHeli_S ESCs

A BLHeli **4-way interface (4w-if)** for **Arduino Uno / Nano** and **Raspberry Pi Pico / Pico W**.
Connected to a PC over USB and to an ESC over its normal servo/signal wire, it lets 4-way-interface host software
(for example a browser-based configurator using Web Serial) read, write and flash a **BLHeli_S** ESC without a flight
controller in between.

*IF YOU ARE JUST HERE TO KNOW THE PINOUTS AND FLASHING THE CODE, GO TO YOUR BOARD:*
*ARDUINO UNO / NANO → [QUICK START GUIDE](#quick-start) AND [PINOUT](#hardware). RASPBERRY PI PICO → [QUICK START GUIDE](#pico-quick-start) AND [PINOUT](#pico-hardware).*

```
  PC / browser                    Uno / Nano  or  Pico                         BLHeli_S ESC
 ┌───────────────────┐ USB serial ┌──────────────────┐  signal wire + GND     ┌──────────────┐
 │  4w-if frames     │◄──────────►│  FourWayIf       │◄──────────────────────►│  bootloader  │
 │                   │            │  4w-if ⇄ BL cmds │  19200 baud, 8N1       │  (BLHeli_S)  │
 └───────────────────┘            └──────────────────┘                        └──────────────┘
```

| | Arduino Uno / Nano | Raspberry Pi Pico / Pico W |
|---|---|---|
| Folder | `uno_4way_IF` | `pico_4way_IF` |
| Signal-wire UART | bit-banged, interrupts off | two PIO state machines (transmit + receive) |
| Default signal pin | D11 | GP2 (any GPIO 0 to 28) |
| PC link | USB-serial chip, 38400 baud | native USB serial (baud rate ignored) |
| Logic level | 5 V | 3.3 V |
| Debug mode | yes | no |

## Table of contents

**[Part 1 – Arduino Uno / Nano](#part-1--arduino-uno--nano)**

1. [Features and limits](#features-and-limits)
2. [Hardware](#hardware)
3. [Quick start](#quick-start)
4. [How it works](#how-it-works)
5. [Bit-banged UART and timing](#bit-banged-uart-and-timing)
6. [Source file and function reference](#source-file-and-function-reference)
7. [Configuration](#configuration)
8. [Debug mode](#debug-mode)
9. [Troubleshooting](#troubleshooting)
10. [Known limitations](#known-limitations)

**[Part 2 – Raspberry Pi Pico / Pico W](#part-2--raspberry-pi-pico--pico-w)**

1. [Pico: Features and limits](#pico-features-and-limits)
2. [Pico: Hardware](#pico-hardware)
3. [Pico: Quick start](#pico-quick-start)
4. [Pico: How it works](#pico-how-it-works)
5. [Pico: PIO UART and timing](#pico-pio-uart-and-timing)
6. [Pico: Source file and function reference](#pico-source-file-and-function-reference)
7. [Pico: Configuration](#pico-configuration)
8. [Pico: Troubleshooting](#pico-troubleshooting)
9. [Pico: Known limitations](#pico-known-limitations)

**[Protocol reference (both boards)](#protocol-reference-both-boards)**

* [PC link: the 4w-if protocol](#pc-link-the-4w-if-protocol)
* [ESC link: the BLHeli_S bootloader protocol](#esc-link-the-blheli_s-bootloader-protocol)
* [Command translation table](#command-translation-table)
* [Byte-level examples](#byte-level-examples)

**[Credits and sources](#credits-and-sources)** · **[License note](#license-note)**

---

## Part 1 – Arduino Uno / Nano

An Arduino sketch that turns an **Arduino Uno or Nano** (ATmega328P, 16 MHz) into a 4-way interface. The signal wire is
driven by bit-banging on D11.

```
  PC / browser                       Arduino Uno / Nano                       BLHeli_S ESC
 ┌───────────────────┐  USB serial  ┌──────────────────┐  D11 ── signal wire  ┌──────────────┐
 │  4w-if frames     │◄────────────►│  FourWayIf       │◄────────────────────►│  bootloader  │
 │  38400 baud, 8N1  │              │  4w-if ⇄ BL cmds │  19200 baud, 8N1     │  (BLHeli_S)  │
 └───────────────────┘              └──────────────────┘  bit-banged + GND    └──────────────┘
```

### Features and limits

| Item | Value |
|---|---|
| Boards | Arduino Uno, Arduino Nano (ATmega328P @ 16 MHz) |
| PC link | 38400 baud, 8N1, 4w-if framing, CRC-16/XMODEM |
| ESC link | 19200 baud, 8N1, bit-banged on **D11**, CRC-16 (poly 0xA001), not inverted |
| Target ESCs | BLHeli_S (SiLabs EFM8 bootloader, banner `471x`) |
| ESC channels | One (channel 0 on D11). Other channel numbers return `ACK_I_INVALID_CHANNEL` |
| Operations | Handshake, flash read, flash write, page erase, EEPROM read/write, reset |
| Not supported | Erase-all, verify, C2 interface, Atmel and ARM bootloaders, BLHeli_32, multiple ESCs |
| Footprint | about 9.5 KB flash and about 0.5 KB RAM including the Arduino core |
| Serial library | Used only for the PC link. The ESC link never uses `Serial` |

### Hardware

#### Wiring

| Arduino | ESC |
|---|---|
| D11 | Signal (servo) wire |
| GND | Signal ground |

* **Do not power the ESC from the Arduino.** Give the ESC its own supply (battery, or a bench supply with current limiting).
* Connect the grounds. Without a common ground the signal wire has no reference.
* The signal wire must idle HIGH when the ESC powers up so its bootloader is listening. When idle, the Arduino holds D11
  as an input with the internal pull-up enabled.
* D11 is `PB3` on the ATmega328P.

#### Voltage levels

The Uno/Nano drives the signal wire at 5 V, while the EFM8 MCU on a BLHeli_S ESC runs at 3.3 V. Most ESC signal inputs
are designed to accept 5 V from a flight controller or receiver, but this is ESC-specific, so check your ESC if unsure.
A 3.3 V board avoids the question. In the other direction, the ATmega328P reads a signal above about 3 V (0.6 × Vcc) as
HIGH, so a clean 3.3 V reply works but with little margin; keep the wire short.

### Quick start

#### Flash the Arduino

1. Open the `uno_4way_IF` folder and open `uno_4way_IF.ino` in the Arduino IDE.
2. Select **Arduino Uno** or **Arduino Nano** (and the correct processor/bootloader variant for your Nano) and the port, then upload.

No libraries are needed beyond the standard Arduino core.

#### Use it

1. Wire the Arduino to the ESC as above and power the ESC separately.
2. Open your 4-way-interface host software (a Web Serial browser such as Chrome or Edge is required for browser-based tools).
3. Select the Arduino's serial port at 38400 baud and connect.
4. Read the ESC settings, change them, or flash firmware.

Always read and save the original settings before flashing, and do not remove ESC power or the USB cable during a write.

### How it works

```
loop()
 └─ Serial.read() ─► parseMessage()  ── state machine: 0x2F cmd addrH addrL len params crcH crcL
        │                               PARSE_OK ─────────► blheli_handle_command()
        │                               PARSE_BAD_CRC ────► fourway_send_response(ACK_I_INVALID_CRC)
        ▼
 blheli_handle_command()
        ├─ interface commands (alive / version / name / set mode / exit) answered locally
        └─ device commands ─► blheli_enter_bootloader / blheli_read / blheli_write / blheli_page_erase / blheli_run
                                  │   cli(); bit-bang D11; restore SREG
                                  ▼
                          fourway_send_response()  ── 0x2E cmd addrH addrL len params ack crcH crcL
```

* **Request/response.** The PC sends one frame and waits for the answer. The Arduino processes one command at a time.
* **Interrupts off during ESC traffic.** Each bootloader transaction runs with interrupts disabled so the bit timing is
  exact. The PC is silent while it waits for the reply, so no serial data is lost. `millis()` does not advance in that
  window, so timeouts there are counted in polling loops.
* **Shared buffer.** One 256-byte buffer carries request parameters in and response data out.
* **Frame timeout.** A partially received PC frame is discarded after 100 ms (`FOURWAY_RX_TIMEOUT_MS`).

### Bit-banged UART and timing

At 19200 baud one bit is **52.08 µs**. The bootloader link is generated and sampled in software on D11 with interrupts
disabled.

#### Transmit (`servo_send_byte`, `servo_send_byte_nostop`)

```
idle HIGH │‾‾‾‾‾‾‾│_____│ d0 │ d1 │ ... │ d7 │‾‾‾‾‾‾‾│
                   start   data bits LSB first   stop
```

* Start bit LOW, 8 data bits LSB first, stop bit HIGH. **Not inverted.**
* `servo_send_byte_nostop` returns right after raising the stop bit. It is used for the **last byte of a message**, so the
  line is released while the ESC is still sampling the stop bit and the start of the ESC's reply is not missed.
* Every command starts with **one idle HIGH bit**. After an ESC reply, the ESC is still finishing its stop bit and
  switching back to receive. Starting a start bit immediately would collide with it, so the Arduino waits one bit first.

#### Receive (`servo_recv_byte`)

1. Poll D11 until it goes LOW (start bit) or the timeout expires.
2. Wait `ESC_RX_FIRST_US` (77 µs) to land in the middle of data bit 0.
3. Sample 8 bits, `ESC_RX_BIT_US` apart, shifting in LSB first.
4. Check that the stop bit is HIGH. Otherwise return `-2` (framing error).

Return values: `0..255` byte, `-1` timeout, `-2` framing error.

#### Cycle accuracy

The delays in `FourWayIf.h` are shortened to compensate for loop overhead at 16 MHz:

| Constant | Value | Purpose |
|---|---|---|
| `ESC_TX_BIT_US` | 51.5 µs | each data bit in the transmit loop |
| `ESC_TX_EDGE_US` | 51.8 µs | start, stop and idle bits |
| `ESC_RX_FIRST_US` | 77.0 µs | start edge to centre of data bit 0 |
| `ESC_RX_BIT_US` | 51.4 µs | between samples |

The compiled transmit and receive loops come to about 835 cycles per bit (52.2 µs, roughly 0.2 % above ideal). That
figure comes from reading the AVR disassembly, not from measuring the pin. If an ESC does not answer, check D11 with a
logic analyser and adjust these four constants.

### Source file and function reference

#### Files

| File | Role |
|---|---|
| `FourWayIf.h` | pin selection, timing constants, command/ACK codes, declarations |
| `FourWayIf.cpp` | all implementation, including `setup()` and `loop()` |
| `uno_4way_IF.ino` | minimal sketch file that includes the header |

#### Functions (`FourWayIf.cpp`)

| Function | Purpose |
|---|---|
| `crc16_xmodem_update(crc, byte)` | CRC-16/XMODEM, PC link |
| `crc16_blheli_update(crc, byte)` | CRC-16 poly 0xA001, ESC link |
| `crc_append_byte(crc)` | utility: byte that makes the CRC low byte 0 (unused by the bridge) |
| `esc_delay_us(us)` | plain microsecond delay for non-critical code |
| `servo_send_byte(b)` | transmit one byte with a full stop bit |
| `servo_send_byte_nostop(b)` | transmit one byte, return right after the stop bit goes HIGH |
| `servo_recv_byte(timeout_ms)` | receive one byte, `-1` timeout, `-2` framing error |
| `blheli_send_command(cmd, len)` | idle bit, command bytes, CRC, release the line |
| `blheli_enter_bootloader(info)` | handshake, fills `blheli_info_t` |
| `blheli_page_erase(addr)` | erase one page |
| `blheli_read(blCmd, addr, buf, len)` | read flash or EEPROM |
| `blheli_write(blCmd, addr, buf, len, ackMs)` | write flash or EEPROM |
| `blheli_run()` | reset the ESC |
| `blheli_get_diag(d8)` | copy the 8 debug bytes of the last read |
| `parseMessage(byte)` | PC frame state machine |
| `fourway_send_response(...)` | build and send a PC response frame |
| `blheli_handle_command(...)` | execute one validated PC command |
| `setup()` / `loop()` | Arduino entry points |

Internal (static) helpers: `servo_pin_input/output/high/low/read` (direct port access for timing), `bl_set_address`,
`bl_get_ack`, `bl_read_buf`.

### Configuration

Edit `FourWayIf.h`:

| Macro | Default | Meaning |
|---|---|---|
| `ESC_PIN_ARDUINO` | 11 | the signal pin (D11) |
| `ESC_DDR`, `ESC_PORT`, `ESC_PINREG`, `ESC_MASK` | PORTB, bit PB3 | port bits behind D11; change together to use another pin |
| `PC_BAUD` | 38400 | PC serial rate |
| `ESC_TX_BIT_US`, `ESC_TX_EDGE_US`, `ESC_RX_FIRST_US`, `ESC_RX_BIT_US` | see above | bit timing |
| `FOURWAY_RX_TIMEOUT_MS` | 100 | discard a stalled PC frame |
| `BL_T_ACK_MS`, `BL_T_NOACK_MS`, `BL_T_BYTE_MS` | 10, 6, 20 | ESC reply timeouts (ms) |
| `BL_T_ERASE_MS`, `BL_T_WRITE_MS`, `BL_T_EEWRITE_MS` | 200, 200, 3000 | erase/write timeouts (ms) |
| `FOURWAY_DEBUG` | 0 | see [Debug mode](#debug-mode) |

To use a different pin, change the four port macros together and keep all pin access on one port register. Delays are
tuned for 16 MHz; other clock speeds need retuning.

### Debug mode

Set `#define FOURWAY_DEBUG 1` in `FourWayIf.h`. When a `DeviceRead` fails, the Arduino then answers with `ACK_OK` and
fills the reply with diagnostic bytes, so the host's log shows them as the reply data.

| Byte | Meaning |
|---|---|
| 0 | `0xDB` marker |
| 1 | stage: 1 `SET_ADDRESS` failed, 2 data timeout/framing, 3 CRC mismatch, 4 bad final ACK, 5 other |
| 2 | reply to `SET_ADDRESS` (`0xFF` = no reply) |
| 3 | number of data bytes received |
| 4 | last receive result: 0 ok, 1 timeout, 2 framing error |
| 5, 6 | computed CRC low, high (both 0 when good) |
| 7 | final ACK byte |
| 8 … | the data bytes that were received |

Settings shown by the host are meaningless in this mode. Set the macro back to 0 for normal use. The debug path adds
about 64 bytes of flash only when enabled.

### Troubleshooting

| Symptom | Likely cause and action |
|---|---|
| Host cannot connect | Wrong port or baud (the sketch uses 38400). Wait about a second after opening the port because the Uno/Nano resets. |
| Handshake (`InitFlash`) fails | ESC not powered, signal wire not connected, no common ground, or the ESC already left its bootloader. Power-cycle the ESC with the signal wire idle HIGH and retry. |
| Handshake works, reads fail | Check bit timing on D11 (see [Bit-banged UART and timing](#bit-banged-uart-and-timing)); enable [Debug mode](#debug-mode) and read the stage byte. |
| Host reports a non-OK reply | The Arduino returned an ACK other than `0x00`: unsupported command, wrong channel, or the ESC did not answer. |
| Works once, fails after a reset | The ESC left the bootloader (for example after `cmd_DeviceReset`). Run the handshake again. |
| Signal level problems | See [Voltage levels](#voltage-levels). |
| Unreliable on other boards | The delays assume 16 MHz. Retune the four timing constants. |

As a last resort if it connects to the interface but does not read the ESC, follow the steps below.

#### Arduino Uno / Nano

Add a **2.7 kΩ** resistor between the GPIO where the signal wire is connected (D11) and **5 V**.

### Known limitations

* One ESC (channel 0 on D11). Hosts that probe other channels receive `ACK_I_INVALID_CHANNEL`.
* SiLabs (BLHeli_S) bootloader only. No Atmel, ARM, BLHeli_32 or C2 programming.
* While a bootloader transaction runs, interrupts are off for up to the transaction length (about 64 ms for a
  112-byte read, up to 3 s while waiting for an EEPROM write acknowledge). `millis()` pauses during that time.

---

## Part 2 – Raspberry Pi Pico / Pico W

The same 4-way interface for the **Raspberry Pi Pico and Pico W (RP2040)**, in the `pico_4way_IF` folder. The protocol layer
is identical to the Arduino version; the signal wire is driven by two PIO state machines instead of bit-banging.

```
  PC / browser                    Raspberry Pi Pico / Pico W                     BLHeli_S ESC
 ┌───────────────────┐  USB serial  ┌──────────────────┐  GP2 ── signal wire  ┌──────────────┐
 │  4w-if frames     │◄────────────►│  FourWayIf       │◄────────────────────►│  bootloader  │
 │  (USB CDC)        │              │  + EscWire (PIO) │  19200 baud, 8N1     │  (BLHeli_S)  │
 └───────────────────┘              └──────────────────┘  PIO UART + GND      └──────────────┘
```

### Pico: Features and limits

| Item | Value |
|---|---|
| Boards | Raspberry Pi Pico, Pico W (RP2040) |
| Arduino core | Arduino-Pico (Earle Philhower), board package "Raspberry Pi Pico/RP2040" |
| PC link | native USB serial, 4w-if framing, CRC-16/XMODEM. The baud rate is ignored (38400 is passed to `Serial.begin()` only) |
| ESC link | 19200 baud, 8N1, PIO half-duplex on one GPIO (default **GP2**), CRC-16 (poly 0xA001), not inverted |
| Target ESCs | BLHeli_S (SiLabs EFM8 bootloader, banner `471x`) |
| ESC channels | One (channel 0 on the signal pin). Other channel numbers return `ACK_I_INVALID_CHANNEL` |
| Operations | Handshake, flash read, flash write, page erase, EEPROM read/write, reset |
| Not supported | Erase-all, verify, C2 interface, Atmel and ARM bootloaders, BLHeli_32, multiple ESCs, debug mode |
| PIO usage | one PIO block: 2 of its 4 state machines and 13 of its 32 instruction words |

### Pico: Hardware

#### Wiring

| Pico | ESC |
|---|---|
| GP2 (physical pin 4) | Signal (servo) wire |
| GND (any GND pin, for example physical pin 3) | Signal ground |

* **Do not power the ESC from the Pico.** Give the ESC its own supply (battery, or a bench supply with current limiting).
* Connect the grounds. Without a common ground the signal wire has no reference.
* The signal wire must idle HIGH when the ESC powers up so its bootloader is listening. When idle, the Pico holds the pin
  as an input with the internal pull-up enabled.
* To use another pin, change `ESC_PIN` in `FourWayIf.h`. Any GPIO from 0 to 28 works (GP17, for example, is physical pin 22).

#### Logic levels

Pico GPIOs are 3.3 V, the same as the MCU on a BLHeli_S ESC, so no level shifting is needed. Pico GPIOs are not 5 V
tolerant, so connect only the signal wire and ground between the Pico and the ESC.

### Pico: Quick start

#### Flash the Pico

1. Get the latest zip from releases and extract it.
2. Hold the BOOTSEL button and power the pico
3. Grab the uf2 from the extracted zip and drap and drop it to the RPI-RP2 drive

No libraries are needed beyond the Arduino-Pico core.

#### Use it

1. Wire the Pico to the ESC as above and power the ESC separately.
2. Open your 4-way-interface host software (a Web Serial browser such as Chrome or Edge is required for browser-based tools).
3. Select the Pico's USB serial port and connect. The baud rate setting does not matter.
4. Read the ESC settings, change them, or flash firmware.

Always read and save the original settings before flashing, and do not remove ESC power or the USB cable during a write.

### Pico: How it works

```
loop()
 └─ Serial.read() ─► parseMessage()  ── state machine: 0x2F cmd addrH addrL len params crcH crcL
        │                               PARSE_OK ─────────► blheli_handle_command()
        │                               PARSE_BAD_CRC ────► fourway_send_response(ACK_I_INVALID_CRC)
        ▼
 blheli_handle_command()
        ├─ interface commands (alive / version / name / set mode / exit) answered locally
        └─ device commands ─► blheli_enter_bootloader / blheli_read / blheli_write / blheli_page_erase / blheli_run
                                  │   esc_wire_tx_begin / tx_byte / tx_end / rx_byte   (EscWire.cpp, PIO)
                                  ▼
                          fourway_send_response()  ── 0x2E cmd addrH addrL len params ack crcH crcL
```

* **Request/response.** The PC sends one frame and waits for the answer. The Pico processes one command at a time.
* **Interrupts stay enabled.** The PIO hardware times every bit, so ESC traffic does not need interrupts masked.
* **Shared buffer.** One 256-byte buffer carries request parameters in and response data out.
* **One-call responses.** A response frame is built in a single buffer and written to USB in one call.
* **Frame timeout.** A partially received PC frame is discarded after 100 ms (`FOURWAY_RX_TIMEOUT_MS`).

### Pico: PIO UART and timing

At 19200 baud one bit is **52.08 µs**. `EscWire.cpp` runs two PIO programs, one transmitter and one receiver, on the same
GPIO. Each program takes 8 PIO clock cycles per bit, so the PIO clock divider is `clk_sys / (8 × 19200)`.

The programs are the Raspberry Pi 8N1 UART transmit and receive programs (`uart_tx`, `uart_rx`), assembled with `pioasm`.

#### Transmit

* The transmit program pulls one byte from an 8-deep FIFO and shifts it out LSB first. The start and stop bits come from
  side-set on the same pin. Not inverted.
* `esc_wire_tx_begin()` stops the receiver, drives the pin HIGH, makes it an output and waits `ESC_IDLE_US` (54 µs, one
  idle bit) before the first start bit.
* `esc_wire_tx_byte()` queues a byte and only blocks while the FIFO is full.
* `esc_wire_tx_end()` waits until the last byte is on the wire (the transmitter stalls on its next pull, which is the
  moment the stop bit starts), then releases the pin and restarts the receiver. Releasing at the start of the stop bit lets
  the ESC's reply be heard; the pull-up holds the line HIGH.

#### Receive

1. The receive program waits for the start bit, then samples 8 bits at bit centres (12 PIO cycles to the first sample, 8 per bit).
2. It checks the stop bit. A bad stop bit raises an IRQ flag and nothing is pushed.
3. `esc_wire_rx_byte(timeout_ms)` returns the byte from the FIFO, `-1` on timeout, or `-2` on a framing error.

#### Timing accuracy

Bit timing comes from the PIO clock divider, not from loop counting, so there are no per-bit timing constants to tune.
The only timing constant is `ESC_IDLE_US`.

### Pico: Source file and function reference

#### Files

| File | Role |
|---|---|
| `FourWayIf.h` | signal pin, command/ACK codes, bootloader constants, declarations |
| `FourWayIf.cpp` | CRC, bootloader layer, 4w-if parser and responder, `setup()` and `loop()` |
| `EscWire.h` | PIO wire API and baud/idle constants |
| `EscWire.cpp` | PIO programs and the half-duplex UART functions |
| `pico_4way_IF.ino` | minimal sketch file that includes the header |

#### Functions (`FourWayIf.cpp`)

| Function | Purpose |
|---|---|
| `crc16_xmodem_update(crc, byte)` | CRC-16/XMODEM, PC link |
| `crc16_blheli_update(crc, byte)` | CRC-16 poly 0xA001, ESC link |
| `blheli_send_command(cmd, len)` | command bytes + CRC |
| `blheli_enter_bootloader(info)` | handshake, fills `blheli_info_t` |
| `blheli_page_erase(addr)` | erase one page |
| `blheli_read(blCmd, addr, buf, len)` | read flash or EEPROM |
| `blheli_write(blCmd, addr, buf, len, ackMs)` | write flash or EEPROM |
| `blheli_run()` | reset the ESC |
| `parseMessage(byte)` | PC frame state machine |
| `fourway_send_response(...)` | build and send a PC response frame |
| `blheli_handle_command(...)` | execute one validated PC command |
| `setup()` / `loop()` | Arduino entry points |

#### Functions (`EscWire.cpp`)

| Function | Purpose |
|---|---|
| `esc_wire_begin(pin)` | claim a PIO block and two state machines, load both programs, bind them to the pin |
| `esc_wire_tx_begin()` | stop the receiver, drive the line, wait one idle bit |
| `esc_wire_tx_byte(b)` | queue one byte |
| `esc_wire_tx_end()` | wait for the last byte, release the pin, restart the receiver |
| `esc_wire_rx_byte(timeout_ms)` | receive one byte, `-1` timeout, `-2` framing error |

### Pico: Configuration

Edit `FourWayIf.h` (and `EscWire.h` for the two wire constants):

| Macro | Default | Meaning |
|---|---|---|
| `ESC_PIN` | 2 | the signal GPIO (0 to 28) |
| `PC_BAUD` | 38400 | passed to `Serial.begin()`; USB serial ignores it |
| `ESC_BAUD` (`EscWire.h`) | 19200 | ESC link baud rate |
| `ESC_IDLE_US` (`EscWire.h`) | 54 | idle HIGH time before a transmission |
| `FOURWAY_RX_TIMEOUT_MS` | 100 | discard a stalled PC frame |
| `BL_T_ACK_MS`, `BL_T_NOACK_MS`, `BL_T_BYTE_MS` | 10, 6, 20 | ESC reply timeouts (ms) |
| `BL_T_ERASE_MS`, `BL_T_WRITE_MS`, `BL_T_EEWRITE_MS` | 200, 200, 3000 | erase/write timeouts (ms) |

To use a different pin, change `ESC_PIN` only.

### Pico: Troubleshooting

| Symptom | Likely cause and action |
|---|---|
| Host cannot connect | Select the Pico's USB serial port. The baud rate does not matter. If the Pico shows up as a drive (RPI-RP2) instead of a serial port, the sketch was not uploaded. |
| The LED blinks quickly and nothing answers | `esc_wire_begin()` failed: no PIO block has two free state machines and 13 free instruction words. Another library may be using the PIO. |
| Handshake (`InitFlash`) fails | ESC not powered, signal wire not connected to `ESC_PIN`, no common ground, or the ESC already left its bootloader. Power-cycle the ESC with the signal wire idle HIGH and retry. |
| Handshake works, reads fail | Check the wiring and ground, and that the ESC supply is stable. |
| Host reports a non-OK reply | The Pico returned an ACK other than `0x00`: unsupported command, wrong channel, or the ESC did not answer. |
| Works once, fails after a reset | The ESC left the bootloader (for example after `cmd_DeviceReset`). Run the handshake again. |

As a last resort if it connects to the interface but does not read the ESC, follow the steps below.

#### Pico

Add a **2.7 kΩ** resistor between the GPIO where the signal wire is connected (GP2 by default) and **3.3 V**
(3V3(OUT), physical pin 36). **NEVER 5 V.**

### Pico: Known limitations

* One ESC (channel 0 on the signal pin). Hosts that probe other channels receive `ACK_I_INVALID_CHANNEL`.
* SiLabs (BLHeli_S) bootloader only. No Atmel, ARM, BLHeli_32 or C2 programming.
* No debug mode.
* While a bootloader transaction runs, `loop()` is busy: about 64 ms for a 112-byte read, up to 3 s while waiting for an
  EEPROM write acknowledge.

---

## Protocol reference (both boards)

These sections apply to both versions. "Arduino" in the tables below means the bridge board, Uno/Nano or Pico.

### PC link: the 4w-if protocol

Serial settings: **38400 baud, 8 data bits, no parity, 1 stop bit.** (On the Pico the link is native USB serial, which ignores the baud rate.)

#### Request (PC → Arduino)

| Byte | Meaning |
|---|---|
| 0 | `0x2F` start marker |
| 1 | Command (`0x30`–`0x40`) |
| 2 | Address high byte |
| 3 | Address low byte |
| 4 | Parameter count N (`0` means 256) |
| 5 … 5+N−1 | Parameters |
| 5+N | CRC high byte |
| 6+N | CRC low byte |

The CRC is **CRC-16/XMODEM** (poly `0x1021`, init `0x0000`, MSB first) over every byte from the start marker up to the last
parameter.

#### Response (Arduino → PC)

| Byte | Meaning |
|---|---|
| 0 | `0x2E` start marker |
| 1 | Command, echoed |
| 2, 3 | Address high, low, echoed |
| 4 | Parameter count N (`0` means 256) |
| 5 … 5+N−1 | Response parameters |
| 5+N | ACK code |
| 6+N | CRC high byte |
| 7+N | CRC low byte |

The CRC covers everything from the start marker through the ACK byte. A response with no data carries one `0x00`
parameter, so its length is 9 bytes.

#### Commands

| Code | Name | Request parameters | Response parameters |
|---|---|---|---|
| `0x30` | `cmd_InterfaceTestAlive` | 1 byte (ignored) | `00` |
| `0x31` | `cmd_ProtocolGetVersion` | 1 byte (ignored) | `6B` (107) |
| `0x32` | `cmd_InterfaceGetName` | 1 byte (ignored) | ASCII `m4wFCIntf` |
| `0x33` | `cmd_InterfaceGetVersion` | 1 byte (ignored) | `C8 05` (200, 5) |
| `0x34` | `cmd_InterfaceExit` | 1 byte (ignored) | `00` |
| `0x35` | `cmd_DeviceReset` | channel | `00` |
| `0x37` | `cmd_DeviceInitFlash` | channel | 4 bytes: signature low, signature high, bootloader character, interface mode |
| `0x38` | `cmd_DeviceEraseAll` | – | rejected (`ACK_I_INVALID_CMD`) |
| `0x39` | `cmd_DevicePageErase` | page number | `00` |
| `0x3A` | `cmd_DeviceRead` | length (`0` = 256); address in header | the data bytes |
| `0x3B` | `cmd_DeviceWrite` | the data bytes; address in header | `00` |
| `0x3C` | `cmd_DeviceC2CK_LOW` | – | rejected |
| `0x3D` | `cmd_DeviceReadEEprom` | length (`0` = 256); address in header | the data bytes |
| `0x3E` | `cmd_DeviceWriteEEprom` | the data bytes; address in header | `00` |
| `0x3F` | `cmd_InterfaceSetMode` | mode (only `1` = SiLabs bootloader accepted) | `00` |
| `0x40` | `cmd_DeviceVerify` | – | rejected |

#### ACK codes

| Code | Name | Used here for |
|---|---|---|
| `0x00` | `ACK_OK` | success |
| `0x02` | `ACK_I_INVALID_CMD` | unsupported command |
| `0x03` | `ACK_I_INVALID_CRC` | PC frame failed its CRC |
| `0x08` | `ACK_I_INVALID_CHANNEL` | channel other than 0 |
| `0x09` | `ACK_I_INVALID_PARAM` | unsupported interface mode |
| `0x0F` | `ACK_D_GENERAL_ERROR` | ESC did not answer or reported an error |

The remaining codes (`0x01`, `0x04`–`0x07`) are defined in the header but never sent.

### ESC link: the BLHeli_S bootloader protocol

The BLHeli_S bootloader is a simple master/slave UART protocol, **19200 baud, 8N1, not inverted**, on the single signal
wire. The Arduino transmits, then releases the line to receive the reply.

#### Command frame (Arduino → ESC)

```
cmd  p1  [p2 ...]  CRC_lo  CRC_hi
```

The CRC is the reflected CRC-16 with polynomial `0xA001` and initial value `0`, computed over the command and
parameter bytes and sent **low byte first**.

#### Bootloader commands

| Code | Name | Frame | Reply |
|---|---|---|---|
| `0x00` | `CMD_RUN` | `00 00` + CRC | none (ESC resets) |
| `0x01` | `CMD_PROG_FLASH` | `01 01` + CRC | `30` |
| `0x02` | `CMD_ERASE_FLASH` | `02 01` + CRC | `30` |
| `0x03` | `CMD_READ_FLASH_SIL` | `03 len` + CRC (`len` 0 = 256) | `len` bytes, CRC lo, CRC hi, `30` |
| `0x04` | `CMD_READ_EEPROM` | `04 len` + CRC | same as flash read |
| `0x05` | `CMD_PROG_EEPROM` | `05 01` + CRC | `30` |
| `0xFD` | `CMD_KEEP_ALIVE` | defined, not used | – |
| `0xFE` | `CMD_SET_BUFFER` | `FE 00 lenHi lenLo` + CRC | **none**, then the data block is sent |
| `0xFF` | `CMD_SET_ADDRESS` | `FF 00 addrHi addrLo` + CRC | `30` |

`0x30` is the bootloader's success byte (`BR_SUCCESS`). It is not the same as the 4w-if `ACK_OK` (`0x00`); the Arduino
translates between them.

#### Handshake (entering the bootloader)

The Arduino sends 17 bytes:

```
00 00 00 00 00 00 00 00   0D   42 4C 48 65 6C 69   F4 7D
8 x zero                   CR   "BLHeli"             CRC lo, hi
```

The CRC covers only the six bytes `BLHeli`; the zeros and the `0x0D` are not part of it.

The ESC answers 9 bytes:

```
34 37 31 xx   sigHi sigLo   bootVersion   bootPages   30
"471" + char  signature     bootloader    flash pages  ACK
```

`xx` is the 4th banner character (`0x64` = `d` on the ESC used in testing). The signature identifies the MCU
(for example `E8B1`).

#### Flash read

```
Arduino → ESC : FF 00 aH aL CRC CRC        (set address)
ESC → Arduino : 30
Arduino → ESC : 03 len CRC CRC             (read)
ESC → Arduino : d0 d1 ... d(len-1)  CRC_lo CRC_hi  30
```

The Arduino checks that the CRC over the data plus the two CRC bytes equals `0`, and that the final byte is `0x30`.

#### Flash write

```
Arduino → ESC : FF 00 aH aL CRC CRC        (set address)          ESC → 30
Arduino → ESC : FE 00 nH nL CRC CRC        (set buffer length)    ESC → (silence)
Arduino → ESC : d0 ... d(n-1) CRC_lo CRC_hi                       ESC → 30
Arduino → ESC : 01 01 CRC CRC              (program flash)        ESC → 30
```

EEPROM writes use `05 01` as the last command and a longer timeout (3 s).

#### Page erase

BLHeli_S flash pages are 512 bytes. The 4w-if page number is converted to an address with `page << 9`, then
`SET_ADDRESS` and `02 01` are sent.

#### Reset

`00 00` + CRC (`00 00 00 00` on the wire) soft-resets the ESC MCU. With no new handshake it then starts the application.

### Command translation table

How each 4w-if command is handled by `blheli_handle_command()`:

| 4w-if command | Action on the ESC link | Notes |
|---|---|---|
| `InterfaceTestAlive` | none | ACK only |
| `ProtocolGetVersion` | none | returns 107 |
| `InterfaceGetName` | none | returns `m4wFCIntf` |
| `InterfaceGetVersion` | none | returns `C8 05` |
| `InterfaceExit` | none | ACK only |
| `InterfaceSetMode` | none | accepts mode 1 only |
| `DeviceReset` | `CMD_RUN` (`00 00`) | channel must be 0 |
| `DeviceInitFlash` | handshake | returns signature low/high, banner char, mode 1 |
| `DevicePageErase` | `SET_ADDRESS` + `02 01` | address = page × 512 |
| `DeviceRead` | `SET_ADDRESS` + `03 len` | up to 256 bytes |
| `DeviceReadEEprom` | `SET_ADDRESS` + `04 len` | up to 256 bytes |
| `DeviceWrite` | `SET_ADDRESS`, `SET_BUFFER`, data, `01 01` | up to 256 bytes |
| `DeviceWriteEEprom` | `SET_ADDRESS`, `SET_BUFFER`, data, `05 01` | up to 256 bytes |
| `DeviceEraseAll`, `DeviceC2CK_LOW`, `DeviceVerify`, unknown | none | `ACK_I_INVALID_CMD` |

### Byte-level examples

All CRCs below were computed with the same algorithms the code uses.

#### 4w-if link (hex)

| Direction | Frame |
|---|---|
| PC → Arduino, test alive | `2F 30 00 00 01 00 CF D4` |
| Arduino → PC, reply | `2E 30 00 00 01 00 00 44 C2` |
| PC → Arduino, init flash, channel 0 | `2F 37 00 00 01 00 A8 00` |
| Arduino → PC, reply (signature `E8B1`, `d`, mode 1) | `2E 37 00 00 04 B1 E8 64 01 00 B2 2D` |
| PC → Arduino, read 0x70 bytes at `0x1A00` | `2F 3A 1A 00 01 70 84 E1` |

#### ESC link (hex, with CRC low byte first)

| Purpose | Bytes |
|---|---|
| Set address `0x1A00` | `FF 00 1A 00 3B 74` |
| Read 0x70 bytes | `03 70 01 14` |
| Erase page | `02 01 C0 A0` |
| Program flash | `01 01 C0 50` |
| Set buffer, 256 bytes | `FE 00 01 00 30 78` |
| Run / reset | `00 00 00 00` |
| Handshake | `00 00 00 00 00 00 00 00 0D 42 4C 48 65 6C 69 F4 7D` |

#### Full read of a settings block

```
PC → Arduino   : 2F 3A 1A 00 01 70 84 E1
Arduino → ESC  : FF 00 1A 00 3B 74
ESC → Arduino  : 30
Arduino → ESC  : 03 70 01 14
ESC → Arduino  : <112 data bytes> <CRC lo> <CRC hi> 30
Arduino → PC   : 2E 3A 1A 00 70 <112 data bytes> 00 <CRC hi> <CRC lo>
```

At 19200 baud a 112-byte read takes about 64 ms on the wire. Erases and flash writes take longer, and EEPROM writes are
allowed up to 3 s.

---

## Credits and sources

Major sources used to write the bootloader and protocol code:

1. **INAV `serial_4way_avrootloader.c`** (INAV 1.5.1, via the mluessi/inav fork of iNavFlight/inav; derived from the
   Cleanflight/Betaflight 4-way code, credited there to 4712 and Hagen's AVRootloader). Source of the handshake bytes,
   command sequences, ACK byte, CRC handling and timeouts.
   <https://github.com/mluessi/inav/blob/1.5.1/src/main/io/serial_4way_avrootloader.c>
2. **ArduPilot `libraries/AP_BLHeli/blheli_4way_protocol.h`** (based on Betaflight `serial_4way.c`). Source of the 4w-if
   command codes, interface modes and protocol version.
   <https://github.com/ArduPilot/ardupilot/blob/master/libraries/AP_BLHeli/blheli_4way_protocol.h>
3. **BLHeli "programming adapters" document** (BLHeli project). Source for the 19200 baud, single-wire bootloader link.
   <https://git.bertfpv.com/englebert/BLHeli/raw/branch/master/SiLabs/BLHeli%20programming%20adapters.pdf>
4. **Raspberry Pi `pico-examples`, `pio/uart_tx` and `pio/uart_rx`** (BSD-3-Clause, © 2020 Raspberry Pi (Trading) Ltd).
   The PIO UART programs used by the Pico version.
   <https://github.com/raspberrypi/pico-examples>

---

## License note

See License

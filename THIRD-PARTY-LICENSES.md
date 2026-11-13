# Third-Party Licenses and Credits

This repository contains two implementations of a BLHeli 4-way interface:

* `uno_4way_IF/` for Arduino Uno / Nano
* `pico_4way_IF/` for Raspberry Pi Pico / Pico W

This project is licensed under the **GNU General Public License v3.0** (see [LICENSE](LICENSE)). This file lists third-party
material that is included in, or was used as a reference for, the code in this repository.

| Component | Used in | License | Type of use |
|---|---|---|---|
| Raspberry Pi `pico-examples` UART PIO programs | `pico_4way_IF/EscWire.cpp` | BSD-3-Clause | Included (assembled instruction words) |
| INAV, Cleanflight, Betaflight 4-way code | `uno_4way_IF/`, `pico_4way_IF/` | GPL v3 | Protocol reference |
| ArduPilot `AP_BLHeli` 4-way protocol header | `uno_4way_IF/`, `pico_4way_IF/` | GPL v3 | Protocol reference |

---

## 1. Raspberry Pi `pico-examples` – UART PIO programs (included)

**Used in:** `pico_4way_IF/EscWire.cpp`

The two PIO programs in `EscWire.cpp` (the transmit program `esc_tx` and the receive program `esc_rx`) are the Raspberry Pi
8N1 UART programs `uart_tx` and `uart_rx`, assembled with `pioasm`. Source:
<https://github.com/raspberrypi/pico-examples> (`pio/uart_tx/uart_tx.pio`, `pio/uart_rx/uart_rx.pio`).

```
Copyright (c) 2020 Raspberry Pi (Trading) Ltd.

SPDX-License-Identifier: BSD-3-Clause

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
   disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following
   disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

---

## 2. INAV, Cleanflight and Betaflight – 4-way interface (protocol reference)

**Used in:** `uno_4way_IF/` and `pico_4way_IF/` (the BLHeli_S bootloader layer in `FourWayIf.cpp`)

The BLHeli_S bootloader command sequences, handshake bytes, command codes, ACK byte and reply timeouts follow the 4-way
interface code in these projects. The code in this repository was written independently, but it follows their protocol
sequences, constants and timeouts closely, so credit is given here.

* **INAV**, `serial_4way_avrootloader.c` (INAV 1.5.1), derived from the Cleanflight and Betaflight 4-way code, which
  credits 4712 and Hagen's AVRootloader.
  <https://github.com/mluessi/inav/blob/1.5.1/src/main/io/serial_4way_avrootloader.c>
* **Cleanflight** and **Betaflight**, `serial_4way` implementation.
  <https://github.com/betaflight/betaflight>

License: **GNU General Public License v3** (GPL v3). See the license file in each project's repository for the full text.

---

## 3. ArduPilot – 4-way protocol header (protocol reference)

**Used in:** `uno_4way_IF/` and `pico_4way_IF/` (command codes, interface modes, protocol version numbers)

The 4w-if command codes, interface mode values and protocol version follow `libraries/AP_BLHeli/blheli_4way_protocol.h`,
which is based on Betaflight's `serial_4way.c`.
<https://github.com/ArduPilot/ardupilot/blob/master/libraries/AP_BLHeli/blheli_4way_protocol.h>

License: **GNU General Public License v3** (GPL v3). See the ArduPilot repository for the full text.

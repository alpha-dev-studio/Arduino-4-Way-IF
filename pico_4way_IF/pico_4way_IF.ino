/*
 * pico_4way_IF.ino
 * BLHeli 4-way interface for BLHeli_S ESCs on Raspberry Pi Pico / Pico W (Arduino-Pico core).
 * All logic is in FourWayIf.cpp (protocol, setup() and loop()) and EscWire.cpp (PIO UART on the signal wire).
 *
 * Wiring: GP2 -> ESC signal wire, GND -> ESC GND. The ESC is powered separately.
 * Links:  PC over USB serial (4w-if), ESC 19200 baud on GP2 (PIO half-duplex).
 */
#include "FourWayIf.h"

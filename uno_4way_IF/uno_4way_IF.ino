// Blheli-4-Way-IF
// Copyright (C) 2026 alpha-dev-studio
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * uno_4way_IF.ino
 * Arduino 4-way interface for BLHeli_S ESCs. All logic is in FourWayIf.cpp (setup() and loop() included).
 *
 * Wiring: D11 -> ESC signal wire, GND -> ESC GND. The ESC is powered separately.
 * Links:  PC 38400 baud (4w-if), ESC 19200 baud (bit-banged bootloader protocol).
 */
#include "FourWayIf.h"

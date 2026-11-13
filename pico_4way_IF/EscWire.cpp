// Blheli-4-Way-IF
// Copyright (C) 2026 alpha-dev-studio
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * EscWire.cpp
 * PIO half-duplex UART for the BLHeli_S signal wire.
 *
 * The two PIO programs are the Raspberry Pi 8N1 UART transmit and receive programs
 * (pico-examples/pio/uart_tx and uart_rx, BSD-3-Clause, (c) 2020 Raspberry Pi (Trading) Ltd), assembled with pioasm 2.3.1.
 * Each runs 8 PIO cycles per bit.
 */
#include "EscWire.h"
#include <hardware/pio.h>
#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <pico/time.h>

/* esc_tx: pull / set x,7 / out pins,1 / jmp x--. Start and stop bits come from side-set on the same pin. */
static const uint16_t esc_tx_instr[] = { 0x9fa0, 0xf727, 0x6001, 0x0642 };
static const pio_program_t esc_tx_prog = { esc_tx_instr, 4, -1 };

/* esc_rx: wait for the start bit, sample 8 bits at bit centres, check the stop bit, push the byte
 * (left-justified in the FIFO word). A bad stop bit raises relative IRQ flag 4 and nothing is pushed. */
static const uint16_t esc_rx_instr[] = { 0x2020, 0xea27, 0x4001, 0x0642, 0x00c8, 0xc014, 0x20a0, 0x0000, 0x8020 };
static const pio_program_t esc_rx_prog = { esc_rx_instr, 9, -1 };

static PIO  s_pio;
static uint s_tx, s_rx;             /* state machine numbers */
static uint s_txOff, s_rxOff;       /* program load offsets */
static uint s_pin;

/* Flush the receiver, restart it at its first instruction and enable it. */
static void rx_start(void)
{
    pio_sm_set_enabled(s_pio, s_rx, false);
    pio_sm_clear_fifos(s_pio, s_rx);
    pio_sm_restart(s_pio, s_rx);
    pio_sm_exec(s_pio, s_rx, pio_encode_jmp(s_rxOff));
    pio_interrupt_clear(s_pio, 4 + s_rx);
    pio_sm_set_enabled(s_pio, s_rx, true);
}

bool esc_wire_begin(uint8_t pin)
{
    PIO pios[2] = { pio0, pio1 };
    for (int i = 0; i < 2; i++) {
        PIO pio = pios[i];
        if (!pio_can_add_program(pio, &esc_tx_prog)) continue;
        int tx = pio_claim_unused_sm(pio, false);
        if (tx < 0) continue;
        int rx = pio_claim_unused_sm(pio, false);
        if (rx < 0) { pio_sm_unclaim(pio, (uint)tx); continue; }
        uint txOff = pio_add_program(pio, &esc_tx_prog);
        if (!pio_can_add_program(pio, &esc_rx_prog)) {
            pio_remove_program(pio, &esc_tx_prog, txOff);
            pio_sm_unclaim(pio, (uint)tx); pio_sm_unclaim(pio, (uint)rx);
            continue;
        }
        uint rxOff = pio_add_program(pio, &esc_rx_prog);

        s_pio = pio; s_tx = (uint)tx; s_rx = (uint)rx; s_txOff = txOff; s_rxOff = rxOff; s_pin = pin;

        float div = (float)clock_get_hz(clk_sys) / (8.0f * (float)ESC_BAUD);

        pio_gpio_init(pio, pin);
        gpio_pull_up(pin);
        pio_sm_set_consecutive_pindirs(pio, s_tx, pin, 1, false);

        pio_sm_config c = pio_get_default_sm_config();
        sm_config_set_wrap(&c, txOff, txOff + 3);
        sm_config_set_sideset(&c, 2, true, false);
        sm_config_set_out_pins(&c, pin, 1);
        sm_config_set_sideset_pins(&c, pin);
        sm_config_set_out_shift(&c, true, false, 32);
        sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
        sm_config_set_clkdiv(&c, div);
        pio_sm_init(pio, s_tx, txOff, &c);

        c = pio_get_default_sm_config();
        sm_config_set_wrap(&c, rxOff, rxOff + 8);
        sm_config_set_in_pins(&c, pin);
        sm_config_set_jmp_pin(&c, pin);
        sm_config_set_in_shift(&c, true, false, 32);
        sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
        sm_config_set_clkdiv(&c, div);
        pio_sm_init(pio, s_rx, rxOff, &c);

        rx_start();
        return true;
    }
    return false;
}

void esc_wire_tx_begin(void)
{
    const uint32_t mask = 1u << s_pin;
    pio_sm_set_enabled(s_pio, s_rx, false);
    pio_sm_clear_fifos(s_pio, s_rx);
    pio_sm_set_enabled(s_pio, s_tx, false);
    pio_sm_clear_fifos(s_pio, s_tx);
    pio_sm_restart(s_pio, s_tx);
    pio_sm_exec(s_pio, s_tx, pio_encode_jmp(s_txOff));
    pio_sm_set_pins_with_mask(s_pio, s_tx, mask, mask);        /* level HIGH first, then drive: no glitch */
    pio_sm_set_pindirs_with_mask(s_pio, s_tx, mask, mask);
    pio_sm_set_enabled(s_pio, s_tx, true);
    busy_wait_us_32(ESC_IDLE_US);
}

void esc_wire_tx_byte(uint8_t b)
{
    pio_sm_put_blocking(s_pio, s_tx, b);
}

void esc_wire_tx_end(void)
{
    const uint32_t stallBit = 1u << (PIO_FDEBUG_TXSTALL_LSB + s_tx);
    uint32_t t0 = time_us_32();
    /* Last byte pulled into the shifter: the FIFO is empty. */
    while (!pio_sm_is_tx_fifo_empty(s_pio, s_tx) && (time_us_32() - t0) < 20000) { }
    /* The transmitter stalls on its next PULL when that byte's data bits are done: stop bit has just started. */
    s_pio->fdebug = stallBit;
    t0 = time_us_32();
    while (!(s_pio->fdebug & stallBit) && (time_us_32() - t0) < 2000) { }
    pio_sm_set_enabled(s_pio, s_tx, false);
    pio_sm_set_pindirs_with_mask(s_pio, s_tx, 0, 1u << s_pin);  /* release; pull-up holds the line HIGH */
    rx_start();
}

int16_t esc_wire_rx_byte(uint32_t timeout_ms)
{
    const uint32_t timeoutUs = timeout_ms * 1000u;
    const uint32_t t0 = time_us_32();
    for (;;) {
        if (!pio_sm_is_rx_fifo_empty(s_pio, s_rx))
            return (int16_t)(pio_sm_get(s_pio, s_rx) >> 24);
        if (pio_interrupt_get(s_pio, 4 + s_rx)) {
            pio_interrupt_clear(s_pio, 4 + s_rx);
            return -2;
        }
        if ((time_us_32() - t0) >= timeoutUs) return -1;
    }
}

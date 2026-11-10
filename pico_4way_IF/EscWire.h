/*
 * EscWire.h
 * Half-duplex 19200 baud 8N1 UART on one GPIO, implemented with two RP2040 PIO state machines
 * (one transmitter, one receiver) sharing the pin. The PIO hardware generates and samples every bit,
 * so no interrupt masking or cycle counting is needed.
 */
#ifndef ESCWIRE_H
#define ESCWIRE_H

#include <stdint.h>
#include <stdbool.h>

#define ESC_BAUD        19200UL
#define ESC_IDLE_US     54          /* idle HIGH time before a transmission (one bit = 52.08 us) */

/* Claim a PIO block and two state machines, load the TX/RX programs and bind them to `pin` (pull-up on).
 * Returns false if no PIO block has room. */
bool    esc_wire_begin(uint8_t pin);

/* Take the line: stop the receiver, drive the pin HIGH and wait one idle bit. */
void    esc_wire_tx_begin(void);

/* Queue one byte for transmission (blocks only while the 8-deep FIFO is full). */
void    esc_wire_tx_byte(uint8_t b);

/* Wait until the last byte is on the wire (its stop bit just started), release the pin, restart the receiver. */
void    esc_wire_tx_end(void);

/* Receive one byte. Returns 0..255, -1 on timeout, -2 on framing error. */
int16_t esc_wire_rx_byte(uint32_t timeout_ms);

#endif

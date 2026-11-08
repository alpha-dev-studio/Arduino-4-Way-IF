/*
 * FourWayIf.h
 * BLHeli 4-way interface (4w-if) bridge for Arduino Uno/Nano (ATmega328P, 16 MHz); also builds for Mega (ATmega2560).
 *
 *   PC (esc-configurator, Web Serial) <--38400 baud, 4w-if--> Arduino <--19200 baud, bit-banged on D11--> BLHeli_S ESC
 *
 * Holds pin selection, timing constants, protocol command/ACK codes and the public function declarations.
 */
#ifndef FOURWAYIF_H
#define FOURWAYIF_H

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

/* ===== Signal-wire pin: Arduino D11 ===== */
#define ESC_PIN_ARDUINO     11
#if defined(__AVR_ATmega2560__)
  #define ESC_DDR           DDRB
  #define ESC_PORT          PORTB
  #define ESC_PINREG        PINB
  #define ESC_MASK          (1 << PB5)      /* D11 = PB5 on Mega */
#else
  #define ESC_DDR           DDRB
  #define ESC_PORT          PORTB
  #define ESC_PINREG        PINB
  #define ESC_MASK          (1 << PB3)      /* D11 = PB3 on Uno/Nano */
#endif

/*
 * Debug switch. 1: a failed DeviceRead is answered with ACK_OK and 8 diagnostic bytes (plus any data received)
 * so esc-configurator's log prints them. 0: a failed read returns ACK_D_GENERAL_ERROR (normal operation).
 *   [0]=0xDB  [1]=stage  [2]=reply to SET_ADDRESS (0xFF = none)  [3]=data bytes received
 *   [4]=last receive result (0 ok, 1 timeout, 2 framing error)  [5..6]=CRC lo/hi (0 when good)  [7]=final ACK byte
 *   stage: 1 SET_ADDRESS failed, 2 data timeout/framing, 3 CRC mismatch, 4 bad final ACK, 5 other
 */
#define FOURWAY_DEBUG       0

/* ===== Baud rates and bit timing ===== */
#define PC_BAUD             38400UL
#define ESC_BAUD            19200UL
#define ESC_BIT_TIME_US     52              /* nominal bit time, 1/19200 s = 52.08 us */
#define ESC_HALF_BIT_US     26

/* Delays used by the bit-banging code, reduced to account for loop overhead at 16 MHz (target 52.08 us per bit). */
#define ESC_TX_BIT_US       51.5            /* data bit inside the transmit loop */
#define ESC_TX_EDGE_US      51.8            /* start bit, stop bit and idle bit */
#define ESC_RX_FIRST_US     77.0            /* start edge to the centre of data bit 0 */
#define ESC_RX_BIT_US       51.4            /* between samples inside the receive loop */

/* ===== 4w-if framing (PC link) ===== */
#define FOURWAY_PC_START    0x2F            /* first byte of a PC -> Arduino frame */
#define FOURWAY_RX_START    0x2E            /* first byte of an Arduino -> PC frame */
#define FOURWAY_RX_TIMEOUT_MS 100           /* a frame that stalls this long is discarded */

/* 4w-if command codes */
#define cmd_InterfaceTestAlive   0x30
#define cmd_ProtocolGetVersion   0x31
#define cmd_InterfaceGetName     0x32
#define cmd_InterfaceGetVersion  0x33
#define cmd_InterfaceExit        0x34
#define cmd_DeviceReset          0x35
#define cmd_DeviceInitFlash      0x37
#define cmd_DeviceEraseAll       0x38       /* rejected: not available in SiLabs bootloader mode */
#define cmd_DevicePageErase      0x39
#define cmd_DeviceRead           0x3A
#define cmd_DeviceWrite          0x3B
#define cmd_DeviceC2CK_LOW       0x3C       /* rejected: C2 interface only */
#define cmd_DeviceReadEEprom     0x3D
#define cmd_DeviceWriteEEprom    0x3E
#define cmd_InterfaceSetMode     0x3F
#define cmd_DeviceVerify         0x40       /* rejected: not available in SiLabs bootloader mode */

/* 4w-if ACK codes (Betaflight serial_4way numbering) */
#define ACK_OK                   0x00
#define ACK_I_UNKNOWN_ERROR      0x01
#define ACK_I_INVALID_CMD        0x02
#define ACK_I_INVALID_CRC        0x03
#define ACK_I_VERIFY_ERROR       0x04
#define ACK_D_INVALID_COMMAND    0x05
#define ACK_D_COMMAND_FAILED     0x06
#define ACK_D_UNKNOWN_ERROR      0x07
#define ACK_I_INVALID_CHANNEL    0x08
#define ACK_I_INVALID_PARAM      0x09
#define ACK_D_GENERAL_ERROR      0x0F

/* 4w-if interface modes (only imSIL_BLB is supported) */
#define imC2        0
#define imSIL_BLB   1
#define imATM_BLB   2
#define imSK        3
#define imARM_BLB   4

/* Identity reported to the PC (Betaflight 4w-if values) */
#define FOURWAY_PROTOCOL_VER     107
#define FOURWAY_IF_VER_HI        200        /* 20005 / 100 */
#define FOURWAY_IF_VER_LO        5          /* 20005 % 100 */
#define FOURWAY_IF_NAME          "m4wFCIntf"

/* ===== BLHeli_S bootloader (ESC link) ===== */
#define BL_RestartBootloader     0          /* CMD_RUN argument: soft-reset the ESC MCU */
#define BL_ExitBootloader        1          /* CMD_RUN argument: jump to the application (unused) */
#define CMD_RUN                  0x00
#define CMD_PROG_FLASH           0x01
#define CMD_ERASE_FLASH          0x02
#define CMD_READ_FLASH_SIL       0x03
#define CMD_READ_EEPROM          0x04
#define CMD_PROG_EEPROM          0x05
#define CMD_KEEP_ALIVE           0xFD
#define CMD_SET_BUFFER           0xFE
#define CMD_SET_ADDRESS          0xFF

#define BR_SUCCESS               0x30       /* bootloader ACK; differs from the 4w-if ACK_OK (0x00) */
#define BR_ERRORCOMMAND          0xC1
#define BR_ERRORCRC              0xC2
#define BR_NONE                  0xFF       /* internal marker: no reply received */

#define BL_PAGE_SHIFT            9          /* BLHeli_S flash pages are 512 bytes */

/* Reply timeouts in ms */
#define BL_T_ACK_MS              10         /* reply to SET_ADDRESS */
#define BL_T_NOACK_MS            6          /* SET_BUFFER must stay silent for this long */
#define BL_T_BYTE_MS             20         /* each byte of a read reply */
#define BL_T_ERASE_MS            200
#define BL_T_WRITE_MS            200
#define BL_T_EEWRITE_MS          3000

/* Result of the bootloader handshake */
typedef struct {
    uint8_t  sigHi;          /* device signature, high byte */
    uint8_t  sigLo;          /* device signature, low byte */
    uint8_t  bootChar;       /* 4th character of the "471x" banner */
    uint8_t  bootVersion;
    uint8_t  bootPages;
} blheli_info_t;

/* ===== CRC ===== */
uint16_t crc16_xmodem_update(uint16_t crc, uint8_t data);   /* poly 0x1021, PC link */
uint16_t crc16_blheli_update(uint16_t crc, uint8_t data);   /* poly 0xA001, ESC link */
uint8_t  crc_append_byte(uint16_t crc);                     /* byte making the CRC low byte 0 (utility) */

/* ===== Signal wire (interrupts must be disabled for the servo_* calls) ===== */
void     esc_delay_us(uint16_t us);
void     servo_send_byte(uint8_t b);                        /* send one byte, full stop bit */
void     servo_send_byte_nostop(uint8_t b);                 /* send one byte, return right after raising the stop bit */
int16_t  servo_recv_byte(uint16_t timeout_ms);              /* receive one byte; -1 timeout, -2 framing error */

/* ===== BLHeli_S bootloader ===== */
void     blheli_send_command(const uint8_t *cmd, uint8_t len);   /* cmd bytes + CRC, then release the line */
bool     blheli_enter_bootloader(blheli_info_t *info);
bool     blheli_page_erase(uint16_t addr);
bool     blheli_read(uint8_t blCmd, uint16_t addr, uint8_t *buf, uint16_t len);
void     blheli_get_diag(uint8_t *d8);                           /* 8 diagnostic bytes of the last read */
bool     blheli_write(uint8_t blCmd, uint16_t addr, const uint8_t *buf, uint16_t len, uint16_t ackMs);
void     blheli_run(void);

/* ===== 4w-if (PC link) ===== */
#define PARSE_NONE      0                   /* frame still incomplete */
#define PARSE_OK        1                   /* complete frame, CRC valid */
#define PARSE_BAD_CRC   2                   /* complete frame, CRC invalid */
uint8_t  parseMessage(uint8_t byteIn);
void     fourway_send_response(uint8_t cmd, uint16_t addr, const uint8_t *params, uint16_t n, uint8_t ack);
void     blheli_handle_command(uint8_t cmd, uint16_t addr, uint8_t *params, uint16_t plen);

/* setup() and loop() are defined in FourWayIf.cpp */
#endif

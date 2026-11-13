// Blheli-4-Way-IF
// Copyright (C) 2026 alpha-dev-studio
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * FourWayIf.cpp
 * BLHeli 4-way interface bridge: PC (4w-if over USB serial) <-> BLHeli_S bootloader (19200 baud, PIO on one GPIO).
 *
 * Sections: CRC | BLHeli_S bootloader layer | 4w-if parser/responder | Arduino entry points.
 * Signal-wire timing lives in EscWire.cpp.
 */
#include "FourWayIf.h"
#include "EscWire.h"
#include <string.h>

/* ===== CRC ===== */

/* CRC-16/XMODEM (poly 0x1021, init 0, MSB first). Used on the PC link. */
uint16_t crc16_xmodem_update(uint16_t crc, uint8_t data)
{
    crc ^= (uint16_t)data << 8;
    for (uint8_t i = 0; i < 8; i++)
        crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    return crc;
}

/* CRC-16 with reflected poly 0xA001 (init 0). Used on the ESC link; sent low byte first. */
uint16_t crc16_blheli_update(uint16_t crc, uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++) {
        if ((data ^ crc) & 0x01) crc = (crc >> 1) ^ 0xA001;
        else                     crc >>= 1;
        data >>= 1;
    }
    return crc;
}

/* ===== BLHeli_S bootloader layer ===== */

/* Send cmd[0..len-1] followed by the 0xA001 CRC (low byte, high byte), then listen for the reply. */
void blheli_send_command(const uint8_t *cmd, uint8_t len)
{
    uint16_t crc = 0;
    esc_wire_tx_begin();
    for (uint8_t i = 0; i < len; i++) {
        esc_wire_tx_byte(cmd[i]);
        crc = crc16_blheli_update(crc, cmd[i]);
    }
    esc_wire_tx_byte(crc & 0xFF);
    esc_wire_tx_byte(crc >> 8);
    esc_wire_tx_end();
}

/* Wait for one ESC reply byte; returns BR_NONE on timeout or framing error. */
static uint8_t bl_get_ack(uint32_t timeout_ms)
{
    int16_t c = esc_wire_rx_byte(timeout_ms);
    return (c < 0) ? BR_NONE : (uint8_t)c;
}

/*
 * Receive len data bytes, then (if checkCrc) 2 CRC bytes, then the ACK byte.
 * Succeeds only when the CRC over data+CRC bytes is 0 and the ACK is BR_SUCCESS (0x30).
 */
static bool bl_read_buf(uint8_t *buf, uint16_t len, bool checkCrc)
{
    uint16_t crc = 0;
    int16_t c;
    for (uint16_t i = 0; i < len; i++) {
        if ((c = esc_wire_rx_byte(BL_T_BYTE_MS)) < 0) return false;
        crc = crc16_blheli_update(crc, (uint8_t)c);
        buf[i] = (uint8_t)c;
    }
    if (checkCrc) {
        for (uint8_t i = 0; i < 2; i++) {
            if ((c = esc_wire_rx_byte(BL_T_BYTE_MS)) < 0) return false;
            crc = crc16_blheli_update(crc, (uint8_t)c);
        }
        if (crc != 0) return false;
    }
    if ((c = esc_wire_rx_byte(BL_T_BYTE_MS)) < 0) return false;
    return (uint8_t)c == BR_SUCCESS;
}

/*
 * Bootloader handshake. Sends 8x 0x00, 0x0D, "BLHeli" and its CRC (F4 7D); the ESC answers "471x",
 * signature high/low, bootloader version, page count and ACK 0x30. Fills *info on success.
 */
bool blheli_enter_bootloader(blheli_info_t *info)
{
    static const uint8_t bootInit[] = { 0,0,0,0,0,0,0,0, 0x0D, 'B','L','H','e','l','i', 0xF4, 0x7D };
    uint8_t bootInfo[8];
    esc_wire_tx_begin();
    for (uint8_t i = 0; i < sizeof(bootInit); i++) esc_wire_tx_byte(bootInit[i]);
    esc_wire_tx_end();
    if (bl_read_buf(bootInfo, sizeof(bootInfo), false) &&
        bootInfo[0] == '4' && bootInfo[1] == '7' && bootInfo[2] == '1') {
        info->bootChar    = bootInfo[3];
        info->sigHi       = bootInfo[4];
        info->sigLo       = bootInfo[5];
        info->bootVersion = bootInfo[6];
        info->bootPages   = bootInfo[7];
        return true;
    }
    return false;
}

/* CMD_SET_ADDRESS: sets the ESC's flash/EEPROM pointer. */
static bool bl_set_address(uint16_t addr)
{
    if (addr == 0xFFFF) return true;
    uint8_t c[4] = { CMD_SET_ADDRESS, 0, (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF) };
    blheli_send_command(c, 4);
    return bl_get_ack(BL_T_ACK_MS) == BR_SUCCESS;
}

/* Erase the flash page containing addr (CMD_SET_ADDRESS + CMD_ERASE_FLASH). */
bool blheli_page_erase(uint16_t addr)
{
    if (!bl_set_address(addr)) return false;
    uint8_t c[2] = { CMD_ERASE_FLASH, 0x01 };
    blheli_send_command(c, 2);
    return bl_get_ack(BL_T_ERASE_MS) == BR_SUCCESS;
}

/* Read len bytes (1..256) at addr into buf using blCmd (CMD_READ_FLASH_SIL or CMD_READ_EEPROM). */
bool blheli_read(uint8_t blCmd, uint16_t addr, uint8_t *buf, uint16_t len)
{
    if (!bl_set_address(addr)) return false;
    uint8_t c[2] = { blCmd, (uint8_t)(len & 0xFF) };   /* length 256 is encoded as 0x00 */
    blheli_send_command(c, 2);
    return bl_read_buf(buf, len, true);
}

/*
 * Write len bytes (1..256) at addr: CMD_SET_ADDRESS, CMD_SET_BUFFER (no reply), data + CRC (ACK),
 * then blCmd (CMD_PROG_FLASH or CMD_PROG_EEPROM) to commit it. ackMs is the commit timeout.
 */
bool blheli_write(uint8_t blCmd, uint16_t addr, const uint8_t *buf, uint16_t len, uint16_t ackMs)
{
    if (!bl_set_address(addr)) return false;
    uint8_t sb[4] = { CMD_SET_BUFFER, 0, (uint8_t)(len >> 8), (uint8_t)(len & 0xFF) };
    blheli_send_command(sb, 4);
    if (bl_get_ack(BL_T_NOACK_MS) != BR_NONE) return false;

    uint16_t crc = 0;
    esc_wire_tx_begin();
    for (uint16_t i = 0; i < len; i++) {
        esc_wire_tx_byte(buf[i]);
        crc = crc16_blheli_update(crc, buf[i]);
    }
    esc_wire_tx_byte(crc & 0xFF);
    esc_wire_tx_byte(crc >> 8);
    esc_wire_tx_end();
    if (bl_get_ack(BL_T_WRITE_MS) != BR_SUCCESS) return false;

    uint8_t c[2] = { blCmd, 0x01 };
    blheli_send_command(c, 2);
    return bl_get_ack(ackMs) == BR_SUCCESS;
}

/* CMD_RUN with RestartBootloader: soft-resets the ESC MCU, which then starts the application unless a new handshake arrives. */
void blheli_run(void)
{
    uint8_t c[2] = { CMD_RUN, BL_RestartBootloader };
    blheli_send_command(c, 2);
}

/* ===== 4w-if (PC link) ===== */

enum { S_START, S_CMD, S_AH, S_AL, S_LEN, S_DATA, S_CRCH, S_CRCL };

static uint8_t  g_buf[256];     /* request parameters in, response parameters out */
static uint16_t g_plen;         /* request parameter count (1..256) */
static uint8_t  g_cmd, g_ah, g_al;
static uint8_t  rxState = S_START;
static uint16_t rxIdx, rxCrc;
static uint8_t  rxCrcHi;
static uint32_t rxLastMs;
static uint8_t  g_mode = imSIL_BLB;

/*
 * Feed one byte from the PC into the frame parser.
 * Frame: 0x2F, cmd, addrHi, addrLo, paramLen (0 = 256), params, crcHi, crcLo (XMODEM over everything before the CRC).
 * Returns PARSE_OK when a frame completes with a valid CRC (fields in g_cmd/g_ah/g_al/g_buf/g_plen),
 * PARSE_BAD_CRC when it completes with an invalid CRC, otherwise PARSE_NONE.
 */
uint8_t parseMessage(uint8_t b)
{
    switch (rxState) {
    case S_START:
        if (b == FOURWAY_PC_START) { rxCrc = crc16_xmodem_update(0, b); rxState = S_CMD; }
        break;
    case S_CMD: g_cmd = b; rxCrc = crc16_xmodem_update(rxCrc, b); rxState = S_AH; break;
    case S_AH:  g_ah  = b; rxCrc = crc16_xmodem_update(rxCrc, b); rxState = S_AL; break;
    case S_AL:  g_al  = b; rxCrc = crc16_xmodem_update(rxCrc, b); rxState = S_LEN; break;
    case S_LEN:
        g_plen = b ? b : 256;
        rxCrc = crc16_xmodem_update(rxCrc, b);
        rxIdx = 0; rxState = S_DATA;
        break;
    case S_DATA:
        g_buf[rxIdx++] = b;
        rxCrc = crc16_xmodem_update(rxCrc, b);
        if (rxIdx >= g_plen) rxState = S_CRCH;
        break;
    case S_CRCH: rxCrcHi = b; rxState = S_CRCL; break;
    case S_CRCL:
        rxState = S_START;
        return (((uint16_t)rxCrcHi << 8 | b) == rxCrc) ? PARSE_OK : PARSE_BAD_CRC;
    }
    return PARSE_NONE;
}

/*
 * Send a response frame to the PC: 0x2E, cmd, addrHi, addrLo, paramLen (0 = 256), params, ack, crcHi, crcLo.
 * An empty parameter list is sent as a single 0x00 byte. The frame is built in one buffer and written in one call.
 */
void fourway_send_response(uint8_t cmd, uint16_t addr, const uint8_t *params, uint16_t n, uint8_t ack)
{
    static uint8_t frame[5 + 256 + 3];
    static const uint8_t zero = 0;
    if (n == 0 || params == NULL) { params = &zero; n = 1; }
    uint16_t k = 0;
    frame[k++] = FOURWAY_RX_START;
    frame[k++] = cmd;
    frame[k++] = (uint8_t)(addr >> 8);
    frame[k++] = (uint8_t)(addr & 0xFF);
    frame[k++] = (uint8_t)(n >= 256 ? 0 : n);
    memcpy(&frame[k], params, n);
    k += n;
    frame[k++] = ack;
    uint16_t crc = 0;
    for (uint16_t i = 0; i < k; i++) crc = crc16_xmodem_update(crc, frame[i]);
    frame[k++] = (uint8_t)(crc >> 8);
    frame[k++] = (uint8_t)(crc & 0xFF);
    Serial.write(frame, k);
}

/* Execute one validated 4w-if command: translate it to bootloader operations and send the response. */
void blheli_handle_command(uint8_t cmd, uint16_t addr, uint8_t *p, uint16_t plen)
{
    uint8_t ack = ACK_OK;
    const uint8_t *out = NULL;
    uint16_t outLen = 0;
    static uint8_t tmp[4];

    switch (cmd) {
    case cmd_InterfaceTestAlive:
        break;

    case cmd_ProtocolGetVersion:
        tmp[0] = FOURWAY_PROTOCOL_VER; out = tmp; outLen = 1;
        break;

    case cmd_InterfaceGetName:
        out = (const uint8_t *)FOURWAY_IF_NAME; outLen = sizeof(FOURWAY_IF_NAME) - 1;
        break;

    case cmd_InterfaceGetVersion:
        tmp[0] = FOURWAY_IF_VER_HI; tmp[1] = FOURWAY_IF_VER_LO; out = tmp; outLen = 2;
        break;

    case cmd_InterfaceExit:
        break;

    case cmd_InterfaceSetMode:
        if (p[0] == imSIL_BLB) g_mode = p[0]; else ack = ACK_I_INVALID_PARAM;
        break;

    case cmd_DeviceReset:
        if (p[0] != 0) { ack = ACK_I_INVALID_CHANNEL; break; }
        blheli_run();
        break;

    case cmd_DeviceInitFlash: {
        if (p[0] != 0) { ack = ACK_I_INVALID_CHANNEL; break; }      /* only channel 0 exists */
        blheli_info_t info;
        if (blheli_enter_bootloader(&info)) {
            tmp[0] = info.sigLo; tmp[1] = info.sigHi;                /* hosts read the signature as p[1]<<8 | p[0] */
            tmp[2] = info.bootChar; tmp[3] = g_mode;
            out = tmp; outLen = 4;
        } else ack = ACK_D_GENERAL_ERROR;
        break;
    }

    case cmd_DevicePageErase:
        if (!blheli_page_erase((uint16_t)p[0] << BL_PAGE_SHIFT)) ack = ACK_D_GENERAL_ERROR;
        break;

    case cmd_DeviceRead:
    case cmd_DeviceReadEEprom: {
        uint16_t len = p[0] ? p[0] : 256;
        uint8_t blCmd = (cmd == cmd_DeviceRead) ? CMD_READ_FLASH_SIL : CMD_READ_EEPROM;
        if (blheli_read(blCmd, addr, g_buf, len)) { out = g_buf; outLen = len; }
        else ack = ACK_D_GENERAL_ERROR;
        break;
    }

    case cmd_DeviceWrite:
        if (!blheli_write(CMD_PROG_FLASH, addr, p, plen, BL_T_WRITE_MS)) ack = ACK_D_GENERAL_ERROR;
        break;

    case cmd_DeviceWriteEEprom:
        if (!blheli_write(CMD_PROG_EEPROM, addr, p, plen, BL_T_EEWRITE_MS)) ack = ACK_D_GENERAL_ERROR;
        break;

    case cmd_DeviceEraseAll:
    case cmd_DeviceC2CK_LOW:
    case cmd_DeviceVerify:
    default:
        ack = ACK_I_INVALID_CMD;
        break;
    }
    fourway_send_response(cmd, addr, out, outLen, ack);
}

/* ===== Arduino entry points ===== */

/* Open the USB serial port and bring up the PIO signal-wire UART; blink the LED forever if no PIO resources are free. */
void setup()
{
    Serial.begin(PC_BAUD);
    if (!esc_wire_begin(ESC_SIGNAL_PIN)) {
        pinMode(LED_BUILTIN, OUTPUT);
        for (;;) {
            digitalWrite(LED_BUILTIN, HIGH); delay(100);
            digitalWrite(LED_BUILTIN, LOW);  delay(100);
        }
    }
}

/* Read PC bytes, run each complete frame, answer bad-CRC frames, and drop a frame that stalls mid-way. */
void loop()
{
    while (Serial.available() > 0) {
        uint8_t r = parseMessage((uint8_t)Serial.read());
        rxLastMs = millis();
        if (r == PARSE_OK) {
            blheli_handle_command(g_cmd, ((uint16_t)g_ah << 8) | g_al, g_buf, g_plen);
        } else if (r == PARSE_BAD_CRC) {
            fourway_send_response(g_cmd, ((uint16_t)g_ah << 8) | g_al, NULL, 0, ACK_I_INVALID_CRC);
        }
    }
    if (rxState != S_START && (millis() - rxLastMs) > FOURWAY_RX_TIMEOUT_MS)
        rxState = S_START;
}

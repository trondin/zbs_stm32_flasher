#!/usr/bin/env python3
"""ZBS Flasher

   Based on the original implementation by Aaron Christophel atc1441
"""


import os.path
import struct
import sys

import serial.tools.list_ports
from serial import SerialException

NUM_RETRIES = 3


class CommunicationError(Exception):
    pass


CMD_GET_VERSION = 1
CMD_RESET_ESP = 2
CMD_ZBS_BEGIN = 10
CMD_RESET_ZBS = 11
CMD_SELECT_PAGE = 12
CMD_SET_POWER = 13
CMD_READ_RAM = 20
CMD_WRITE_RAM = 21
CMD_READ_FLASH = 22
CMD_WRITE_FLASH = 23
CMD_READ_SFR = 24
CMD_WRITE_SFR = 25
CMD_ERASE_FLASH = 26
CMD_ERASE_INFOBLOCK = 27
CMD_SAVE_MAC_FROM_FW = 40
CMD_PASS_THROUGH = 50


class ZBSFlasher:
    flash_size = 0x10000
    info_size = 0x400

    def __init__(self, serial_port):
        self._serial = serial_port
        #self._slow_spi = slow_spi

    def _send_command(self, command, data=None):
        buffer = int(command).to_bytes(1, byteorder='big', signed=False)
        if data is not None:
            buffer += len(data).to_bytes(1, byteorder='big', signed=False)
            buffer += data
        else:
            buffer += b"\x00"
        crc_val = 0xAB34 + sum(buffer)
        buffer += int(crc_val & 0xffff).to_bytes(2, byteorder='big', signed=False)
        self._serial.write(b"AT" + buffer)

    def _read_response(self, expected_command):
        rx_crc = 0xAB34
        rx_buffer = []

        self._serial.read_until(b"AT")
        buf = self._serial.read(2)
        if len(buf) != 2:
            raise CommunicationError("timeout")
        rx_crc += sum(buf)
        command = buf[0]
        packet_len = int(buf[1])
        if packet_len:
            payload = self._serial.read(packet_len)
            rx_crc += sum(payload)
            rx_buffer.extend(payload)

        crc = self._serial.read(2)
        if rx_crc & 0xffff != crc[0] << 8 | crc[1]:
            raise CommunicationError("CRC Error")
        if expected_command != command:
            raise CommunicationError("command missmatch")
        return bytes(rx_buffer)

    def _query(self, command, data=None):
        for _ in range(NUM_RETRIES):
            self._send_command(command, data)
            return self._read_response(command)
        else:
            raise CommunicationError(f"Failed to communicate with the Flasher (command: {command} data: {data})")

    def flush(self):
        self._serial.flush()
        while self._serial.inWaiting():
            self._serial.read(self._serial.inWaiting())

    def init(self):
        slow_spi = 0
        if self._slow_spi:
            slow_spi = 1
        result = self._query(CMD_ZBS_BEGIN, bytearray([slow_spi]))
        if result[0] != 1:
            raise CommunicationError("Initialisation failed, is a device connected?")

    def version(self):
        return struct.unpack(">I", self._query(CMD_GET_VERSION))[0]

    def select_flash_page(self, page):
        self._query(CMD_SELECT_PAGE, bytearray([page & 1]))

    def read_flash(self, address, length):
        if address + length > self.flash_size:
            raise ValueError(f"Address out of bounds: {address} + {length} ")
        return self._query(
            CMD_READ_FLASH,
            bytearray([
                length,
                (address >> 8) & 0xff, address & 0xff
            ])
        )

    def write_flash(self, address, data):
        length = len(data)
        if length > 250:
            raise ValueError(f"Data is too big {length} > 250")
        if address + length > self.flash_size:
            raise ValueError(f"Address out of bounds: {address} + {length} ")
        response = self._query(
            CMD_WRITE_FLASH, bytearray([
                length,
                (address >> 8) & 0xff, address & 0xff
            ]) + data
        )
        if response[0] != 1:
            raise CommunicationError("Writing command returned an error")

    def erase_flash(self):
        return self._query(CMD_ERASE_FLASH)

    def erase_infopage(self):
        return self._query(CMD_ERASE_INFOBLOCK)

    def copy_mac_from_firmware(self):
        result = self._query(CMD_SAVE_MAC_FROM_FW)
        if result[0] != 1:
            raise CommunicationError("command failed")

    def reset(self):
        self._query(CMD_RESET_ZBS)

    def enter_passthrough_mode(self):
        self._query(CMD_PASS_THROUGH)
        self._serial.timeout = 0.5  # set the timeout to something low, so we don't lag too much.
        while True:
            try:
                data = self._serial.read(128)
                sys.stdout.buffer.write(data)
                sys.stdout.buffer.flush()
            except SerialException as e:
                return CommunicationError(str(e))



#def main(ctx, port, baudrate, slow):
def main():
    try:
        serial_port = serial.Serial(
            "/dev/ttyACM2",
            115200,
            serial.EIGHTBITS,
            serial.PARITY_NONE,
            serial.STOPBITS_ONE,
            timeout=2
        )
    except SerialException as e:
        raise click.ClickException(f"Could not open serial port: {str(e)}")
    #ctx.obj = ZBSFlasher(serial_port)
    
    ctx = ZBSFlasher(serial_port)
    ctx.flush()
    
    ctx.reset()



if __name__ == '__main__':
    main()

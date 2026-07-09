import threading
import time
from functools import reduce
from operator import xor

import serial
import serial.tools.list_ports

from config import SERIAL_BAUD, SERIAL_PORT, SERIAL_TIMEOUT

PROTO_START    = 0xAA
PROTO_STOP     = 0x55
PROTO_MAX_DATA = 57

_ser:  serial.Serial | None = None
_lock: threading.Lock        = threading.Lock()


def get_connection() -> serial.Serial:
    global _ser
    if _ser is None or not _ser.is_open:
        _ser = serial.Serial(SERIAL_PORT, SERIAL_BAUD, timeout=SERIAL_TIMEOUT)
    return _ser


def get_lock() -> threading.Lock:
    return _lock


def build_frame(data: bytes) -> bytes:
    parity = reduce(xor, data, 0)
    return bytes([PROTO_START, len(data)]) + data + bytes([parity, PROTO_STOP])


def read_frame(ser) -> bytes | None:
    deadline = time.time() + ser.timeout
    while time.time() < deadline:
        b = ser.read(1)
        if b and b[0] == PROTO_START:
            break
    else:
        return None

    b = ser.read(1)
    if not b:
        return None
    n = b[0]
    if n == 0 or n > PROTO_MAX_DATA:
        return None

    rest = ser.read(n + 2)
    if len(rest) != n + 2:
        return None

    data   = rest[:n]
    parity = rest[n]
    stop   = rest[n + 1]

    if stop != PROTO_STOP or reduce(xor, data, 0) != parity:
        return None

    return bytes(data)


def send_frame(payload: bytes) -> bytes | None:
    with get_lock():
        ser = get_connection()
        ser.reset_input_buffer()
        ser.write(build_frame(payload))
        return read_frame(ser)


def list_usb_ports() -> list[str]:
    """Portas serial USB conectadas (ignora as ttyS* nativas, que nao tem VID/PID)."""
    return [p.device for p in serial.tools.list_ports.comports() if p.vid is not None]


def probe_port(port: str, payload: bytes, timeout: float = SERIAL_TIMEOUT) -> bytes | None:
    """Abre uma conexao avulsa em `port`, envia o frame e le a resposta.
    Usado pra sondar varias portas sem depender da conexao unica global."""
    try:
        with serial.Serial(port, SERIAL_BAUD, timeout=timeout) as ser:
            ser.reset_input_buffer()
            ser.write(build_frame(payload))
            return read_frame(ser)
    except serial.SerialException:
        return None


_CLI_TAG = "[CLI]"


def read_text_line(ser) -> str | None:
    raw = ser.readline()
    if not raw:
        return None
    return raw.decode(errors="replace").rstrip("\r\n")


def send_and_stream_text(payload: bytes, max_messages: int) -> None:
    """Envia o frame e imprime as linhas de debug do HUB marcadas com [CLI].
    A UART1 é compartilhada com tráfego de outros módulos (rádio, status),
    então só o que carrega essa tag conta para `max_messages`."""
    with get_lock():
        ser = get_connection()
        ser.reset_input_buffer()
        ser.write(build_frame(payload))
        received = 0
        while received < max_messages:
            line = read_text_line(ser)
            if line is None or _CLI_TAG not in line:
                continue
            print(line.replace(_CLI_TAG, "", 1).strip())
            received += 1

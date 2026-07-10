import datetime

from scripts.protocol import list_usb_ports, probe_port, send_frame
from scripts.sync_rtc import _CMD_SET_TIME
from scripts.ui import choose, describe_node, labeled_row, mark, plain_row, print_box

_CMD_IDENTIFY = ord("I")
_CMD_STATUS   = ord("V")

_ADDR_CONTROLLER = 0x01

_PN532_FW_VER = 1
_PN532_FW_REV = 6
_CC1101_VERSION = 0x14

_SYNC_TOLERANCE_S = 5


def _is_synced(year: int, month: int, day: int, hours: int, minutes: int, seconds: int) -> bool:
    try:
        rtc_dt = datetime.datetime(year, month, day, hours, minutes, seconds)
    except ValueError:
        return False
    return abs((rtc_dt - datetime.datetime.now()).total_seconds()) <= _SYNC_TOLERANCE_S


def _render_controller(node_label: str, pn532_ok: bool, cc1101_ok: bool, data: bytes) -> bool:
    hours, minutes, seconds, day, month, year_2d, weekday, is_valid = data[5:13]
    year = 2000 + year_2d

    rtc_ok = bool(is_valid) and _is_synced(year, month, day, hours, minutes, seconds)

    date_str = f"{day:02d}/{month:02d}/{year:04d}"
    time_str = f"{hours:02d}:{minutes:02d}:{seconds:02d}"

    print_box(f"A.R.C.A. :: {node_label.upper()}", [
        labeled_row("NFC", pn532_ok),
        labeled_row("CC1101", cc1101_ok),
        labeled_row("RTC", rtc_ok),
        plain_row(f"{date_str}  {time_str}"),
    ])

    return pn532_ok and cc1101_ok


def _render_door(node_label: str, pn532_ok: bool, cc1101_ok: bool, data: bytes) -> bool:
    dht_ok, dht_temp, dht_humidity, press_ok, pressed = data[5:10]
    dht_ok   = bool(dht_ok)
    press_ok = bool(press_ok)

    press_reading = "Pressionado" if pressed else "Não pressionado"

    print_box(f"A.R.C.A. :: {node_label.upper()}", [
        labeled_row("NFC", pn532_ok),
        labeled_row("CC1101", cc1101_ok),
        labeled_row("DHT11", dht_ok),
        plain_row(f"{dht_temp}°C  {dht_humidity}%"),
        labeled_row("FORCA", press_ok),
        plain_row(press_reading),
    ])

    return pn532_ok and cc1101_ok and dht_ok and press_ok


def _sync_payload() -> bytes:
    now = datetime.datetime.now()
    return bytes([
        _CMD_SET_TIME,
        now.hour, now.minute, now.second,
        now.day, now.month, now.year - 2000,
        now.isoweekday(),
    ])


def _controller_needs_sync(data: bytes) -> bool:
    if len(data) < 13 or data[4] != _ADDR_CONTROLLER:
        return False
    hours, minutes, seconds, day, month, year_2d, weekday, is_valid = data[5:13]
    return not (bool(is_valid) and _is_synced(2000 + year_2d, month, day, hours, minutes, seconds))


def _render(data: bytes) -> bool:
    if len(data) < 5:
        print(f"{mark(False)} Resposta incompleta do dispositivo")
        return False

    pn532_present, pn532_ver, pn532_rev, cc1101_ver, addr = data[0:5]
    pn532_ok   = pn532_present == 1 and pn532_ver == _PN532_FW_VER and pn532_rev == _PN532_FW_REV
    cc1101_ok  = cc1101_ver == _CC1101_VERSION
    node_label = describe_node(addr)

    # o proprio endereco de origem (addr) diz se quem respondeu e o
    # Controller ou uma Door — cada um manda um payload diferente daqui pra frente
    if addr == _ADDR_CONTROLLER:
        if len(data) < 13:
            print(f"{mark(False)} Resposta incompleta do Controller")
            return False
        return _render_controller(node_label, pn532_ok, cc1101_ok, data)

    if len(data) < 10:
        print(f"{mark(False)} Resposta incompleta da Door")
        return False
    return _render_door(node_label, pn532_ok, cc1101_ok, data)


def check_status() -> bool:
    ports = list_usb_ports()

    # so uma (ou nenhuma) porta USB -> comportamento de sempre, sem menu
    if len(ports) <= 1:
        data = send_frame(bytes([_CMD_STATUS]))
        if data is None:
            print(f"{mark(False)} Sem resposta válida do dispositivo")
            return False
        if _controller_needs_sync(data):
            send_frame(_sync_payload())
            data = send_frame(bytes([_CMD_STATUS])) or data
        return _render(data)

    # varias portas conectadas -> so identifica quem e quem (comando leve,
    # sem ler sensores nem rodar a bateria de testes da Door)
    candidates = []
    for port in ports:
        ident = probe_port(port, bytes([_CMD_IDENTIFY]))
        if ident is not None and len(ident) >= 1:
            candidates.append((port, ident[0]))

    if not candidates:
        print(f"{mark(False)} Nenhum dispositivo respondeu")
        return False

    if len(candidates) == 1:
        port, _ = candidates[0]
    else:
        options = [f"{describe_node(addr)}  ({port})" for port, addr in candidates]
        idx = choose("Mais de um dispositivo conectado — selecione:", options)
        if idx is None:
            print("Cancelado.")
            return False
        port, _ = candidates[idx]

    # so agora, com o dispositivo confirmado, pede o status completo —
    # dados frescos e, na Door, dispara a bateria de testes
    data = probe_port(port, bytes([_CMD_STATUS]))
    if data is None:
        print(f"{mark(False)} Sem resposta válida do dispositivo")
        return False
    if _controller_needs_sync(data):
        probe_port(port, _sync_payload())
        data = probe_port(port, bytes([_CMD_STATUS])) or data
    return _render(data)


if __name__ == "__main__":
    check_status()

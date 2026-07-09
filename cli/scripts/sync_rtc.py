import datetime

from scripts.protocol import send_frame

_CMD_SET_TIME = 0x53  # 'S'


def sync_time() -> bool:
    now = datetime.datetime.now()
    payload = bytes([
        _CMD_SET_TIME,
        now.hour, now.minute, now.second,
        now.day, now.month, now.year - 2000,
        now.isoweekday(),
    ])
    data = send_frame(payload)
    return data is not None and len(data) >= 1 and data[0] == 0x06


if __name__ == "__main__":
    print("Sincronizando RTC com horário do sistema...")
    ok = sync_time()
    print("Sync", "OK" if ok else "FALHOU")

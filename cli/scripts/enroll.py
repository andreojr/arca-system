from scripts.protocol import send_and_stream_text

_CMD_ENROLL = ord("C")

# "[HUB] ENROLLING — aproxime o cartao (10s)" + resultado
# (cadastrado / ja cadastrado / erro / timeout -> IDLE)
_MAX_MESSAGES = 2


def request_enroll() -> None:
    send_and_stream_text(bytes([_CMD_ENROLL]), _MAX_MESSAGES)


if __name__ == "__main__":
    print("Enviando comando de cadastro...")
    request_enroll()

from scripts.protocol import send_and_stream_text

_CMD_DELETE = ord("D")

# "[HUB] DELETING — aproxime o cartao (10s)" + resultado
# (removido / nao encontrado / erro / timeout -> IDLE)
_MAX_MESSAGES = 2


def request_delete() -> None:
    send_and_stream_text(bytes([_CMD_DELETE]), _MAX_MESSAGES)


if __name__ == "__main__":
    print("Enviando comando de deleção...")
    request_delete()

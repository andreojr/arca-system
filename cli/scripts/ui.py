_GREEN_CHECK = "\033[32m✓\033[0m"
_RED_X       = "\033[31m✗\033[0m"

_ADDR_CONTROLLER = 0x01
_LABEL_WIDTH = 10  # rótulo + pontos, ex: "NFC......."

_BOX_TL, _BOX_TR = "╔", "╗"
_BOX_BL, _BOX_BR = "╚", "╝"
_BOX_ML, _BOX_MR = "╠", "╣"
_BOX_H, _BOX_V   = "═", "║"


def mark(ok: bool) -> str:
    return _GREEN_CHECK if ok else _RED_X


def describe_node(addr: int) -> str:
    if addr == _ADDR_CONTROLLER:
        return "Controller"
    room = (addr >> 4) & 0x0F
    side = "Externo" if (addr & 0x0F) == 1 else "Interno"
    return f"Sala {room} — {side}"


def labeled_row(label: str, ok: bool) -> tuple[str, str]:
    dots = "." * (_LABEL_WIDTH - len(label))
    plain = f"{label}{dots} {'✓' if ok else '✗'}"
    colored = f"{label}{dots} {mark(ok)}"
    return plain, colored


def plain_row(text: str) -> tuple[str, str]:
    return text, text


def print_box(title: str, rows: list[tuple[str, str]]) -> None:
    indent = "  "
    plain_rows   = [indent + plain for plain, _ in rows]
    colored_rows = [indent + colored for _, colored in rows]

    width = max(len(title), *(len(row) for row in plain_rows)) + 4

    print(_BOX_TL + _BOX_H * width + _BOX_TR)
    print(_BOX_V + title.center(width) + _BOX_V)
    print(_BOX_ML + _BOX_H * width + _BOX_MR)
    for plain, colored in zip(plain_rows, colored_rows):
        print(_BOX_V + colored + " " * (width - len(plain)) + _BOX_V)
    print(_BOX_BL + _BOX_H * width + _BOX_BR)


def _navigable_menu(title: str, options: list[str]) -> int | None:
    import curses

    def _run(stdscr) -> int | None:
        curses.curs_set(0)
        idx = 0
        while True:
            stdscr.clear()
            stdscr.addstr(0, 0, title)
            for i, opt in enumerate(options):
                prefix = "> " if i == idx else "  "
                attr = curses.A_REVERSE if i == idx else curses.A_NORMAL
                stdscr.addstr(i + 2, 0, f"{prefix}{opt}", attr)
            stdscr.refresh()

            key = stdscr.getch()
            if key in (curses.KEY_UP, ord("k")):
                idx = (idx - 1) % len(options)
            elif key in (curses.KEY_DOWN, ord("j")):
                idx = (idx + 1) % len(options)
            elif key in (curses.KEY_ENTER, 10, 13):
                return idx
            elif key == 27:  # Esc
                return None

    return curses.wrapper(_run)


def _numbered_menu(title: str, options: list[str]) -> int | None:
    print(title)
    for i, opt in enumerate(options, start=1):
        print(f"  {i}) {opt}")
    choice = input("Escolha (numero): ").strip()
    if choice.isdigit() and 1 <= int(choice) <= len(options):
        return int(choice) - 1
    return None


def choose(title: str, options: list[str]) -> int | None:
    """Menu navegavel (setas + Enter); cai pro modo numerado sem terminal interativo."""
    import sys

    if sys.stdout.isatty():
        try:
            return _navigable_menu(title, options)
        except Exception:
            pass
    return _numbered_menu(title, options)

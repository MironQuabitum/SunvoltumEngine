import asyncio
import random
import collections
import time

# ---------------------------------------------------------------------------
# Конфигурация
# ---------------------------------------------------------------------------
CLIENT_LISTEN_ADDR = ("127.0.0.1", 8888)
SERVER_ADDR        = ("127.0.0.1", 7777)

# ---------------------------------------------------------------------------
# Профили сети
#
# base_ms    — базовый RTT (туда+обратно), делится пополам для одного направления
# jitter_ms  — максимальный случайный разброс на пакет
# loss_pct   — вероятность потери пакета (0.0 – 1.0)
#
# Реальные данные (приблизительно):
#   WiFi 50m   — стены и расстояние дают 10-40 мс + заметный джиттер
#   WiFi 5m    — почти как LAN, 2-8 мс, минимальный джиттер
#   Europe→EU  — локальный дата-центр, <10 мс, стабильно
#   EU→NA      — транс-атлантика, ~120 мс базовый RTT
# ---------------------------------------------------------------------------
NETWORK_PROFILES = {
    # Имя              base_ms  jitter_ms  loss_pct
    "wifi_50m":        (35,     25,        0.02),
    "wifi_5m":         (5,      3,         0.005),
    "server_europe":   (8,      4,         0.001),
    "server_na":       (120,    15,        0.005),
}

# ---------------------------------------------------------------------------
# Активное состояние эмуляции
# ---------------------------------------------------------------------------
delay_ms   = 0      # базовая задержка в одну сторону (мс)
jitter_ms  = 0      # максимальный джиттер (мс)
loss_pct   = 0.0    # вероятность дропа пакета [0..1]


def _apply_profile(name: str) -> bool:
    """Применить профиль по имени. Возвращает False если имя неизвестно."""
    global delay_ms, jitter_ms, loss_pct
    if name not in NETWORK_PROFILES:
        return False
    base, jit, loss = NETWORK_PROFILES[name]
    # base_ms — это RTT; для одного направления берём половину
    delay_ms  = base // 2
    jitter_ms = jit
    loss_pct  = loss
    return True


def _reset():
    global delay_ms, jitter_ms, loss_pct
    delay_ms  = 0
    jitter_ms = 0
    loss_pct  = 0.0


# ---------------------------------------------------------------------------
# DelayQueue — буферизованная очередь с задержкой
#
# Каждый пакет кладётся в deque вместе с временем когда его можно отправить
# (now + delay + случайный джиттер). Фоновый цикл каждые 1 мс проверяет
# голову очереди и выпускает всё что созрело — FIFO, без лавин.
# ---------------------------------------------------------------------------
class DelayQueue:
    def __init__(self, name: str):
        self.name       = name
        self._queue     = collections.deque()   # [(send_at, data, addr)]
        self._transport = None

    def set_transport(self, transport):
        self._transport = transport

    def enqueue(self, data: bytes, addr: tuple):
        global delay_ms, jitter_ms, loss_pct

        # Потеря пакета
        if loss_pct > 0.0 and random.random() < loss_pct:
            return

        if delay_ms <= 0 and jitter_ms <= 0:
            # Без задержки — отправить немедленно
            if self._transport:
                self._transport.sendto(data, addr)
            return

        extra    = random.uniform(0, jitter_ms / 1000.0) if jitter_ms > 0 else 0.0
        send_at  = time.monotonic() + delay_ms / 1000.0 + extra
        self._queue.append((send_at, data, addr))

    async def flush_loop(self):
        while True:
            now = time.monotonic()
            while self._queue and self._queue[0][0] <= now:
                _, data, addr = self._queue.popleft()
                if self._transport:
                    self._transport.sendto(data, addr)
            await asyncio.sleep(0.001)

    def clear(self):
        self._queue.clear()


# Две очереди: клиент→сервер и сервер→клиент
queue_to_server = DelayQueue("->server")
queue_to_client = DelayQueue("->client")


# ---------------------------------------------------------------------------
# Протоколы
# ---------------------------------------------------------------------------
class ClientSideProtocol(asyncio.DatagramProtocol):
    def __init__(self):
        self.transport    = None
        self.client_addr  = None
        self.server_proto = None

    def connection_made(self, transport):
        self.transport = transport
        queue_to_client.set_transport(transport)
        print(f"[Proxy] Client side listening on {CLIENT_LISTEN_ADDR[0]}:{CLIENT_LISTEN_ADDR[1]}")

    def datagram_received(self, data, addr):
        if self.client_addr is None:
            self.client_addr = addr
            print(f"[Proxy] Client connected from {addr[0]}:{addr[1]}")
        queue_to_server.enqueue(bytes(data), SERVER_ADDR)

    def send_to_client(self, data: bytes):
        if self.client_addr:
            queue_to_client.enqueue(data, self.client_addr)


class ServerSideProtocol(asyncio.DatagramProtocol):
    def __init__(self, client_proto: ClientSideProtocol):
        self.transport    = None
        self.client_proto = client_proto

    def connection_made(self, transport):
        self.transport = transport
        queue_to_server.set_transport(transport)
        print(f"[Proxy] Server side socket ready -> {SERVER_ADDR[0]}:{SERVER_ADDR[1]}")

    def datagram_received(self, data, addr):
        self.client_proto.send_to_client(bytes(data))


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
HELP = """
Команды:
  AddPing <ms>              — задержка в одну сторону (мс)
  ClearPing                 — сбросить всё
  LoosePackageRate <pct>    — потери пакетов в % (например: 2.5)
  WifiWay <50m|5m|Xm>       — WiFi профиль по дистанции (любое число метров)
  ServerLocation <EU|NA|Xms>— дата-центр по имени или своя задержка в мс
  Status                    — текущие параметры
"""


def _print_status():
    print(f"  Задержка (1 сторона) : {delay_ms} ms")
    print(f"  Джиттер              : 0..{jitter_ms} ms")
    print(f"  Потери пакетов       : {loss_pct * 100:.1f}%")


def _apply_and_print(profile_key: str, label: str):
    if _apply_profile(profile_key):
        queue_to_server.clear()
        queue_to_client.clear()
        print(f"[+] Профиль: {label}")
        _print_status()
    else:
        print(f"[-] Неизвестный профиль: {profile_key}")


async def cli_loop():
    global delay_ms, jitter_ms, loss_pct

    print(HELP)

    loop = asyncio.get_running_loop()
    while True:
        user_input = await loop.run_in_executor(None, input, "> ")
        parts = user_input.strip().split()
        if not parts:
            continue

        cmd = parts[0].lower()

        # ---- AddPing -------------------------------------------------------
        if cmd == "addping":
            if len(parts) < 2:
                print("[-] Использование: AddPing <ms>")
                continue
            try:
                v = int(parts[1])
                if v < 0:
                    print("[-] Задержка не может быть отрицательной.")
                    continue
                delay_ms = v
                queue_to_server.clear()
                queue_to_client.clear()
                print(f"[+] Задержка: {delay_ms} ms")
                _print_status()
            except ValueError:
                print("[-] Некорректное число.")

        # ---- ClearPing -----------------------------------------------------
        elif cmd == "clearping":
            _reset()
            queue_to_server.clear()
            queue_to_client.clear()
            print("[+] Сброшено — без задержки, без потерь.")

        # ---- LoosePackageRate ----------------------------------------------
        elif cmd == "loosepackagerate":
            if len(parts) < 2:
                print("[-] Использование: LoosePackageRate <процент>  (например: 2.5)")
                continue
            try:
                pct = float(parts[1].replace(",", ".").rstrip("%"))
                if pct < 0 or pct > 100:
                    print("[-] Процент должен быть от 0 до 100.")
                    continue
                loss_pct = pct / 100.0
                print(f"[+] Потери пакетов: {pct:.1f}%")
                _print_status()
            except ValueError:
                print("[-] Некорректное число.")

        # ---- WifiWay -------------------------------------------------------
        elif cmd == "wifiway":
            if len(parts) < 2:
                print("[-] Использование: WifiWay <50m|5m|Xm>")
                continue
            arg = parts[1].lower().replace("m", "").strip()
            # Предустановки
            if arg == "50":
                _apply_and_print("wifi_50m", "WiFi 50m (медленный, много стен)")
            elif arg == "5":
                _apply_and_print("wifi_5m", "WiFi 5m (быстрый, прямая видимость)")
            else:
                # Свободный ввод: линейная интерполяция по дистанции
                # 1m → ~2ms base, ~1ms jitter, 0.1% loss
                # 100m → ~60ms base, ~40ms jitter, 5% loss
                try:
                    dist = float(arg)
                    if dist <= 0:
                        print("[-] Дистанция должна быть больше 0.")
                        continue
                    t         = min(dist / 100.0, 1.0)   # нормализуем к [0..1] на 100m
                    delay_ms  = int(2  + t * 58)          # 2..60 ms
                    jitter_ms = int(1  + t * 39)          # 1..40 ms
                    loss_pct  = 0.001 + t * 0.049         # 0.1%..5%
                    queue_to_server.clear()
                    queue_to_client.clear()
                    print(f"[+] WifiWay {dist:.0f}m (свободный профиль)")
                    _print_status()
                except ValueError:
                    print("[-] Некорректное значение. Примеры: WifiWay 50m  WifiWay 15m")

        # ---- ServerLocation ------------------------------------------------
        elif cmd == "serverlocation":
            if len(parts) < 2:
                print("[-] Использование: ServerLocation <EU|NA|Xms>")
                continue
            arg = parts[1].upper().replace("MS", "").strip()
            if arg in ("EU", "EUROPE"):
                _apply_and_print("server_europe", "Server — Europe (локально)")
            elif arg in ("NA", "NORTHAMERICA", "NORTH_AMERICA"):
                _apply_and_print("server_na", "Server — North America (транс-атлантика)")
            else:
                # Свободный ввод в мс (RTT)
                try:
                    rtt = float(parts[1].lower().replace("ms", "").replace(",", "."))
                    if rtt < 0:
                        print("[-] Задержка не может быть отрицательной.")
                        continue
                    delay_ms  = int(rtt / 2)              # RTT → одна сторона
                    jitter_ms = max(1, int(rtt * 0.08))   # ~8% от RTT
                    loss_pct  = 0.001 + min(rtt / 500.0 * 0.01, 0.02)  # до 2%
                    queue_to_server.clear()
                    queue_to_client.clear()
                    print(f"[+] ServerLocation {rtt:.0f}ms RTT (свободный профиль)")
                    _print_status()
                except ValueError:
                    print("[-] Некорректное значение. Примеры: ServerLocation EU  ServerLocation 80ms")

        # ---- Status --------------------------------------------------------
        elif cmd == "status":
            _print_status()

        else:
            print(f"[-] Неизвестная команда: {parts[0]}")
            print(HELP)


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------
async def main():
    loop = asyncio.get_running_loop()

    client_proto = ClientSideProtocol()
    server_proto = ServerSideProtocol(client_proto)
    client_proto.server_proto = server_proto

    await loop.create_datagram_endpoint(
        lambda: client_proto,
        local_addr=CLIENT_LISTEN_ADDR
    )
    await loop.create_datagram_endpoint(
        lambda: server_proto,
        local_addr=("127.0.0.1", 0)
    )

    print(f"[Proxy] Ready: Client={CLIENT_LISTEN_ADDR[1]}  Server={SERVER_ADDR[1]}")

    asyncio.ensure_future(queue_to_server.flush_loop())
    asyncio.ensure_future(queue_to_client.flush_loop())

    await cli_loop()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n[Proxy] Остановлен.")

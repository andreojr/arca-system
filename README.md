# A.R.C.A. — Access and Room Control with Authentication

Sistema embarcado de controle de acesso e registro de presença em salas, desenvolvido como projeto acadêmico. O foco é demonstrar a **integração de múltiplos sensores no STM32 e a comunicação entre módulos via rádio frequência (CC1101)**.

---

## Arquitetura

O sistema é composto por três tipos de módulos. Para cada sala existem dois módulos transmissores (externo e interno), e um único **Controlador Central** que gerencia todas as salas.

```
[Sala 1 - Módulo Externo] ─┐
[Sala 1 - Módulo Interno] ─┤
[Sala 2 - Módulo Externo] ─┼──► [Controlador Central]
[Sala 2 - Módulo Interno] ─┘
```

Os módulos externo e interno **não se comunicam entre si** — ambos falam exclusivamente com o Controlador Central via CC1101.

O firmware é **único** para todos os módulos. O tipo de nó é selecionado em tempo de compilação pela flag `-DMY_ADDR=0xXX`, que define internamente `MODULE_CONTROLLER` ou `MODULE_TRANSMITTER`.

---

## Hardware

| Componente | Externo | Interno | Controlador |
|---|:---:|:---:|:---:|
| STM32F411 / STM32F401 | ✅ | ✅ | ✅ |
| CC1101 (RF 433 MHz) | ✅ | ✅ | ✅ |
| NFC PN532 | ✅ | ✅ | ✅ |
| DHT11 (temperatura/umidade) | ✅ | ✅ | ❌ |
| Sensor de força (entrada da porta) | ✅ | ✅ | ❌ |
| LED azul (NFC ativo) | ✅ | ✅ | ✅ |
| LED verde (acesso liberado) | ✅ | ✅ | ❌ |
| LED vermelho (acesso negado) | ✅ | ✅ | ❌ |
| LED porta — open-drain compartilhado | ✅ | ✅ | ❌ |
| Buzzer — open-drain compartilhado | ✅ | ✅ | ❌ |
| Display LCD MSP2402 | ❌ | ❌ | ✅ |
| Memória Flash interna | ❌ | ❌ | ✅ |
| Serial CP2102 (UART → debug) | ❌ | ❌ | ✅ |

---

## Estrutura do Repositório

```
arca/
└── Core/
    ├── Inc/
    │   ├── app.h                            # Entry points da aplicação
    │   └── Modules/
    │       ├── config.h                     # Endereços de rede, eventos e seleção de módulo
    │       ├── dispatcher.h                 # Roteamento de eventos RF
    │       ├── hub.h                        # Máquina de estados do controlador
    │       ├── door.h                       # Máquina de estados da porta
    │       ├── flash_db.h                   # Banco de dados em flash
    │       ├── dht11.h                      # Driver DHT11
    │       ├── press.h                      # Sensor de força (ADC)
    │       ├── Communication/
    │       │   ├── cc1101.h                 # Driver CC1101 (registradores, SPI)
    │       │   └── com_module.h             # Protocolo de pacotes RF
    │       └── Nfc/
    │           ├── pn532.h                  # Driver PN532 (MIFARE, NTAG2xx)
    │           └── pn532_stm32f4.h          # HAL layer STM32 (SPI, IRQ, bit-reversal)
    └── Src/
        ├── app.c                            # Inicialização e loop principal da aplicação
        ├── main.c                           # Inicialização HAL, periféricos e ISRs
        └── Modules/
            ├── dispatcher.c                 # Roteamento de eventos RF
            ├── hub.c                        # Validação de acesso e cadastro
            ├── door.c                       # Lógica de autorização e abertura
            ├── flash_db.c                   # Leitura/escrita na flash interna
            ├── dht11.c                      # Driver DHT11 (OneWire)
            ├── press.c                      # Leitura do sensor de força via ADC
            ├── Communication/
            │   ├── cc1101.c                 # Driver CC1101
            │   └── com_module.c             # Protocolo RF (TX/RX, callbacks)
            └── Nfc/
                ├── pn532.c                  # Driver PN532
                └── pn532_stm32f4.c          # Adaptador STM32 para PN532
```

---

## Protocolo de Comunicação RF

Cada pacote tem o formato:

```
[ DST : 1B ] [ EVENT : 1B ] [ SRC : 1B ] [ PAYLOAD : 0–59B ]
```

### Endereços de Rede (`config.h`)

| Endereço | Nó |
|---|---|
| `0x00` | BROADCAST |
| `0x01` | Controlador Central |
| `0x11` | Sala 1 — Módulo Externo (entrada) |
| `0x12` | Sala 1 — Módulo Interno (saída) |
| `0x21` | Sala 2 — Módulo Externo (entrada) |
| `0x22` | Sala 2 — Módulo Interno (saída) |

> Os transmissores alternam o próprio endereço entre `0x?1` e `0x?2` após cada abertura de porta, registrando automaticamente o sentido (entrada/saída).

### Eventos

| Código | Evento | Direção | Descrição |
|---|---|---|---|
| `0x01` | `EVENT_AUTHORIZE` | Transmissor → Controlador | Solicita autorização (envia UID) |
| `0x02` | `EVENT_ACCESS_GRANTED` | Controlador → Transmissor | Acesso liberado |
| `0x03` | `EVENT_ACCESS_DENIED` | Controlador → Transmissor | Acesso negado |
| `0x04` | `EVENT_STATUS_REQUEST` | Controlador → Transmissor | Solicita dados ambientais |
| `0x05` | `EVENT_STATUS_RESPONSE` | Transmissor → Controlador | Responde com temperatura/umidade/porta |
| `0x06` | `EVENT_ACCESS_CONFIRMED` | Transmissor → Controlador | Porta foi fisicamente aberta |

### Fluxo de Autorização

```
Transmissor                          Controlador
    │                                     │
    │── EVENT_AUTHORIZE (uid) ──────────►│
    │                                     │  valida UID na flash
    │◄── EVENT_ACCESS_GRANTED ───────────│  (se autorizado)
    │                                     │
    │  [sensor de força detecta abertura] │
    │                                     │
    │── EVENT_ACCESS_CONFIRMED ─────────►│
    │                                     │  registra log na flash
```

---

## Máquinas de Estado

### Módulo de Porta (`door.c`)

```
IDLE ──(NFC lido)──► VALIDATING ──(ACCESS_GRANTED)──► UNLOCKED ──(força detectada)──► OPEN
  ▲                      │                                │                              │
  │                      │(timeout 2s)                    │(timeout 5s)                  │(timeout 5s)
  └──────────────────────┴────────────────────────────────┴──────────────────────────────┘
                                                                              │(porta aberta > 5s)
                                                                           [buzzer]
```

| Estado | Duração | Descrição |
|---|---|---|
| `DOOR_IDLE` | — | Aguardando cartão NFC |
| `DOOR_VALIDATING` | 2 s | Aguarda resposta do controlador via RF |
| `DOOR_UNLOCKED` | 5 s | Acesso liberado (LED verde), aguarda sensor de força |
| `DOOR_OPEN` | 5 s | Porta aberta; buzzer dispara se não fechar no prazo |
| `DOOR_DENIED` | 1 s | Acesso negado (LED vermelho + buzzer) |

### Controlador (`hub.c`)

| Estado | Trigger | Descrição |
|---|---|---|
| `HUB_IDLE` | — | Processa requisições de autorização via RF |
| `HUB_ENROLLING` | UART `'C'` | Aguarda cartão NFC para cadastro (timeout 10 s) |
| `HUB_DELETING` | UART `'D'` | Aguarda cartão NFC para remoção (timeout 10 s) |

---

## Banco de Dados em Flash (`flash_db.c`)

Utiliza dois setores da flash interna do STM32F411:

| Setor | Endereço | Conteúdo | Capacidade |
|---|---|---|---|
| Setor 6 | `0x08040000` | Tabela de usuários | 4096 registros |
| Setor 7 | `0x08060000` | Log de acessos | 4096 registros |

### Registro de Usuário (`FlashUser_t` — 32 bytes)

```c
uint8_t uid[7];       // UID do cartão NFC
uint8_t uid_len;      // Comprimento do UID
char    name[21];     // Nome do titular
uint8_t access_lvl;  // Nível de acesso (reservado)
uint8_t valid;        // 0xFF=vazio, 0x01=ativo, 0x00=removido (soft-delete)
```

### Registro de Log (`FlashLog_t` — 32 bytes)

```c
uint32_t timestamp;   // Timestamp do evento (RTC — integração pendente)
uint8_t  uid[7];      // UID do cartão
uint8_t  uid_len;
uint8_t  room_id;     // Sala onde ocorreu o evento
uint8_t  direction;   // 1 = entrada, 2 = saída
```

---

## Periféricos (STM32F411)

| Periférico | Pino | Função |
|---|---|---|
| SPI1 | PA5/PA6/PA7 | CC1101 + PN532 (NSS por software) |
| UART1 | PA9/PA10 | Debug (printf) + comandos do controlador |
| ADC1 CH4 | PA4 | Sensor de força (limiar 2000, debounce 50 ms) |
| RTC | — | Timestamp de log (LSE 32.768 kHz, inicializado) |
| GPIO PB9 | LED_GRANTED | LED verde (acesso liberado) |
| GPIO PB8 | LED_DENIED | LED vermelho (acesso negado) |
| GPIO PB7 | BUZZER | Buzzer (acesso negado / porta aberta) |
| GPIO PB6 | DHT11_DATA | Sensor de temperatura/umidade (open-drain) |
| GPIO PA1 | NFC_INT | Interrupção de cartão NFC (falling edge) |
| GPIO PB12 | INT_CC1101 | Interrupção RX do CC1101 (falling edge) |

Clock do sistema: **84 MHz** (HSI × PLL).

---

## Status de Implementação

### Implementado

- [x] Driver CC1101 completo (registradores, SPI burst, strobe, RX/TX)
- [x] Protocolo de pacotes RF com callbacks
- [x] Driver PN532 completo (SPI, MIFARE Classic, NTAG2xx, bit-reversal LSB)
- [x] Detecção de cartão NFC via IRQ
- [x] Driver DHT11 completo (OneWire, validação de checksum, timeout)
- [x] Sensor de força via ADC (PA4, limiar configurável, debounce 50 ms)
- [x] LEDs de feedback (verde/vermelho) nos transmissores
- [x] Buzzer acionado em acesso negado e porta aberta além do timeout
- [x] Máquina de estados do módulo de porta (5 estados, 4 timeouts)
- [x] Máquina de estados do controlador (cadastro/remoção via UART)
- [x] Banco de dados de usuários na flash interna (soft-delete, 4096 slots)
- [x] Log de acessos na flash interna (4096 entradas)
- [x] `EVENT_ACCESS_CONFIRMED` — transmissor informa controlador que a porta foi efetivamente aberta
- [x] Alternância de endereço entrada↔saída após abertura de porta
- [x] Dispatcher de eventos RF (controller e transmitter)
- [x] Debug via UART1 (printf redirecionado)

### Pendente / Em desenvolvimento

- [ ] **Display LCD MSP2402** — nenhum driver implementado; será a interface de gerenciamento do controlador central
- [ ] **RTC no log de acesso** — periférico inicializado, mas o timestamp real ainda não é lido e preenchido em `FlashDB_LogAdd`
- [ ] **Validação por nível de acesso** — campo `access_lvl` existe na estrutura, lógica de comparação com o nível da sala não implementada
- [ ] **Status periódico** — `HUB_RequestStatus` é chamado uma vez na inicialização; polling contínuo por `HAL_GetTick` não implementado
- [ ] **Exportação de logs via UART** — escrita na flash implementada; leitura e envio dos registros para PC via serial não implementada

---

## Build

O projeto usa **STM32CubeMX** para geração de código HAL e **CMake** para build.

O tipo de módulo é definido pela flag `MY_ADDR` passada ao compilador:

| `MY_ADDR` | Módulo compilado |
|---|---|
| `0x01` | Controlador Central |
| `0x11` | Sala 1 — Módulo Externo |
| `0x12` | Sala 1 — Módulo Interno |
| `0x21` | Sala 2 — Módulo Externo |
| `0x22` | Sala 2 — Módulo Interno |

```bash
# Exemplo: compilar como Controlador Central
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DMY_ADDR=0x01
cmake --build build

# Exemplo: compilar como Sala 1 - Módulo Externo
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DMY_ADDR=0x11
cmake --build build
```

**Toolchain:** `arm-none-eabi-gcc`
**Targets:** STM32F411CEUx (próprio) / STM32F401 (professor)

> O arquivo `.ioc` é único para todos os módulos. Gere o código separadamente no STM32CubeMX para F401 e F411 conforme necessário.

---

## Equipe

Projeto acadêmico — Laboratório de Informática Industrial II

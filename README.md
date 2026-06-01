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
├── shared/                          # Drivers e protocolo compartilhados
│   └── Core/
│       ├── Inc/Modules/
│       │   ├── Communication/
│       │   │   ├── cc1101.h         # Driver CC1101 (registradores, SPI)
│       │   │   └── com_module.h     # Protocolo de pacotes RF
│       │   ├── Nfc/
│       │   │   ├── pn532.h          # Driver PN532 (MIFARE, NTAG2xx)
│       │   │   ├── pn532_stm32f4.h  # HAL layer STM32 (SPI, IRQ, bit-reversal)
│       │   │   └── nfc_module.h     # Abstração de alto nível
│       │   └── config.h             # Endereços de rede e eventos
│       └── Src/Modules/             # Implementações correspondentes
│
├── controller/                      # Firmware — Controlador Central
│   └── Core/
│       ├── Inc/Modules/
│       │   ├── hub.h                # Máquina de estados do controlador
│       │   └── flash_db.h           # Banco de dados em flash
│       └── Src/Modules/
│           ├── dispatcher.c         # Roteamento de eventos RF
│           ├── hub.c                # Validação de acesso e cadastro
│           ├── flash_db.c           # Leitura/escrita na flash interna
│           └── main.c               # Inicialização e loop principal
│
└── transmitter/                     # Firmware — Módulo de Porta
    └── Core/
        ├── Inc/Modules/
        │   └── door.h               # Máquina de estados da porta
        └── Src/Modules/
            ├── dispatcher.c         # Roteamento de eventos RF
            ├── door.c               # Lógica de autorização e abertura
            └── main.c               # Inicialização e loop principal
```

---

## Protocolo de Comunicação RF

Cada pacote tem o formato:

```
[ SRC : 1B ] [ DST : 1B ] [ EVENT : 1B ] [ PAYLOAD : 0–8B ]
```

### Endereços de Rede (`config.h`)

| Endereço | Nó |
|---|---|
| `0x00` | BROADCAST |
| `0x01` | Controlador Central |
| `0x11` | Sala 1 — Módulo Externo |
| `0x12` | Sala 1 — Módulo Interno |
| `0x21` | Sala 2 — Módulo Externo |
| `0x22` | Sala 2 — Módulo Interno |

### Eventos

| Código | Evento | Descrição |
|---|---|---|
| `0x01` | `EVENT_AUTHORIZE` | Transmissor solicita autorização (envia UID) |
| `0x02` | `EVENT_ACCESS_GRANTED` | Controlador libera acesso |
| `0x03` | `EVENT_ACCESS_DENIED` | Controlador nega acesso |
| `0x04` | `EVENT_STATUS_REQUEST` | Controlador solicita dados ambientais |
| `0x05` | `EVENT_STATUS_RESPONSE` | Transmissor responde com temperatura/umidade |

### Fluxo de Autorização

```
Transmissor                          Controlador
    │                                     │
    │── EVENT_AUTHORIZE (uid) ──────────►│
    │                                     │  valida UID na flash
    │◄── EVENT_ACCESS_GRANTED ───────────│  (se autorizado)
    │    (controlador ignora se negado)   │
```

---

## Máquinas de Estado

### Módulo de Porta (`door.c`)

```
IDLE ──(NFC lido)──► VALIDATING ──(ACCESS_GRANTED)──► UNLOCKED ──(força detectada)──► OPEN
  ▲                      │                                │                              │
  │                      │(timeout 2s)                    │(timeout 5s)                  │(timeout 5s)
  └──────────────────────┴────────────────────────────────┴──────────────────────────────┘
```

| Estado | Duração | Descrição |
|---|---|---|
| `IDLE` | — | Aguardando cartão NFC (LED azul aceso) |
| `VALIDATING` | 2 s | Aguarda resposta do controlador via RF |
| `UNLOCKED` | 5 s | Acesso liberado, aguarda sensor de força |
| `OPEN` | 5 s | Porta aberta, entrada/saída registrada |
| `DENIED` | 1 s | Acesso negado (LED vermelho + buzzer) |

### Controlador (`hub.c`)

| Estado | Trigger | Descrição |
|---|---|---|
| `HUB_IDLE` | — | Processa requisições de autorização via RF |
| `HUB_ENROLLING` | UART `'C'` | Aguarda cartão NFC para cadastro (timeout 10 s) |
| `HUB_DELETING` | UART `'D'` | Aguarda cartão NFC para remoção (timeout 10 s) |

---

## Banco de Dados em Flash (`flash_db.c`)

Utiliza dois setores da flash interna do STM32F411:

| Setor | Endereço | Conteúdo |
|---|---|---|
| Setor 6 | `0x08040000` | Tabela de usuários (128 KB) |
| Setor 7 | `0x08060000` | Log de acessos (128 KB) |

### Registro de Usuário (`FlashUser_t`)

```c
uint8_t uid[7];       // UID do cartão NFC
uint8_t uid_len;      // Comprimento do UID
char    name[21];     // Nome do titular
uint8_t access_lvl;  // Nível de acesso (sistema S.H.I.E.L.D.)
uint8_t valid;        // 0xFF=vazio, 0x01=ativo, 0x00=removido (soft-delete)
```

### Registro de Log (`FlashLog_t`)

```c
uint32_t timestamp;   // Timestamp do evento (RTC)
uint8_t  uid[7];      // UID do cartão
uint8_t  uid_len;
uint8_t  room_id;     // Sala onde ocorreu o evento
uint8_t  direction;   // Entrada ou saída
```

---

## Status de Implementação

### Implementado

- [x] Driver CC1101 completo (registradores, SPI burst, strobe, RX/TX)
- [x] Protocolo de pacotes RF com callbacks
- [x] Driver PN532 completo (SPI, MIFARE Classic, NTAG2xx, bit-reversal LSB)
- [x] Detecção de cartão NFC via IRQ
- [x] Máquina de estados do módulo de porta (5 estados, 4 timeouts)
- [x] Máquina de estados do controlador (cadastro/remoção via UART)
- [x] Banco de dados de usuários na flash interna (soft-delete)
- [x] Log de acessos na flash interna
- [x] Dispatcher de eventos RF (controller e transmitter)
- [x] Debug via UART1 (printf redirecionado)

### Pendente / Em desenvolvimento

- [ ] **DHT11** — integração do driver (código disponível em repositório da equipe, pendente de portagem para este projeto)
- [ ] **Sensor de força** — integração do driver (código disponível em repositório da equipe, pendente de portagem para este projeto)
- [ ] **LEDs** — lógica de acionamento dos LEDs (azul/verde/vermelho/porta) nos módulos transmissores; atualmente sem implementação de GPIO para feedback visual
- [ ] **Display LCD MSP2402** — nenhum driver implementado; será a interface de gerenciamento do controlador central
- [ ] **RTC** — leitura do timestamp real para preenchimento do campo `timestamp` no log de acesso (periférico inicializado, integração com `FlashDB_LogAdd` pendente)
- [ ] **Registro de acesso na abertura da porta** — o log deve ser gravado no momento em que o sensor de força detecta a abertura (estado `OPEN`), não na concessão do acesso; requer novo evento RF do transmissor para o controlador informando que a porta foi efetivamente aberta
- [ ] **Buzzer por porta aberta** — acionar buzzer compartilhado (open-drain) se o sensor de força indicar porta aberta além do timeout do estado `OPEN`
- [ ] **Validação por nível de acesso** — estrutura de dados existe (`access_lvl`), lógica de comparação com o nível da sala não implementada

### Adicional

- [ ] **Exportação de logs via UART** — escrita na flash implementada; leitura e envio dos registros para PC via serial não implementada

---

## Build

O projeto usa **STM32CubeMX** para geração de código HAL e **CMake** para build.

```bash
# Controller
cd controller
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Transmitter
cd transmitter
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

**Toolchain:** `arm-none-eabi-gcc`
**Targets:** STM32F411CEUx (próprio) / STM32F401 (professor)

> O arquivo `.ioc` é único para todos os módulos. Gere o código separadamente no STM32CubeMX para F401 e F411 conforme necessário.

---

## Equipe

Projeto acadêmico — Laboratório de Informática Industrial II

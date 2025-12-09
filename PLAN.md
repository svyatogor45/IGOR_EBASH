# План разработки Arbi45 Terminal v1

## Обзор

Арбитражный терминал для торговли на фьючерсных рынках USDT-M.

**Стек технологий:**
- Backend: Go
- Frontend: Svelte (SvelteKit)
- База данных: SQLite
- Деплой: systemd + nginx

**Поддерживаемые биржи:** BYBIT, BINGX, BITGET, GATE, OKX, HTX

---

## Phase 0: Базовый скелет Backend

### Цель
Создать основу проекта с конфигурацией, логированием и HTTP-сервером.

### Задачи

#### 0.1 Структура проекта
```
arbi45/
├── cmd/
│   └── server/
│       └── main.go
├── internal/
│   ├── config/
│   ├── logger/
│   └── api/
├── configs/
│   └── config.yaml
├── data/
├── web/
└── deploy/
```

#### 0.2 Config Manager
- Файл: `internal/config/config.go`
- Загрузка YAML-конфигурации
- Параметры:
  - HTTP порт
  - Логин/пароль веб-интерфейса
  - Лимиты (max_pairs: 30, max_active: 10)
  - Путь к БД

#### 0.3 Logger
- Файл: `internal/logger/logger.go`
- Файл: `internal/logger/format.go`
- Уровни: DEBUG, INFO, WARN, ERROR
- Форматы логов согласно ТЗ (секции "Торговля" и "ERROR")
- Вывод в stdout + файл

**Примеры форматов логов из ТЗ:**
```
// Секция "Торговля":
[2024-01-15 14:30:22] BTCUSDT открыт цикл #42
[2024-01-15 14:30:22] BTCUSDT ордера 1/3 открыты: BYBIT Long 0.5 @ 42150.5, BINGX Short 0.5 @ 42165.2
[2024-01-15 14:35:18] BTCUSDT ордера 1/3 закрыты: BYBIT 0.5 @ 42160.0, BINGX 0.5 @ 42145.0, PNL: +2.35
[2024-01-15 14:40:00] BTCUSDT цикл #42 закрыт, Total PNL: +7.05

// Секция "ERROR":
[2024-01-15 14:30:23] ERROR BTCUSDT: асимметрия ног - Long исполнен, Short отклонён
[2024-01-15 14:30:24] BTCUSDT: принудительное закрытие Long позиции
```

#### 0.4 HTTP Server
- Файл: `internal/api/server.go`
- Базовый HTTP-сервер на стандартной библиотеке или chi/gin
- Health-check эндпоинт: `GET /api/health`
- CORS настройки

### Тестирование Phase 0
- [ ] Unit-тест загрузки конфигурации
- [ ] Unit-тест форматирования логов
- [ ] Интеграционный тест запуска сервера

### Критерий завершения
- Сервер запускается, читает конфиг, пишет логи
- `curl http://localhost:8080/api/health` возвращает 200

---

## Phase 1: База данных

### Цель
Реализовать слой работы с БД и все репозитории.

### Задачи

#### 1.1 SQLite подключение
- Файл: `internal/db/sqlite.go`
- Подключение к файлу `data/arbi45.db`
- Настройка WAL-режима для производительности

#### 1.2 Миграции
- Файл: `internal/db/migrations.go`
- Папка: `internal/db/migrations/`
- Файл: `internal/db/migrations/001_init.sql`

```sql
-- exchanges
CREATE TABLE exchanges (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    status TEXT NOT NULL DEFAULT 'DISCONNECTED',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- api_keys
CREATE TABLE api_keys (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    exchange_id TEXT NOT NULL,
    api_key TEXT NOT NULL,
    secret_key TEXT NOT NULL,
    passphrase TEXT,
    status TEXT NOT NULL DEFAULT 'ACTIVE',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (exchange_id) REFERENCES exchanges(id)
);

-- pairs
CREATE TABLE pairs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    symbol TEXT NOT NULL,
    volume REAL NOT NULL,
    entry_spread REAL NOT NULL,
    exit_spread REAL NOT NULL,
    num_orders INTEGER NOT NULL DEFAULT 1,
    status TEXT NOT NULL DEFAULT 'PAUSE',
    total_cycles INTEGER DEFAULT 0,
    total_pnl REAL DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- cycles (арбитражные циклы)
CREATE TABLE cycles (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    pair_id INTEGER NOT NULL,
    status TEXT NOT NULL DEFAULT 'OPEN',
    total_volume REAL DEFAULT 0,
    total_pnl REAL DEFAULT 0,
    started_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    closed_at DATETIME,
    FOREIGN KEY (pair_id) REFERENCES pairs(id)
);

-- parts (части ордеров)
CREATE TABLE parts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    cycle_id INTEGER NOT NULL,
    part_number INTEGER NOT NULL,
    volume REAL NOT NULL,
    long_exchange TEXT NOT NULL,
    short_exchange TEXT NOT NULL,
    long_volume REAL,
    short_volume REAL,
    long_entry_price REAL,
    short_entry_price REAL,
    long_exit_price REAL,
    short_exit_price REAL,
    status TEXT NOT NULL DEFAULT 'PENDING',
    error_reason TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (cycle_id) REFERENCES cycles(id)
);

-- positions
CREATE TABLE positions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    exchange_id TEXT NOT NULL,
    symbol TEXT NOT NULL,
    side TEXT NOT NULL,
    volume REAL NOT NULL,
    entry_price REAL NOT NULL,
    status TEXT NOT NULL DEFAULT 'OPEN',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (exchange_id) REFERENCES exchanges(id)
);

-- balances
CREATE TABLE balances (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    exchange_id TEXT NOT NULL,
    available_margin REAL NOT NULL,
    used_margin REAL DEFAULT 0,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (exchange_id) REFERENCES exchanges(id)
);

-- logs
CREATE TABLE logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    type TEXT NOT NULL,
    message TEXT NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Индексы
CREATE INDEX idx_pairs_status ON pairs(status);
CREATE INDEX idx_cycles_pair_id ON cycles(pair_id);
CREATE INDEX idx_cycles_status ON cycles(status);
CREATE INDEX idx_parts_cycle_id ON parts(cycle_id);
CREATE INDEX idx_logs_type ON logs(type);
CREATE INDEX idx_logs_created_at ON logs(created_at);
```

#### 1.3 Репозитории
- `internal/db/repositories/exchanges.go`
- `internal/db/repositories/api_keys.go`
- `internal/db/repositories/pairs.go`
- `internal/db/repositories/cycles.go`
- `internal/db/repositories/parts.go`
- `internal/db/repositories/positions.go`
- `internal/db/repositories/balances.go`
- `internal/db/repositories/logs.go`

Каждый репозиторий реализует CRUD операции.

### Тестирование Phase 1
- [ ] Unit-тесты для каждого репозитория
- [ ] Тест миграций (создание/откат)
- [ ] Тест транзакций

### Критерий завершения
- Миграции применяются успешно
- Все CRUD операции работают
- Тесты проходят

---

## Phase 2: Exchange Layer

### Цель
Реализовать унифицированный интерфейс для работы с 6 биржами.

### Задачи

#### 2.1 Типы и интерфейсы
- Файл: `internal/exchange/types.go`

```go
type Order struct {
    ID           string
    Exchange     string
    Symbol       string
    Side         string    // BUY, SELL
    Type         string    // MARKET
    Volume       float64
    FilledVolume float64
    AvgPrice     float64
    Status       string
    CreatedAt    time.Time
}

type Position struct {
    Exchange   string
    Symbol     string
    Side       string    // LONG, SHORT
    Volume     float64
    EntryPrice float64
    UnrealizedPNL float64
}

type Balance struct {
    Exchange        string
    AvailableMargin float64
    UsedMargin      float64
}

type OrderBook struct {
    Exchange string
    Symbol   string
    Bids     []PriceLevel
    Asks     []PriceLevel
    UpdatedAt time.Time
}

type PriceLevel struct {
    Price  float64
    Volume float64
}

type Ticker struct {
    Exchange  string
    Symbol    string
    BestBid   float64
    BestAsk   float64
    UpdatedAt time.Time
}
```

#### 2.2 Exchange Manager
- Файл: `internal/exchange/manager.go`

```go
type ExchangeAdapter interface {
    // REST API
    Connect(apiKey, secret, passphrase string) error
    Disconnect() error
    GetBalance() (*Balance, error)
    GetPositions(symbol string) ([]Position, error)
    GetOpenOrders(symbol string) ([]Order, error)  // Для синхронизации при рестарте
    PlaceMarketOrder(symbol, side string, volume float64) (*Order, error)
    ClosePosition(symbol, side string, volume float64) (*Order, error)

    // WebSocket
    SubscribeOrderBook(symbol string, callback func(*OrderBook)) error
    SubscribeTicker(symbol string, callback func(*Ticker)) error
    Unsubscribe(symbol string) error

    // Utils
    FormatSymbol(symbol string) string
    FormatVolume(symbol string, volume float64) float64
    GetFees() (maker, taker float64)
}

// Биржи, требующие passphrase при подключении:
// - OKX: требует passphrase
// - BingX: требует passphrase
// Остальные биржи (BYBIT, BITGET, GATE, HTX): passphrase не требуется

type Manager struct {
    adapters map[string]ExchangeAdapter
    // ...
}
```

#### 2.3 Унификация символов
- Файл: `internal/exchange/symbols.go`
- Маппинг: `BTCUSDT` → формат конкретной биржи
- Обратный маппинг

#### 2.4 Форматирование объёмов
- Файл: `internal/exchange/volumes.go`
- Округление до требуемой точности биржи
- Минимальные объёмы

#### 2.5 Rate Limiting
- Файл: `internal/exchange/ratelimit.go`
- Лимиты запросов для каждой биржи
- Token bucket или leaky bucket

#### 2.6 Синхронизация с биржами
- Файл: `internal/exchange/sync.go`
- Периодический запрос позиций и балансов
- Сверка с внутренним состоянием

#### 2.7 Адаптеры бирж
- `internal/exchange/adapters/bybit.go`
- `internal/exchange/adapters/bingx.go`
- `internal/exchange/adapters/bitget.go`
- `internal/exchange/adapters/gate.go`
- `internal/exchange/adapters/okx.go`
- `internal/exchange/adapters/htx.go`

Каждый адаптер реализует `ExchangeAdapter` интерфейс.

#### 2.8 Комиссии бирж
- Файл: `internal/exchange/fees.go`
- Taker fees для каждой биржи (используется в CleanSpread)

| Биржа  | Taker Fee |
|--------|-----------|
| BYBIT  | 0.055%    |
| BINGX  | 0.05%     |
| BITGET | 0.06%     |
| GATE   | 0.05%     |
| OKX    | 0.05%     |
| HTX    | 0.05%     |

### Тестирование Phase 2
- [ ] Mock-тесты адаптеров
- [ ] Unit-тест rate limiter
- [ ] Unit-тест унификации символов
- [ ] Интеграционные тесты с testnet (опционально)

### Критерий завершения
- Все 6 адаптеров реализованы
- Можно получить баланс с реальной биржи
- Rate limiting работает

---

## Phase 3: Market Data Engine

### Цель
Реализовать получение и хранение рыночных данных в реальном времени.

### Задачи

#### 3.1 Market Data Engine
- Файл: `internal/market/engine.go`

```go
type Engine struct {
    exchangeManager *exchange.Manager
    orderBooks      map[string]map[string]*OrderBook  // exchange -> symbol -> book
    tickers         map[string]map[string]*Ticker
    subscribers     []chan MarketUpdate
    mu              sync.RWMutex
}

func (e *Engine) Subscribe(symbol string) error
func (e *Engine) Unsubscribe(symbol string) error
func (e *Engine) GetOrderBook(exchange, symbol string) *OrderBook
func (e *Engine) GetTicker(exchange, symbol string) *Ticker
func (e *Engine) GetBestBidAsk(symbol string) map[string]Ticker
```

#### 3.2 Order Book
- Файл: `internal/market/orderbook.go`
- Хранение первых 10 уровней
- Атомарные обновления
- Timestamp последнего обновления

#### 3.3 Ticker
- Файл: `internal/market/ticker.go`
- Best bid / best ask
- Быстрый доступ для фильтра спреда

#### 3.4 VWAP Calculator
- Файл: `internal/market/vwap.go`

```go
// CalculateVWAP рассчитывает средневзвешенную цену для заданного объёма
func CalculateVWAP(book *OrderBook, side string, volume float64) (float64, bool) {
    // side: "BUY" использует asks, "SELL" использует bids
    // Возвращает VWAP и флаг достаточности ликвидности
}
```

#### 3.5 WebSocket Reconnection
- Файл: `internal/market/reconnect.go`
- Автоматическое переподключение при обрыве
- Таймаут переподключения: 10 секунд
- Exponential backoff

**ВАЖНО (согласно ТЗ):** Поведение при потере соединения зависит от статуса пары:

**Если пара в статусе READY (без открытых позиций):**
- Новые части не открываются
- Пара остаётся в READY
- Торговых действий не предпринимается до восстановления связи
- Опционально: предупреждение в секцию «ERROR»

**Если пара в статусе TRADE (есть открытые позиции):**
- Пара **остаётся в статусе TRADE** (НЕ переводится в ERROR!)
- Активные позиции **НЕ закрываются автоматически**
- Терминал ожидает восстановления соединения

**После восстановления связи терминал:**
1. Актуализирует данные о позициях и ценах
2. Пересчитывает текущий спред выхода
3. Если спред выхода удовлетворяет условиям → пытается закрыть позицию
4. Если спред не удовлетворяет → позиция продолжается, ожидание условий

**Риски:** Пользователь осознанно принимает риск убытков при резких движениях рынка во время потери связи. Терминал v1 не ограничивает максимальный убыток.

### Тестирование Phase 3
- [ ] Unit-тест VWAP расчёта
- [ ] Unit-тест обновления стакана
- [ ] Тест reconnection логики

### Критерий завершения
- Получение стаканов по WebSocket работает
- VWAP считается корректно
- Reconnection работает

---

## Phase 4: State Manager + PNL

### Цель
Реализовать управление состоянием пар и расчёт PNL.

### Задачи

#### 4.1 State Manager
- Файл: `internal/state/manager.go`

```go
type Manager struct {
    pairs    map[int]*TradingPair
    cycles   map[int]*ArbitrageCycle
    db       *db.Repository
    mu       sync.RWMutex
}

func (m *Manager) GetPair(id int) *TradingPair
func (m *Manager) UpdatePairStatus(id int, status string) error
func (m *Manager) GetActivePairs() []*TradingPair
func (m *Manager) GetPairsInStatus(status string) []*TradingPair
```

#### 4.2 Trading Pair
- Файл: `internal/state/pair.go`

```go
type TradingPair struct {
    ID          int
    Symbol      string
    Volume      float64
    EntrySpread float64
    ExitSpread  float64
    NumOrders   int
    Status      string  // PAUSE, READY, TRADE, ERROR

    // Runtime
    CurrentCycle *ArbitrageCycle
    mu           sync.RWMutex
}
```

#### 4.3 Status Transitions
- Файл: `internal/state/status.go`

```go
// Валидные переходы статусов
var ValidTransitions = map[string][]string{
    "PAUSE": {"READY"},
    "READY": {"PAUSE", "TRADE", "ERROR"},
    "TRADE": {"READY", "PAUSE", "ERROR"},
    "ERROR": {"PAUSE"},
}

func (m *Manager) TransitionStatus(pairID int, newStatus string) error {
    // Валидация перехода
    //
    // Особые случаи:
    // 1. PAUSE -> READY: Проверить, что нет открытых позиций по паре на биржах
    //    - Если есть позиции → перевести в ERROR, а не в READY
    // 2. TRADE -> PAUSE: Закрыть все открытые позиции перед переходом
}

// CheckPositionsBeforeReady проверяет отсутствие позиций перед переходом в READY
func (m *Manager) CheckPositionsBeforeReady(pair *TradingPair) error {
    // Запросить позиции по символу со всех подключённых бирж
    // Если есть открытые позиции → вернуть ошибку, установить ERROR
}

// ClosePositionsOnPause закрывает все позиции при переходе TRADE -> PAUSE
func (m *Manager) ClosePositionsOnPause(pair *TradingPair) error {
    // При нажатии Pause из статуса TRADE:
    // 1. Закрыть все открытые позиции по паре
    // 2. Рассчитать PNL
    // 3. Записать лог
    // 4. Только после этого перевести в PAUSE
}
```

#### 4.4 Arbitrage Cycle
- Файл: `internal/state/cycle.go`

```go
type ArbitrageCycle struct {
    ID           int
    PairID       int
    Status       string  // OPEN, CLOSED, ERROR
    Parts        []*Part
    TotalVolume  float64
    TotalPNL     float64
    StartedAt    time.Time
    ClosedAt     *time.Time
}

type Part struct {
    Number         int
    Volume         float64
    LongExchange   string
    ShortExchange  string
    LongVolume     float64
    ShortVolume    float64
    LongEntryPrice float64
    ShortEntryPrice float64
    LongExitPrice  float64
    ShortExitPrice float64
    Status         string  // PENDING, OPENED, CLOSED, ERROR
}
```

#### 4.5 PNL Calculator
- Файл: `internal/pnl/calculator.go`

```go
func CalculatePartPNL(part *Part, fees float64) float64 {
    // PNL = (ShortEntry - ShortExit) * ShortVolume + (LongExit - LongEntry) * LongVolume - Fees
}

func CalculateCyclePNL(cycle *ArbitrageCycle) float64 {
    // Сумма PNL всех частей
}
```

#### 4.6 Fees Integration
- Файл: `internal/pnl/fees.go`
- Расчёт комиссий на основе exchange fees
- Учёт taker fee для обеих ног

### Тестирование Phase 4
- [ ] Unit-тест переходов статусов
- [ ] Unit-тест расчёта PNL
- [ ] Unit-тест расчёта комиссий

### Критерий завершения
- Статусы переключаются корректно
- PNL считается с учётом комиссий
- Состояние синхронизируется с БД

---

## Phase 5: Trading Engine

### Цель
Реализовать основную торговую логику арбитража.

### Задачи

#### 5.1 Trading Engine
- Файл: `internal/trading/engine.go`

```go
type Engine struct {
    stateManager    *state.Manager
    marketEngine    *market.Engine
    exchangeManager *exchange.Manager
    pnlCalculator   *pnl.Calculator
    logger          *logger.Logger

    stopCh chan struct{}
}

func (e *Engine) Start() error
func (e *Engine) Stop() error
func (e *Engine) ProcessPair(pair *state.TradingPair) error
```

#### 5.2 Spread Calculator
- Файл: `internal/trading/spread.go`

```go
// PreSpread - быстрый фильтр по best bid/ask
func CalculatePreSpread(bestBid, bestAsk float64) float64 {
    return (bestBid - bestAsk) / bestAsk * 100
}

// CleanSpread - точный расчёт по VWAP с учётом комиссий
func CalculateCleanSpread(vwapLong, vwapShort, fees float64) float64 {
    return (vwapShort - vwapLong) / vwapLong * 100 - fees
}
```

#### 5.3 Liquidity Check
- Файл: `internal/trading/liquidity.go`

```go
func CheckLiquidity(
    longBook, shortBook *market.OrderBook,
    volume float64,
    maxSlippage float64,
) (bool, float64, float64) {
    // Проверяет, можно ли исполнить объём без превышения VWAP slippage
    // Возвращает: ok, longVWAP, shortVWAP
}
```

#### 5.4 Margin Check
- Файл: `internal/trading/margin.go`

```go
func CheckMargin(
    longExchange, shortExchange string,
    volume, price float64,
    balances map[string]*exchange.Balance,
) bool {
    // Проверяет достаточность маржи на обеих биржах
}
```

#### 5.5 Entry Logic
- Файл: `internal/trading/entry.go`

```go
func (e *Engine) TryEnterPart(pair *state.TradingPair, partNum int) error {
    // 1. Рассчитать PreSpread
    // 2. Если PreSpread < EntrySpread -> skip
    // 3. Определить биржи (max bid = Short, min ask = Long)
    // 4. Рассчитать CleanSpread по VWAP для V_part
    // 5. Проверить ликвидность
    // 6. Проверить маржу
    // 7. Отправить два рыночных ордера одновременно
    // 8. Проверить симметрию исполнения
    // 9. Обновить состояние
}
```

#### 5.6 Exit Logic
- Файл: `internal/trading/exit.go`

```go
func (e *Engine) TryExitPart(pair *state.TradingPair, part *state.Part) error {
    // Зеркальная логика входа, НО:
    // ВАЖНО: Проверка маржи при выходе НЕ выполняется (согласно ТЗ)
    //
    // 1. Рассчитать спред выхода по VWAP
    // 2. Проверить ликвидность стаканов
    // 3. НЕ проверять маржу (в отличие от входа)
    // 4. Отправить ордера на закрытие
    // 5. Рассчитать PNL части
}
```

#### 5.7 Error Handling (Leg Asymmetry)
- Файл: `internal/trading/errors.go`

```go
// Константы для проверки симметрии исполнения
const (
    MaxVolumeDeviation = 0.01  // Максимальное отклонение объёмов: 1%
)

func (e *Engine) HandleLegAsymmetry(
    pair *state.TradingPair,
    executedLeg *exchange.Order,
    failedExchange string,
) error {
    // 1. Немедленно закрыть исполненную ногу
    // 2. Перевести пару в ERROR
    // 3. Записать лог в секцию ERROR
}

// CheckVolumeSymmetry проверяет симметрию исполненных объёмов
func CheckVolumeSymmetry(longVolume, shortVolume float64) bool {
    // Отклонение не должно превышать MaxVolumeDeviation (1%)
    deviation := math.Abs(longVolume-shortVolume) / math.Max(longVolume, shortVolume)
    return deviation <= MaxVolumeDeviation
}
```

#### 5.8 Partial Fills Handling
- Файл: `internal/trading/partialfills.go`

```go
func (e *Engine) HandlePartialFill(order *exchange.Order) error {
    // Обработка частичного исполнения ордеров от биржи
    // 1. Сравнить FilledVolume с запрошенным Volume
    // 2. Если partial fill - учесть реальный исполненный объём
    // 3. Проверить симметрию с другой ногой
}
```

#### 5.9 Recovery After Restart
- Файл: `internal/trading/recovery.go`

```go
func (e *Engine) RecoverAfterRestart() error {
    // 1. Загрузить все пары из БД
    // 2. Перевести все пары в PAUSE
    // 3. Запросить позиции со всех бирж (GetPositions + GetOpenOrders)
    // 4. Для пар с открытыми позициями:
    //    - Принудительно закрыть позиции
    //    - Записать лог
    // 5. Синхронизировать состояние
}
```

#### 5.10 Periodic Sync
- Файл: `internal/trading/sync.go`

```go
func (e *Engine) StartPeriodicSync(interval time.Duration) {
    // Периодическая синхронизация состояния с биржами во время торговли
    // 1. Запросить реальные позиции с бирж
    // 2. Сверить с внутренним состоянием
    // 3. При расхождении - логировать и при необходимости корректировать
}
```

### Тестирование Phase 5
- [ ] Unit-тест расчёта спредов
- [ ] Unit-тест проверки ликвидности
- [ ] Unit-тест логики входа (с mock биржами)
- [ ] Unit-тест обработки асимметрии
- [ ] Интеграционный тест полного цикла

### Критерий завершения
- Торговый движок работает в event loop
- Вход/выход частями работает
- Асимметрия ног обрабатывается корректно
- Recovery при рестарте работает

---

## Phase 6: API Layer

### Цель
Реализовать REST и WebSocket API для веб-интерфейса.

### Задачи

#### 6.1 Auth Middleware
- Файл: `internal/api/auth.go`
- Файл: `internal/api/middleware.go`

```go
func AuthMiddleware(config *config.Config) func(http.Handler) http.Handler {
    return func(next http.Handler) http.Handler {
        // Проверка JWT токена или session
    }
}

func (s *Server) Login(w http.ResponseWriter, r *http.Request) {
    // POST /api/login
    // Проверить логин/пароль из конфига
    // Вернуть JWT токен
}
```

#### 6.2 Validators
- Файл: `internal/api/validators.go`

```go
func ValidatePairInput(input *PairInput) error {
    // - Symbol не пустой
    // - Volume > 0
    // - EntrySpread > 0
    // - EntrySpread > ExitSpread
    // - NumOrders от 1 до 10
}

func ValidateExchangeDisconnect(exchangeID string, pairs []*state.TradingPair) error {
    // Проверка: нельзя отключить биржу, если существует пара в статусе TRADE
    // которая использует эту биржу
    for _, pair := range pairs {
        if pair.Status == "TRADE" {
            // Проверить, использует ли пара эту биржу
            return errors.New("cannot disable exchange while TRADE pair exists")
        }
    }
    return nil
}
```

#### 6.3 Handlers
- `internal/api/handlers/pairs.go`

```go
// GET /api/pairs - список всех пар
// POST /api/pairs - добавить пару
// DELETE /api/pairs/:id - удалить пару
// POST /api/pairs/:id/start - запустить пару (PAUSE -> READY)
// POST /api/pairs/:id/pause - остановить пару
// POST /api/pairs/:id/reset - сбросить ошибку (ERROR -> PAUSE)
```

- `internal/api/handlers/exchanges.go`

```go
// GET /api/exchanges - список бирж с балансами
// POST /api/exchanges/:id/connect - подключить биржу (ввод API ключей)
// POST /api/exchanges/:id/disconnect - отключить биржу
// DELETE /api/exchanges/:id/keys - удалить API ключи
```

- `internal/api/handlers/actions.go`

```go
// Общие действия
```

- `internal/api/handlers/logs.go`

```go
// GET /api/logs?type=TRADE&limit=100 - получить логи
// GET /api/logs?type=ERROR&limit=100
```

#### 6.4 WebSocket Hub
- Файл: `internal/api/ws/hub.go`
- Файл: `internal/api/ws/types.go`

```go
type Hub struct {
    clients    map[*Client]bool
    broadcast  chan Message
    register   chan *Client
    unregister chan *Client
}

type Message struct {
    Type    string      `json:"type"`
    Payload interface{} `json:"payload"`
}

// Message types:
// - pair_status_changed
// - balance_updated
// - new_log
// - cycle_opened
// - cycle_closed
// - part_executed
```

### Тестирование Phase 6
- [ ] Unit-тесты валидаторов
- [ ] Integration тесты API эндпоинтов
- [ ] Тест WebSocket подключения

### Критерий завершения
- Все REST эндпоинты работают
- Авторизация работает
- WebSocket отправляет обновления в реальном времени

---

## Phase 7: Frontend (Svelte)

### Цель
Реализовать веб-интерфейс на SvelteKit.

### Задачи

#### 7.1 Структура проекта

```
web/
├── src/
│   ├── routes/
│   │   ├── +layout.svelte
│   │   ├── +page.svelte          # Redirect to /bot
│   │   ├── login/
│   │   │   └── +page.svelte
│   │   ├── bot/
│   │   │   └── +page.svelte
│   │   ├── connections/
│   │   │   └── +page.svelte
│   │   └── logs/
│   │       └── +page.svelte
│   ├── lib/
│   │   ├── components/
│   │   │   ├── ExchangeList.svelte
│   │   │   ├── ExchangeBalance.svelte
│   │   │   ├── ExchangeCard.svelte
│   │   │   ├── PairList.svelte
│   │   │   ├── PairCard.svelte
│   │   │   ├── AddPairModal.svelte
│   │   │   ├── ApiKeyForm.svelte
│   │   │   ├── LogsPanel.svelte
│   │   │   ├── LogsTabs.svelte
│   │   │   └── StatusBadge.svelte
│   │   ├── stores/
│   │   │   ├── auth.ts
│   │   │   ├── pairs.ts
│   │   │   ├── exchanges.ts
│   │   │   └── logs.ts
│   │   ├── api/
│   │   │   ├── client.ts
│   │   │   └── websocket.ts
│   │   ├── validators/
│   │   │   └── pair.ts
│   │   ├── types.ts
│   │   └── config.ts
│   └── app.html
├── static/
├── svelte.config.js
├── vite.config.ts
└── package.json
```

#### 7.2 Вкладка "Бот"
- Список подключенных бирж с балансами
- Кнопка "Добавить торговую пару"
- Список торговых пар:
  - Статус (цветной badge)
  - Параметры (symbol, volume, spreads, orders)
  - Статистика (cycles, PNL)
  - Кнопки управления (Start/Pause/Reset/Delete)

#### 7.3 Вкладка "Подключения"
- Список всех 6 бирж
- Для каждой биржи:
  - Статус подключения
  - Баланс (если подключена)
  - Форма ввода API ключей
  - Кнопки: Включить/Отключить/Удалить ключи

#### 7.4 Вкладка "Логи"
- Две секции: "Торговля" и "ERROR"
- Автоскролл новых логов
- Фильтрация по дате

#### 7.5 WebSocket Integration
- Автоматическое обновление данных
- Reconnection при обрыве связи

### Тестирование Phase 7
- [ ] Тест рендеринга компонентов
- [ ] E2E тесты основных сценариев

### Критерий завершения
- Все 3 вкладки работают
- Данные обновляются в реальном времени
- Авторизация работает

---

## Phase 8: Deploy & Documentation

### Цель
Подготовить систему к production-деплою.

### Задачи

#### 8.1 Systemd Service
- Файл: `deploy/arbi45.service`

```ini
[Unit]
Description=Arbi45 Trading Terminal
After=network.target

[Service]
Type=simple
User=arbi45
WorkingDirectory=/opt/arbi45
ExecStart=/opt/arbi45/arbi45-server
Restart=always
RestartSec=5
Environment=CONFIG_PATH=/opt/arbi45/configs/config.yaml

[Install]
WantedBy=multi-user.target
```

#### 8.2 Nginx Config
- Файл: `deploy/nginx.conf`

```nginx
server {
    listen 80;
    server_name _;

    location / {
        root /opt/arbi45/web/build;
        try_files $uri $uri/ /index.html;
    }

    location /api {
        proxy_pass http://127.0.0.1:8080;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
    }

    location /ws {
        proxy_pass http://127.0.0.1:8080;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
    }
}
```

#### 8.3 Install Script
- Файл: `deploy/install.sh`

```bash
#!/bin/bash
set -e

# Создание пользователя
useradd -r -s /bin/false arbi45 || true

# Создание директорий
mkdir -p /opt/arbi45/{configs,data,web/build}

# Копирование файлов
cp arbi45-server /opt/arbi45/
cp -r web/build/* /opt/arbi45/web/build/
cp configs/config.yaml /opt/arbi45/configs/

# Права
chown -R arbi45:arbi45 /opt/arbi45

# Systemd
cp deploy/arbi45.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable arbi45
systemctl start arbi45

# Nginx
cp deploy/nginx.conf /etc/nginx/sites-available/arbi45
ln -sf /etc/nginx/sites-available/arbi45 /etc/nginx/sites-enabled/
systemctl reload nginx

echo "Installation complete!"
```

#### 8.4 Build Script
- Файл: `scripts/build.sh`

```bash
#!/bin/bash
set -e

# Build backend
cd cmd/server
go build -o ../../arbi45-server .
cd ../..

# Build frontend
cd web
npm install
npm run build
cd ..

echo "Build complete!"
```

#### 8.5 Документация
- `README.md` - краткое описание и quick start
- `docs/INSTALL.md` - инструкция по установке
- `docs/CONFIG.md` - описание конфигурации
- `docs/API.md` - документация API

### Тестирование Phase 8
- [ ] Тест установки на чистую Ubuntu VPS
- [ ] Тест автозапуска после reboot
- [ ] Тест nginx проксирования

### Критерий завершения
- Система устанавливается одним скриптом
- Автозапуск работает
- Документация написана

---

## Зависимости между фазами

```
Phase 0 (Skeleton)
    │
    ▼
Phase 1 (Database)
    │
    ▼
Phase 2 (Exchange) ──────┐
    │                    │
    ▼                    ▼
Phase 3 (Market) ───► Phase 4 (State + PNL)
    │                    │
    └────────┬───────────┘
             │
             ▼
      Phase 5 (Trading)
             │
             ▼
      Phase 6 (API)
             │
             ▼
      Phase 7 (Frontend)
             │
             ▼
      Phase 8 (Deploy)
```

---

## Сценарии взаимодействия компонентов

### Сценарий «Добавление торговой пары»

1. Пользователь в Frontend → заполняет форму → «Добавить торговую пару»
2. Frontend отправляет запрос в Backend (REST)
3. Backend (API) → валидирует данные → вызывает DB Layer → создаёт запись о паре в статусе PAUSE
4. State Manager обновляет внутренний список пар
5. Frontend получает обновлённый список пар через WebSocket и отображает новую пару

### Сценарий «Старт пары» (PAUSE → READY)

1. Пользователь нажимает «Старт» в интерфейсе
2. Frontend отправляет запрос в Backend
3. Backend через State Manager:
   - Проверяет отсутствие открытых позиций на биржах по этой паре
   - Если есть позиции → ERROR, если нет → меняет статус на READY
4. Market Data Engine начинает WebSocket-подписки и мониторинг по паре
5. Trading Engine включает пару в цикл обработки решений
6. Статус и кнопки обновляются через WebSocket

### Сценарий «Вход в арбитраж»

1. Trading Engine получает от Market Data Engine данные (best bid/ask, стаканы)
2. Выполняет:
   - Быстрый фильтр PreSpread
   - Расчёт VWAP и CleanSpread по объёму части
   - Проверку ликвидности и маржи
3. При выполнении всех условий:
   - Формирует команду на вход
   - Через Exchange Manager отправляет два рыночных ордера одновременно
4. Exchange Manager возвращает информацию об исполнении
5. Trading Engine и PNL Calculator:
   - Проверяют симметрию исполнения (отклонение < 1%)
   - Фиксируют вход части
   - Обновляют цикл в БД
6. Logging пишет запись в секцию «Торговля»
7. WebSocket отправляет обновление во Frontend

### Сценарий «Ошибка одной ноги»

1. Exchange Manager сообщает: одна нога исполнена, вторая — нет
2. Trading Engine:
   - Немедленно отдаёт команду Exchange Manager на закрытие исполненной ноги
   - Через State Manager переводит пару в ERROR
3. Logging пишет запись в секцию «ERROR»
4. WebSocket отправляет обновление статуса
5. Frontend показывает пользователю кнопку «Сброс»

### Сценарий «Сброс» (ERROR → PAUSE)

1. Пользователь нажимает кнопку «Сброс»
2. Frontend → запрос в Backend
3. Backend через Exchange Manager:
   - Принудительно закрывает все открытые позиции по паре
   - Ожидает подтверждения закрытия
4. State Manager переводит пару в PAUSE
5. Logging пишет лог о принудительном закрытии
6. Frontend отображает обновлённое состояние

### Сценарий «Пауза из TRADE»

1. Пользователь нажимает «Пауза» при активной позиции
2. Backend через Exchange Manager:
   - Закрывает все открытые позиции по паре рыночными ордерами
   - Ожидает исполнения на обеих биржах
3. PNL Calculator рассчитывает итоговый PNL цикла
4. State Manager переводит пару в PAUSE только после закрытия всех позиций
5. Logging записывает завершение цикла

---

## Модель конкурентности

### Асинхронная событийная модель (Event Loop)

Терминал использует асинхронную модель выполнения на базе goroutines в Go:

```go
// Основные goroutines:
// 1. WebSocket receivers - по одной на каждое соединение с биржей
// 2. Market Data processor - обработка входящих данных стаканов
// 3. Trading Engine loop - основной цикл принятия торговых решений
// 4. API server - обработка HTTP/WebSocket запросов от Frontend
// 5. Periodic sync - периодическая синхронизация с биржами
```

### Принципы конкурентного доступа

1. **State Manager** — единый источник правды, защищён `sync.RWMutex`
2. **Market Data** — lock-free структуры для стаканов где возможно
3. **Exchange Manager** — отдельные goroutines для каждой биржи
4. **Каналы (channels)** — для передачи событий между компонентами

### Требования к неблокирующей работе

- Основной trading loop не должен блокироваться на I/O
- WebSocket приём данных в отдельных goroutines
- REST запросы к биржам с таймаутами
- БД операции не должны блокировать trading logic

---

## Логика продолжения набора позиции

### Ситуация: условия для следующей части не выполняются

Если после открытия одной или нескольких частей:
- Спред входа становится меньше заданного, или
- Ликвидности в стакане недостаточно для V_part, или
- Маржи недостаточно

**Поведение терминала:**
1. Дальнейший набор прекращается
2. Терминал НЕ пытается открыть следующие части
3. Уже набранные части продолжают удерживаться как действующая позиция
4. Пара **остаётся в статусе TRADE** (не ERROR!)
5. Терминал ждёт выполнения условий выхода

**Важно:** Статус ERROR НЕ выставляется, так как текущая позиция симметрична и корректно открыта. Это штатная ситуация, а не ошибка.

### Возобновление набора

Если условия снова станут благоприятными (спред, ликвидность, маржа), терминал может продолжить набор оставшихся частей до полного объёма.

---

## Чек-лист готовности к production

### Функциональность
- [ ] Все 6 бирж подключаются
- [ ] Стаканы получаются по WebSocket
- [ ] Вход/выход частями работает
- [ ] Асимметрия ног обрабатывается (отклонение объёмов < 1%)
- [ ] Partial fills обрабатываются корректно
- [ ] Recovery после рестарта работает
- [ ] Все 4 статуса переключаются корректно
- [ ] Проверка позиций при переходе PAUSE → READY
- [ ] Закрытие позиций при переходе TRADE → PAUSE
- [ ] Запрет отключения биржи при наличии TRADE пар
- [ ] Периодическая синхронизация с биржами работает
- [ ] PNL считается правильно
- [ ] Веб-интерфейс работает
- [ ] Логи записываются в правильном формате
- [ ] При потере связи в TRADE пара остаётся в TRADE (не ERROR)
- [ ] После восстановления связи данные актуализируются
- [ ] При невозможности открыть следующую часть — пара остаётся в TRADE

### Надёжность
- [ ] Reconnection к биржам работает
- [ ] Rate limiting не вызывает банов
- [ ] Данные сохраняются в БД
- [ ] Автозапуск при reboot работает
- [ ] Неблокирующий trading loop
- [ ] Таймауты на REST запросы к биржам

### Безопасность
- [ ] API ключи не передаются в frontend
- [ ] Авторизация работает
- [ ] Пароль хранится в конфиге, не в БД

---

## Целевые показатели производительности (из ТЗ)

| Операция | Целевое время |
|----------|---------------|
| Обновление стакана (10 уровней) | 0.5-1 мс |
| Расчёт PreSpread | 0.1-0.3 мс |
| Расчёт VWAP | 1-3 мс |
| Расчёт CleanSpread | 1-3 мс |

**Примечание:** Эти показатели критичны для арбитража, где счёт идёт на миллисекунды.

---

## Примечания

1. **Testnet**: Рекомендуется первоначальное тестирование на testnet биржах
2. **Backup**: Пользователь сам отвечает за бэкапы БД
3. **Риски**: Терминал v1 не имеет стоп-лосса и защиты от ликвидации
4. **Комиссии**: Fees захардкожены, при изменении на бирже требуется обновление

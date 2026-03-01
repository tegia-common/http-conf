# Task 02: Переход на единый `epoll`-менеджер внешних подключений

Дата: 2026-03-01  
Статус: draft for implementation

## 1. Цель

Ввести единый централизованный `epoll`-менеджер для всех внешних подключений и каналов:

1. HTTP/FastCGI сокеты.
2. Внешние TCP/Unix сокеты.
3. `pipe`/`eventfd` и аналогичные служебные FD.

Результат: один управляемый event-loop контур I/O с едиными правилами регистрации FD, обработки событий, таймаутов и graceful shutdown.

## 2. Мотивация

Текущее состояние с локальными циклами ожидания и точечной обработкой I/O усложняет:

1. масштабирование под нагрузкой;
2. централизованный контроль backpressure;
3. эксплуатационную наблюдаемость;
4. единообразное управление жизненным циклом соединений.

## 3. Архитектурное решение (target design)

### 3.1 Компоненты

1. `io::epoll_manager`
   - владеет `epoll_fd`;
   - регистрирует/удаляет/модифицирует FD (`ADD/MOD/DEL`);
   - выполняет `epoll_wait` и маршрутизирует события.
2. `io::fd_registry`
   - хранит metadata по FD:
     - `fd_type` (`http_listener`, `http_conn`, `socket`, `pipe`, `eventfd`);
     - owner/handler id;
     - mask интересующих событий;
     - policy (timeouts, limits).
3. `io::event_dispatcher`
   - преобразует `epoll` events в внутренние сообщения акторов:
     - `on_accept`;
     - `on_readable`;
     - `on_writable`;
     - `on_error`;
     - `on_hup`.
4. `io::timer_wheel` или equivalent timeout layer (можно начать с простого deadline heap)
   - idle/read/write timeout;
   - periodic maintenance callbacks.

### 3.2 Контракт адаптации к actor-модели

`epoll_manager` не исполняет бизнес-логику напрямую.  
Он только преобразует I/O-событие в message для owner-актора.

Контракт:

1. register:
   - `register_fd(fd, fd_type, owner_actor, event_mask, policies)`
2. event delivery:
   - `send(owner_actor, "/io/event", payload)`
3. unregister:
   - `unregister_fd(fd)`

### 3.3 Модель потоков (базовый вариант)

1. Один event-loop thread (MVP).
2. Бизнес-обработка остается в actor runtime.
3. Для high-load дальнейший шаг: N event-loop threads с `SO_REUSEPORT`/шардированием FD.

## 4. Scope и границы

### 4.1 Этот репозиторий (`configurations/http-conf`, library scope)

Обязательные изменения:

1. Выделить модуль I/O (`src/common/io/*`) с `epoll_manager`.
2. Интегрировать `HTTP::LISTENER` и `HTTP::CONNECTION` через единый I/O API.
3. Убрать прямые/локальные wait-loop паттерны в HTTP-контуре, где это применимо.
4. Добавить базовые unit/component тесты:
   - register/unregister;
   - dispatch correctness;
   - timeout + error/hup сценарии.
5. Добавить метрики event-loop:
   - active_fds;
   - events_per_sec;
   - dispatch_latency_ms;
   - dropped_events_total.

### 4.2 Отдельный репозиторий приложения (application scope)

Обязательные изменения:

1. Включить новый I/O manager в runtime-конфигурацию приложения.
2. Обеспечить e2e нагрузочные тесты и soak тесты.
3. Адаптировать внешние сокетные/pipe интеграции к новому register API.
4. Зафиксировать эксплуатационные алерты по метрикам event-loop.

## 5. Нефункциональные требования

1. C++ стандарт: строго `C++20`.
2. Fail-safety:
   - ошибка одного FD не останавливает event-loop;
   - корректная cleanup-последовательность при shutdown.
3. Производительность:
   - отсутствие busy-wait;
   - O(1)/амортизированно эффективные операции на горячем пути.
4. Observability:
   - структурированные логи ключевых событий I/O;
   - базовый набор метрик и счетчиков ошибок.

## 5.1 Зафиксированные архитектурные ограничения (из roadmap)

1. Модель `actor-per-request` сохраняется как целевая.
2. `LISTENER::unload` допускается только в системном контуре.
3. Политика владения объектами: `RAII-only` (без raw owning pointers).
4. Глобальные лимиты body должны быть конфигурируемыми и храниться в конфигурационном файле.
5. Контур `HTTP::WS` не является целевым в текущем плане и подлежит удалению/выводу из активного scope.

## 6. Риски и ограничения

1. Риск регрессий в HTTP-пути при замене механики accept/read/write.
2. Риск гонок при переходе на централизованный lifecycle FD.
3. Риск деградации latency при неоптимальном dispatch.
4. Риск частичного двойного управления FD, если old/new контуры временно работают параллельно.

Митигирующие меры:

1. Поэтапная миграция с feature flag.
2. Строгие stress/soak тесты.
3. Инструментирование и сравнение baseline vs new loop по метрикам.

## 7. План внедрения

### Этап 1: Design + MVP (1 спринт)

1. Специфицировать API `epoll_manager` и payload контракта `/io/event`.
2. Реализовать core manager + registry + dispatcher (без полной миграции всех контуров).
3. Подключить HTTP listener path как pilot.

### Этап 2: HTTP migration (1-2 спринта)

1. Перевести HTTP/FastCGI I/O на единый manager.
2. Добавить timeout/backpressure policies.
3. Включить метрики и базовые алерты.

### Этап 3: sockets + pipe consolidation (2+ спринта)

1. Мигрировать остальные внешние socket/pipe контуры.
2. Убрать legacy I/O loops.
3. Провести нагрузочную валидацию и закрепить новый контур как default.

## 8. Критерии приемки

1. Все внешние FD, заявленные в scope, регистрируются через единый I/O manager API.
2. Нет локальных дублирующих wait-loop в целевых контурах.
3. При ошибке/HUP конкретного FD event-loop продолжает работу.
4. Доступны метрики event-loop и они интегрированы в мониторинг.
5. Нагрузочные тесты показывают не хуже baseline по latency и error-rate.

## 9. Open Questions

1. Где и как формализовать единый `authN/authZ` gateway относительно нового I/O pipeline.
2. Какая целевая стратегия backpressure: drop, bounded queue, или adaptive throttling.
3. Где хранить ownership metadata FD: внутри manager или в actor registry с ссылками.
4. Нужна ли совместимость с edge-triggered режимом (`EPOLLET`) на первом этапе.
5. Какая утвержденная политика логирования PII/секретов для I/O событий и ошибок.

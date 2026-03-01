# File Storage Roadmap: аудит и план развития

Дата: 2026-03-01

## 1. Scope

Документ покрывает текущую реализацию upload/download в `http-conf` и roadmap доведения до production/highload уровня в library scope.

Основные точки реализации:

1. Upload parsing и сохранение файлов:
   - `src/common/connection.cpp`
   - `src/common/storage.h`
2. Download response:
   - `src/actors/CONNECTION/actions/response.cpp`

## 2. Краткая оценка текущего состояния

Текущий контур файлового хранилища: **3/10** (MVP/prototype, не production-grade).

Причины:

1. Read-all-in-memory для multipart body.
2. Отсутствие обязательных лимитов и квот.
3. Отсутствие транзакционной модели metadata+blob.
4. Слабая обработка ошибок файловых операций.
5. Локально-зависимый контракт путей и storage-base.

## 3. Текущая архитектура (as-is)

1. `connection_t::_multipart_form_data`:
   - читает весь body в память;
   - вручную парсит multipart;
   - пишет части во временный файл в `/tmp`.
2. `storage_t::save`:
   - копирует temp file в workspace-папку;
   - удаляет temp file;
   - обновляет metadata path/uuid.
3. Download:
   - отдает `X-Accel-Redirect` на внутренний путь;
   - формирует `Content-Disposition`.

## 4. Audit Findings

### 4.1 Critical

1. Нет контролируемых лимитов на размер body и число файлов в upload.
2. Body полностью загружается в память (`read-all-in-memory`) до обработки.
3. Нет строгого контроля ошибок в файловых операциях (`copy/remove` фактически без полноценной реакции на fail).

### 4.2 High

1. Нет атомарного finalize blob+metadata (возможны orphan/partial состояния).
2. Контракт использует локальные FS-пути и жестко зашитые base paths.
3. Отсутствует целостность файла (нет обязательного `sha256`/etag в процессе finalize).

### 4.3 Medium

1. Отсутствует полноценная observability по upload/download.
2. Нет lifecycle-джобов (GC/reconciliation).
3. В хранилище нет формальной модели состояний файла/сессии загрузки.

## 5. Целевая архитектура (to-be)

### 5.1 Базовые принципы

1. Не передавать локальные temp-path в межсервисных контрактах.
2. Единый upload-протокол с идемпотентностью:
   - `/upload/init`
   - `/upload/append`
   - `/upload/finish`
   - `/upload/abort`
3. `READY` только после атомарного finalize.
4. Обязательные лимиты и квоты на входе.

### 5.2 Компоненты

1. `HTTP::CONNECTION`:
   - только ingress/egress, authn/authz, I/O orchestration.
2. `HTTP::FILE_STORAGE` (отдельный bounded context):
   - lifecycle файла;
   - consistency metadata+blob;
   - storage backend abstraction (`localfs` -> `s3`).
3. `Storage driver` слой:
   - `LocalFSDriver` (MVP),
   - `S3Driver` (target).

### 5.3 Состояния файла

`INITIATED -> RECEIVING -> STAGED -> READY -> DELETING -> DELETED`, с обработкой `FAILED`.

## 6. Highload требования к file storage

1. Streaming upload без полного чтения body в память.
2. Backpressure и ограничение in-flight upload sessions.
3. Конфигурируемые лимиты:
   - max_file_size;
   - max_files_per_request;
   - max_upload_sessions_per_actor/tenant;
   - per-route rate limits.
4. Graceful degradation:
   - controlled reject при saturation;
   - timeout policy для slow clients.

## 7. Безопасность (обязательный baseline)

1. Входные лимиты до чтения body.
2. MIME allowlist + content sniffing.
3. Санитизация имен и заголовков download.
4. ACL проверка для download/delete.
5. Audit log всех операций file lifecycle.
6. Политика обработки PII в логах.

## 8. Data model (рекомендуемая)

### 8.1 `http_files`

1. `file_id` (UUID, PK)
2. `status`
3. `workspace/domain/owner_sub`
4. `backend/object_key/version_id/etag`
5. `original_name/content_type/size_bytes/sha256`
6. `created_at/updated_at/deleted_at`

### 8.2 `http_upload_sessions`

1. `upload_id` (UUID, PK)
2. `file_id`
3. `status` (`OPEN/FINISHING/DONE/ABORTED/FAILED`)
4. `idempotency_key`
5. `next_expected_chunk_no`
6. `received_bytes/last_activity_at`

## 9. Roadmap внедрения

### Этап 1 (P0): Stabilize current path (1-2 спринта)

1. Ввести жесткие лимиты body/files/count/timeouts.
2. Добавить строгую обработку ошибок FS-операций.
3. Убрать hardcoded storage paths в конфиг.
4. Добавить минимальные метрики upload/download.

Критерий:

1. Нет unbounded memory behavior.
2. Upload ошибки детерминированно обрабатываются и логируются.

### Этап 2 (P1): File lifecycle service (2-3 спринта)

1. Выделить `FILE_STORAGE` bounded context.
2. Реализовать протокол `init/append/finish/abort`.
3. Добавить state machine и идемпотентность.
4. Ввести finalize с проверкой размера и sha256.

Критерий:

1. Blob+metadata консистентны в стандартных и error сценариях.
2. Повторные запросы безопасны (idempotent behavior).

### Этап 3 (P2): Production hardening (3+ спринтов)

1. Reconciliation + GC jobs.
2. Расширенные SRE метрики и алерты.
3. Опциональный переход на `S3Driver`/presigned multipart flow.

Критерий:

1. Стабильность на soak/load тестах.
2. Контролируемый orphan rate и предсказуемый recovery.

## 10. Открытые вопросы

1. Какой backend является целевым по умолчанию (`localfs` vs `s3`)?
2. Где хранится authoritative metadata (текущая БД приложения или выделенная схема)?
3. Нужна ли quarantine/AV pipeline в первой production итерации?
4. Какой SLA по latency и throughput для upload/download?

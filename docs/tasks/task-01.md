# Task 01: Корректное имя файла с кириллицей в `Content-Disposition`

Дата: 2026-03-01  
Статус: архитектурно согласовано

## 1. Проблема

В текущей реализации download-ответа заголовок формируется только через `filename`:

- `src/actors/CONNECTION/actions/response.cpp:153`
- `Content-Disposition: attachment; filename="<имя_файла>"`

Для UTF-8 имен (кириллица) часть браузеров сохраняет файл с искажением.

## 2. Принятые решения

1. Legacy-клиенты не поддерживаются как отдельная цель.
2. ASCII fallback фиксируется строго как `download.<ext>` (без транслитерации).
3. Используем RFC 6266 + RFC 5987: в ответе всегда передаем `filename` и `filename*`.
4. Логика формируется централизованным helper (единая точка генерации заголовка).
5. Переопределения `Content-Disposition` на уровне nginx для этого потока нет.

## 3. Целевой контракт заголовка

Формат:

`Content-Disposition: attachment; filename="<fallback_ascii>"; filename*=UTF-8''<rfc5987_encoded_utf8>`

Пример:

`Content-Disposition: attachment; filename="download.m4a"; filename*=UTF-8''%D1%82%D0%B5%D1%81%D1%82%D0%BE%D0%B2%D1%8B%D0%B9%20%D1%84%D0%B0%D0%B9%D0%BB.m4a`

## 4. Правила нормализации и безопасности имени

### 4.1 Источник

`raw_filename` приходит из `message->http["response"]["filename"]`.

### 4.2 Санитизация `raw_filename` для `filename*`

Перед RFC5987-encoding:

1. Удалить `\r`, `\n`, `\0`.
2. Заменить path separators (`/`, `\`) на `_`.
3. Удалить управляющие символы ASCII (`0x00-0x1F`, `0x7F`).
4. Если после очистки имя пустое: использовать `download.bin`.
5. Ограничить длину итогового базового имени (рекомендуемо: 255 байт, с учетом расширения).

### 4.3 Формирование ASCII fallback `filename`

Алгоритм:

1. Из sanitized имени извлечь расширение (последний сегмент после `.`), если оно валидно:
   - длина `1..16`
   - символы `[A-Za-z0-9]`
2. Если расширение валидно: fallback = `download.<ext_lowercase>`
3. Иначе: fallback = `download.bin`

Итог: fallback всегда ASCII и детерминированный.

## 5. RFC5987 encoder (обязательное требование)

Нельзя считать URL-encoder полностью эквивалентным RFC5987 encoder.

Требование:

1. Ввести отдельную функцию `encode_rfc5987_value(const std::string& utf8)`.
2. Кодировать в `%HH` все байты UTF-8, кроме разрешенного attr-char набора RFC5987.
3. Префикс параметра всегда `UTF-8''`.

Примечание: `tegia::http::escape()` можно использовать только после документированного доказательства эквивалентности для нашего набора входов; иначе реализовать специализированный encoder.

## 6. Архитектура и размещение кода

### 6.1 Единая точка сборки

Создать helper уровня HTTP-ответа (общий util, не локальная ad-hoc конкатенация в action-файле):

- `sanitize_download_filename(...)`
- `build_ascii_fallback_filename(...)`
- `encode_rfc5987_value(...)`
- `build_content_disposition_attachment(...)`

### 6.2 Точка интеграции

Заменить строковую конкатенацию в:

- `src/actors/CONNECTION/actions/response.cpp` (блок `200 file/download`)

на вызов `build_content_disposition_attachment(raw_filename)`.

## 7. Нефункциональные требования

1. Безопасность:
   - исключить CRLF/header injection.
   - исключить path traversal в имени сохранения.
2. Совместимость:
   - корректное имя в Chrome/Firefox/Safari/Edge.
3. Сопровождаемость:
   - единая библиотечная реализация, исключить дубли.

## 8. Критерии приемки

1. Для `Отчет 2026.m4a`:
   - `filename="download.m4a"`
   - `filename*` содержит UTF-8 percent-encoded исходное имя.
2. Для `simple.txt`:
   - `filename="download.txt"`
   - `filename*` присутствует и валиден.
3. Для имени без валидного расширения:
   - fallback `download.bin`.
4. Для вредоносного ввода (`"a\r\nX-Test:1.mp3"`):
   - в итоговом заголовке нет инъекции/дополнительных header lines.
5. Unit-тесты на helper и интеграционный тест download-ответа проходят.

## 9. План внедрения

1. Реализовать helper и RFC5987 encoder.
2. Подключить helper в `response.cpp`.
3. Добавить unit-тесты:
   - кириллица, ASCII, спецсимволы, пустое имя, длинное имя.
4. Провести ручную проверку скачивания в Chrome/Firefox/Safari/Edge.

## 10. Разделение задач по репозиториям

### 10.1 Этот репозиторий (`configurations/http-conf`, библиотеки/модули)

Обязательные доработки:

1. Реализовать централизованный helper:
   - `sanitize_download_filename(...)`
   - `build_ascii_fallback_filename(...)`
   - `encode_rfc5987_value(...)`
   - `build_content_disposition_attachment(...)`
2. Интегрировать helper в `src/actors/CONNECTION/actions/response.cpp` в блок download-ответа.
3. Удалить ad-hoc конкатенацию `Content-Disposition` в местах, где затрагивается этот сценарий.
4. Добавить unit-тесты helper-функций (санитизация, fallback, RFC5987-encoding).
5. Добавить интеграционный тест формирования HTTP-ответа для download.
6. Обновить локальную документацию контракта заголовка и критериев приемки.

Граница ответственности:

1. Библиотечный код формирует корректный и безопасный HTTP-заголовок.
2. Библиотечный код не принимает решений о бизнес-именовании файла сверх зафиксированного fallback-алгоритма.

### 10.2 Отдельный репозиторий приложения (хост-приложение, использующее SDK/библиотеки)

Обязательные доработки:

1. Обеспечить передачу исходного `raw_filename` в библиотечный слой без предварительной порчи кодировки.
2. Проверить, что маршруты/обработчики приложения не переопределяют `Content-Disposition` после возврата из библиотек.
3. Зафиксировать контракт интеграции: библиотека отвечает за финальный формат заголовка, приложение не дублирует эту логику.
4. Добавить e2e-проверки на уровне приложения:
   - кириллица в имени файла;
   - ASCII-имя;
   - вредоносные значения с CRLF;
   - кейс без валидного расширения.
5. Проверить сквозной сценарий через runtime-конфигурацию веб-сервера/прокси приложения.
6. Зафиксировать и применить Unicode normalization policy (рекомендуемо: NFC) до передачи `raw_filename` в библиотечный слой.

Граница ответственности:

1. Приложение обеспечивает корректную доставку входных данных и отсутствие конфликтующих post-processing шагов.
2. Приложение владеет сквозной e2e-валидацией в своем runtime-контуре.

## 11. Риски

1. Ошибка в encoder => некорректные имена в части браузеров.
2. Неправильное выделение расширения => неверный fallback MIME/UX.
3. Регресс в местах, где `Content-Disposition` формируется вне централизованного helper.

Митигирующие меры:

1. Строгие unit-тесты для encoder и sanitization.
2. Запрет прямой конкатенации `Content-Disposition` в review checklist.

## 12. Отписание выполнения (для текста коммита)

### 12.1 Подробно

Выполнены доработки в library scope этого репозитория.

Изменения в коде:

1. Добавлен новый общий helper для формирования безопасного download-заголовка:
   - файл: `src/common/content_disposition.h`
   - функции:
     - `sanitize_download_filename(...)`
     - `build_ascii_fallback_filename(...)`
     - `encode_rfc5987_value(...)`
     - `build_content_disposition_attachment(...)`
2. Реализована санитизация входного имени:
   - удаление `\r`, `\n`, `\0`
   - удаление ASCII control-символов
   - замена `/` и `\` на `_`
   - ограничение длины имени до 255 байт с безопасным усечением на границе UTF-8.
3. Реализован детерминированный ASCII fallback:
   - извлечение валидного расширения (`[A-Za-z0-9]`, длина `1..16`)
   - fallback: `download.<ext_lowercase>`
   - если расширение невалидно/отсутствует: `download.bin`
4. Реализован отдельный RFC5987 encoder для `filename*`:
   - `%HH` encoding байтов UTF-8 вне разрешенного attr-char набора
   - параметр формируется с префиксом `UTF-8''`
5. Интеграция в download-ответ:
   - файл: `src/actors/CONNECTION/actions/response.cpp`
   - удалена ad-hoc конкатенация `Content-Disposition: attachment; filename="..."`
   - подключен вызов `HTTP::headers::build_content_disposition_attachment(raw_filename)`

Проверка:

1. Выполнена сборка `libCONNECTION`: `make -C bin libCONNECTION`
2. Результат: сборка успешна, ошибок компиляции/линковки нет.

Ограничения текущей итерации:

1. Отдельный unit/integration test harness в данном репозитории не обнаружен.
2. E2E-проверки на уровне хост-приложения остаются задачей application scope (отдельный репозиторий).

### 12.2 Кратко (готовый текст для commit message)

`http-conf: fix Content-Disposition for UTF-8 filenames (RFC6266/RFC5987)`

1. Add centralized header builder in `src/common/content_disposition.h`.
2. Implement filename sanitization and deterministic ASCII fallback (`download.<ext>` / `download.bin`).
3. Implement dedicated RFC5987 encoder for `filename*` (`UTF-8''...`).
4. Replace ad-hoc `Content-Disposition` concatenation in download response (`response.cpp`).
5. Verify by successful `make -C bin libCONNECTION` build.

## 13. Code Review (по реализации Task 01)

### 13.1 Findings (по severity)

1. Medium: не выполнен заявленный критерий приемки по автотестам.
   - В документе критерий требует unit/integration тесты: `docs/tasks/task-01.md:114`.
   - В фактической реализации добавлен код и сборочная проверка, но тестовый контур для новых функций не добавлен.
   - Риск: регрессы в edge-cases (`CRLF`, невалидный UTF-8, длинные имена) будут ловиться только вручную.

2. Low: helper реализован как header-only без выделенного testable compilation unit.
   - Файл: `src/common/content_disposition.h:1`.
   - Это допустимо, но усложняет изоляцию unit-тестов и может приводить к дублированию инстанцирования в разных модулях.
   - Рекомендация: при расширении логики вынести реализацию в `.cpp` + `.h` и подключить в общую библиотеку.

3. Low: стилистическая неоднородность форматирования в месте интеграции.
   - Файл: `src/actors/CONNECTION/actions/response.cpp:138`.
   - Ветка `case 3421455882` отформатирована не в одном стиле с соседними ветками. На поведение не влияет, но снижает читаемость.

### 13.2 Что проверено и подтверждено

1. Формирование `Content-Disposition` теперь централизовано:
   - `src/common/content_disposition.h:173`
   - `src/actors/CONNECTION/actions/response.cpp:142`
2. Для `filename*` используется отдельный RFC5987 encoder:
   - `src/common/content_disposition.h:153`
3. Для `filename` применяется детерминированный fallback `download.<ext>` / `download.bin`:
   - `src/common/content_disposition.h:96`
4. Сборка проходит:
   - `make -C bin libCONNECTION` (успешно).

### 13.3 Оценка переиспользования `namespace HTTP::headers`

Короткий вывод:

1. Да, код может переиспользоваться в других задачах, если они формируют HTTP-ответы с download/attachment (не только внутри текущего action).
2. Для задач вне HTTP-протокола прямое использование нецелесообразно, так как функции специфичны для `Content-Disposition` и RFC5987 header-параметров.
3. Для межрепозиторного переиспользования разумно перенести этот helper в общий SDK-уровень (например, `tegia-sdk`), чтобы исключить дубли между `http-conf` и хост-приложением.

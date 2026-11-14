# Старт DevOps-практики

Код приложения готов: клиент, сервер и база работают вместе. Вся
инфраструктура — Docker, CI/CD, конфигурация, миграции, мониторинг — пока
отсутствует **намеренно**: это твоя работа по плану «DevOps-план для мини-CRM».

Ниже — как поднять проект **вручную**. Сделай это хотя бы один раз: после
этого станет понятно, какую рутину автоматизирует каждый этап плана.

## 1. Что нужно

* **Сервер и база** — Linux. Проще всего WSL (Ubuntu 24.04) на Windows
  или виртуалка в VirtualBox.
* **Клиент** — Windows или Linux с Qt 5.15 или Qt 6 и Qt Creator.

## 2. Поднять PostgreSQL (Linux / WSL)

```bash
sudo apt update
sudo apt install postgresql
sudo service postgresql start

# пользователь и база приложения
sudo -u postgres psql -c "CREATE USER minicrm WITH PASSWORD 'minicrm';"
sudo -u postgres psql -c "CREATE DATABASE minicrm OWNER minicrm;"

# схема и демонстрационные данные
psql -h localhost -U minicrm -d minicrm -f db/init.sql
psql -h localhost -U minicrm -d minicrm -f db/seed.sql
```

## 3. Собрать и запустить сервер (Linux / WSL)

Нужен Qt 6.4+ с модулем HttpServer и драйвер QPSQL:

```bash
sudo apt install build-essential cmake qt6-base-dev qt6-httpserver-dev libqt6sql6-psql
# если пакет не найден: apt search qt6-httpserver

cmake -S . -B build -DMINICRM_BUILD_CLIENT=OFF
cmake --build build -j
ctest --test-dir build --output-on-failure     # тесты ядра

./build/server/minicrm-server --db-password minicrm
./build/server/minicrm-server --help           # все параметры
```

Проверка из второго терминала:

```bash
curl http://localhost:8080/api/clients
curl -X POST http://localhost:8080/api/clients \
     -H "Content-Type: application/json" \
     -d '{"name": "Тестов Тест", "email": "test@example.com"}'
curl -X DELETE http://localhost:8080/api/clients/6
```

## 4. Собрать и запустить клиент

Qt Creator → **Открыть проект** → `CMakeLists.txt` → выбрать комплект → Запуск.

* Если у твоего Qt нет модуля HttpServer (обычно на Windows), CMake выдаст
  предупреждение и соберёт только клиент. Это нормально.
* Клиент обращается к `http://localhost:8080`. WSL 2 пробрасывает этот порт
  в Windows сам. Для VirtualBox настрой проброс порта 8080 в сети NAT.
* Если сервер недоступен, клиент покажет ошибку и отключит сохранение, чтобы
  не затереть базу. Запусти сервер и нажми F5.

## 5. Что оставлено тебе

Это не баги, а точки входа в план. Каждую ты почувствуешь на ручном запуске выше.

| Сейчас | Этап плана |
|---|---|
| Сервер, база и зависимости ставятся руками через `apt` | 1, 3 — Docker, docker-compose |
| Нет сборки и тестов на каждый коммит | 2 — CI |
| Пароль БД передаётся аргументом и по умолчанию равен `minicrm` | 4 — переменные окружения, секреты |
| Адрес сервера зашит в `client/apiclient.cpp` | 4 — настройка через QSettings |
| Схема создаётся вручную скриптом `db/init.sql` | 5 — миграции |
| Бэкапов нет | 5 — `pg_dump` |
| Приложение под пользователем-владельцем базы | 5, 8 — минимальные права |
| Дистрибутивы и деплой делаются руками | 6 — CD |
| Логи сервера — обычный текст, нет `/health` и `/metrics` | 7 — наблюдаемость |
| API без авторизации, HTTP без шифрования | 6, 8 — Nginx + HTTPS, токен |

Ещё одна особенность для размышления: клиент сохраняет **весь список**
(`PUT /api/clients`), поэтому при двух одновременно открытых клиентах
побеждает тот, кто сохранил последним. Это пригодится на этапе 9: нагрузочный
тест и учебный инцидент.

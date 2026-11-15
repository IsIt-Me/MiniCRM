# Мини-CRM — база клиентов

Сквозной проект по курсу Qt (вариант 20), переделанный в клиент-серверное
приложение для практики DevOps.

* **Qt-клиент** (Windows / Linux) — список клиентов, карточка, заказы,
  автосохранение раз в 60 секунд.
* **API-сервер** на Qt 6 HttpServer — REST API в формате JSON.
* **PostgreSQL** — хранение клиентов и истории заказов.

```
[Qt-клиент]  --HTTP/JSON-->  [minicrm-server :8080]  -->  [PostgreSQL :5432]
```

## Структура

```
MiniCRM/
├── CMakeLists.txt          — корневой проект CMake
├── core/                   — общее ядро (QtCore): Client, Order, Validator, JSON
├── client/                 — настольное приложение (Qt Widgets)
│   └── apiclient.h/.cpp    — HTTP-клиент к серверу
├── server/                 — API-сервер (Qt HttpServer + Qt Sql/QPSQL)
│   └── clientrepository.*  — весь SQL сервера
├── db/
│   ├── init.sql            — схема базы данных
│   └── seed.sql            — демонстрационные данные
├── tests/                  — модульные тесты ядра (Qt Test)
├── DEVOPS_START.md         — как запустить проект сейчас и что дальше
└── PROJECT_FORM.md         — форма выбора темы
```

## API

| Метод | Путь | Что делает |
|---|---|---|
| GET | `/api/clients` | список клиентов с заказами |
| PUT | `/api/clients` | синхронизация всего списка (так сохраняет Qt-клиент) |
| POST | `/api/clients` | создать клиента → `201` |
| GET | `/api/clients/<id>` | один клиент или `404` |
| PUT | `/api/clients/<id>` | изменить клиента |
| DELETE | `/api/clients/<id>` | удалить клиента → `204` |

Ошибки возвращаются как `{"error": "текст"}`: `400` — данные не прошли
проверку `Validator`, `404` — клиента нет, `500` — ошибка базы данных.

Пример клиента:

```json
{
  "id": 1,
  "name": "Иванов Иван",
  "phone": "+7 900 123-45-67",
  "email": "ivanov@example.com",
  "company": "ООО «Вектор»",
  "notes": "",
  "created": "2026-11-11T10:00:00Z",
  "orders": [
    { "date": "2026-10-28", "description": "Поставка оборудования", "amount": 125000 }
  ]
}
```

## Горячие клавиши клиента

| Клавиши | Действие |
|---|---|
| Ctrl+N | новый клиент |
| Ctrl+S | сохранить на сервере |
| F5 | обновить с сервера |
| Ctrl+D | удалить выбранного клиента |
| Ctrl+Shift+Del | очистить базу |
| Ctrl+Q | выход |


## Как собрать

Проект состоит из трёх частей: PostgreSQL и API-сервер работают на Linux,
Qt-клиент — на Windows. Проверено на Astra Linux 1.8.1 и Windows 11.

### 1. Требования

**Сервер (Linux):** CMake 3.16+, Ninja, компилятор C++17, Qt 6.4+ с модулями
Core, Network, Sql (драйвер QPSQL) и HttpServer, PostgreSQL 13+.

**Клиент (Windows):** Qt 5.15 или Qt 6 с модулями Widgets и Network,
Qt Creator, Ninja (`winget install Ninja-build.Ninja`).

### 2. База данных

```bash
sudo apt install -y postgresql
sudo systemctl enable --now postgresql
sudo -u postgres psql -c "CREATE USER minicrm WITH PASSWORD 'minicrm';"
sudo -u postgres psql -c "CREATE DATABASE minicrm OWNER minicrm;"
psql -h localhost -U minicrm -d minicrm -f db/init.sql
psql -h localhost -U minicrm -d minicrm -f db/seed.sql   # тестовые данные
```

### 3. Сервер

```bash
sudo apt install -y build-essential cmake ninja-build git \
    qt6-base-dev qt6-base-private-dev libqt6sql6-psql
```

Если в репозитории дистрибутива нет пакета Qt HttpServer (например, в Astra Linux),
соберите модуль из исходников

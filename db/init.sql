-- Схема базы данных мини-CRM (PostgreSQL).
-- Применить:  psql -h localhost -U minicrm -d minicrm -f db/init.sql
-- Скрипт можно запускать повторно: существующие таблицы не пересоздаются.

CREATE TABLE IF NOT EXISTS clients (
    id          BIGSERIAL    PRIMARY KEY,
    name        TEXT         NOT NULL CHECK (char_length(btrim(name)) BETWEEN 2 AND 100),
    phone       TEXT         NOT NULL DEFAULT '',
    email       TEXT         NOT NULL DEFAULT '',
    company     TEXT         NOT NULL DEFAULT '',
    notes       TEXT         NOT NULL DEFAULT '',
    created_at  TIMESTAMPTZ  NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS orders (
    id           BIGSERIAL      PRIMARY KEY,
    client_id    BIGINT         NOT NULL REFERENCES clients (id) ON DELETE CASCADE,
    order_date   DATE           NOT NULL,
    description  TEXT           NOT NULL,
    amount       NUMERIC(12, 2) NOT NULL DEFAULT 0 CHECK (amount >= 0)
);

-- Заказы всегда читаются по клиенту.
CREATE INDEX IF NOT EXISTS orders_client_id_idx ON orders (client_id);

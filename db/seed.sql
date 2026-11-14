-- Демонстрационные данные: 5 клиентов и 3 заказа.
-- Применять на пустую базу после init.sql:
--   psql -h localhost -U minicrm -d minicrm -f db/seed.sql

INSERT INTO clients (name, phone, email, company) VALUES
    ('Иванов Иван',     '+7 900 123-45-67',  'ivanov@example.com', 'ООО «Вектор»'),
    ('Петрова Мария',   '+7 912 555-01-02',  '',                   'ИП Петрова'),
    ('ООО Ромашка',     '',                  'info@romashka.ru',   ''),
    ('Сидоров Алексей', '8 (495) 765-43-21', 'sidorov@mail.ru',    'АО «Техно»'),
    ('Кузнецова Ольга', '+7 921 000-11-22',  'olga.k@example.com', '');

INSERT INTO orders (client_id, order_date, description, amount)
SELECT id, current_date - 14, 'Поставка оборудования', 125000 FROM clients WHERE name = 'Иванов Иван'
UNION ALL
SELECT id, current_date - 3, 'Сервисное обслуживание', 18500 FROM clients WHERE name = 'Иванов Иван'
UNION ALL
SELECT id, current_date - 3, 'Сервисное обслуживание', 18500 FROM clients WHERE name = 'Сидоров Алексей';

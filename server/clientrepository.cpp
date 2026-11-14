#include "clientrepository.h"

#include <QDebug>
#include <QHash>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <utility>

namespace {

void setError(QString *error, const QString &text)
{
    if (error)
        *error = text;
}

// Пустую строку храним как '' (а не NULL): так проще и клиенту, и SQL.
QVariant text(const QString &value)
{
    return value.isNull() ? QStringLiteral("") : value;
}

} // namespace

ClientRepository::ClientRepository(const DbConfig &config)
    : m_config(config)
    , m_connectionName(QStringLiteral("minicrm"))
{
}

ClientRepository::~ClientRepository()
{
    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool ClientRepository::open(QString *error)
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QPSQL"))) {
        setError(error, QStringLiteral("Драйвер QPSQL не найден. Доступны: %1")
                            .arg(QSqlDatabase::drivers().join(QStringLiteral(", "))));
        return false;
    }

    QSqlDatabase db = QSqlDatabase::contains(m_connectionName)
                          ? QSqlDatabase::database(m_connectionName, false)
                          : QSqlDatabase::addDatabase(QStringLiteral("QPSQL"), m_connectionName);
    db.setHostName(m_config.host);
    db.setPort(m_config.port);
    db.setDatabaseName(m_config.name);
    db.setUserName(m_config.user);
    db.setPassword(m_config.password);
    db.setConnectOptions(QStringLiteral("connect_timeout=5"));

    if (!db.open()) {
        setError(error, db.lastError().text());
        return false;
    }
    qInfo().noquote() << QStringLiteral("[DB] подключено к %1:%2/%3 как %4")
                             .arg(m_config.host).arg(m_config.port)
                             .arg(m_config.name, m_config.user);
    return true;
}

bool ClientRepository::ensureOpen(QString *error)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (db.isValid() && db.isOpen())
        return true;
    qWarning() << "[DB] нет соединения с базой, переподключаюсь";
    return open(error);
}

bool ClientRepository::exec(QSqlQuery &query, QString *error)
{
    if (query.exec())
        return true;

    const QSqlError err = query.lastError();
    setError(error, err.text());
    qWarning().noquote() << "[DB] ошибка запроса:" << err.text();

    // Соединение оборвалось (например, перезапустили PostgreSQL) — закрываем,
    // чтобы следующий запрос переподключился через ensureOpen().
    if (err.type() == QSqlError::ConnectionError)
        QSqlDatabase::database(m_connectionName, false).close();
    return false;
}

// ---------------------------------------------------------------------------
//                                  Чтение
// ---------------------------------------------------------------------------

bool ClientRepository::listAll(QVector<Client> &clients, QString *error)
{
    clients.clear();
    if (!ensureOpen(error))
        return false;

    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral(
        "SELECT id, name, phone, email, company, notes, created_at "
        "FROM clients ORDER BY id"));
    if (!exec(q, error))
        return false;

    while (q.next()) {
        Client c;
        c.id = q.value(0).toLongLong();
        c.name = q.value(1).toString();
        c.phone = q.value(2).toString();
        c.email = q.value(3).toString();
        c.company = q.value(4).toString();
        c.notes = q.value(5).toString();
        c.created = q.value(6).toDateTime();
        clients.append(c);
    }
    return loadOrders(clients, error);
}

bool ClientRepository::get(qint64 id, Client &client, bool *found, QString *error)
{
    *found = false;
    if (!ensureOpen(error))
        return false;

    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral(
        "SELECT id, name, phone, email, company, notes, created_at "
        "FROM clients WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    if (!exec(q, error))
        return false;
    if (!q.next())
        return true;   // не ошибка: клиента просто нет

    QVector<Client> one(1);
    Client &c = one[0];
    c.id = q.value(0).toLongLong();
    c.name = q.value(1).toString();
    c.phone = q.value(2).toString();
    c.email = q.value(3).toString();
    c.company = q.value(4).toString();
    c.notes = q.value(5).toString();
    c.created = q.value(6).toDateTime();
    if (!loadOrders(one, error))
        return false;

    client = one.first();
    *found = true;
    return true;
}

bool ClientRepository::loadOrders(QVector<Client> &clients, QString *error)
{
    if (clients.isEmpty())
        return true;

    QHash<qint64, int> rowById;
    for (int i = 0; i < clients.size(); ++i) {
        clients[i].orders.clear();
        rowById.insert(clients.at(i).id, i);
    }

    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    if (clients.size() == 1) {
        q.prepare(QStringLiteral(
            "SELECT client_id, order_date, description, amount FROM orders "
            "WHERE client_id = :id ORDER BY order_date, id"));
        q.bindValue(QStringLiteral(":id"), clients.first().id);
    } else {
        q.prepare(QStringLiteral(
            "SELECT client_id, order_date, description, amount FROM orders "
            "ORDER BY client_id, order_date, id"));
    }
    if (!exec(q, error))
        return false;

    while (q.next()) {
        const auto it = rowById.constFind(q.value(0).toLongLong());
        if (it == rowById.constEnd())
            continue;
        Order o;
        o.date = q.value(1).toDate();
        o.description = q.value(2).toString();
        o.amount = q.value(3).toDouble();
        clients[it.value()].orders.append(o);
    }
    return true;
}

// ---------------------------------------------------------------------------
//                                  Запись
// ---------------------------------------------------------------------------

bool ClientRepository::insertClient(Client &client, QString *error)
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral(
        "INSERT INTO clients (name, phone, email, company, notes, created_at) "
        "VALUES (:name, :phone, :email, :company, :notes, :created) RETURNING id"));
    if (!client.created.isValid())
        client.created = QDateTime::currentDateTimeUtc();
    q.bindValue(QStringLiteral(":name"), client.name.trimmed());
    q.bindValue(QStringLiteral(":phone"), text(client.phone.trimmed()));
    q.bindValue(QStringLiteral(":email"), text(client.email.trimmed()));
    q.bindValue(QStringLiteral(":company"), text(client.company.trimmed()));
    q.bindValue(QStringLiteral(":notes"), text(client.notes.trimmed()));
    q.bindValue(QStringLiteral(":created"), client.created);
    if (!exec(q, error) || !q.next())
        return false;

    client.id = q.value(0).toLongLong();
    return writeOrders(client.id, client.orders, error);
}

bool ClientRepository::updateClientRow(const Client &client, bool *found, QString *error)
{
    *found = false;
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral(
        "UPDATE clients SET name = :name, phone = :phone, email = :email, "
        "company = :company, notes = :notes WHERE id = :id"));
    q.bindValue(QStringLiteral(":name"), client.name.trimmed());
    q.bindValue(QStringLiteral(":phone"), text(client.phone.trimmed()));
    q.bindValue(QStringLiteral(":email"), text(client.email.trimmed()));
    q.bindValue(QStringLiteral(":company"), text(client.company.trimmed()));
    q.bindValue(QStringLiteral(":notes"), text(client.notes.trimmed()));
    q.bindValue(QStringLiteral(":id"), client.id);
    if (!exec(q, error))
        return false;
    if (q.numRowsAffected() == 0)
        return true;   // клиента с таким id нет

    *found = true;
    return writeOrders(client.id, client.orders, error);
}

bool ClientRepository::writeOrders(qint64 clientId, const QList<Order> &orders, QString *error)
{
    // История заказов перезаписывается целиком: так проще, чем сравнивать списки.
    QSqlQuery del(QSqlDatabase::database(m_connectionName));
    del.prepare(QStringLiteral("DELETE FROM orders WHERE client_id = :id"));
    del.bindValue(QStringLiteral(":id"), clientId);
    if (!exec(del, error))
        return false;

    QSqlQuery ins(QSqlDatabase::database(m_connectionName));
    ins.prepare(QStringLiteral(
        "INSERT INTO orders (client_id, order_date, description, amount) "
        "VALUES (:client, :date, :description, :amount)"));
    for (const Order &o : orders) {
        ins.bindValue(QStringLiteral(":client"), clientId);
        ins.bindValue(QStringLiteral(":date"), o.date);
        ins.bindValue(QStringLiteral(":description"), o.description.trimmed());
        ins.bindValue(QStringLiteral(":amount"), o.amount);
        if (!exec(ins, error))
            return false;
    }
    return true;
}

bool ClientRepository::create(Client &client, QString *error)
{
    if (!ensureOpen(error))
        return false;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.transaction()) {
        setError(error, db.lastError().text());
        return false;
    }
    if (!insertClient(client, error)) {
        db.rollback();
        client.id = 0;
        return false;
    }
    if (!db.commit()) {
        setError(error, db.lastError().text());
        db.rollback();
        client.id = 0;
        return false;
    }
    return true;
}

bool ClientRepository::update(const Client &client, bool *found, QString *error)
{
    *found = false;
    if (!ensureOpen(error))
        return false;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.transaction()) {
        setError(error, db.lastError().text());
        return false;
    }
    if (!updateClientRow(client, found, error)) {
        db.rollback();
        return false;
    }
    if (!*found) {
        db.rollback();
        return true;   // клиента с таким id нет — это не ошибка БД
    }
    if (!db.commit()) {
        setError(error, db.lastError().text());
        db.rollback();
        return false;
    }
    return true;
}

bool ClientRepository::remove(qint64 id, bool *found, QString *error)
{
    *found = false;
    if (!ensureOpen(error))
        return false;

    // Заказы удалятся сами: внешний ключ объявлен с ON DELETE CASCADE.
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral("DELETE FROM clients WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    if (!exec(q, error))
        return false;
    *found = q.numRowsAffected() > 0;
    return true;
}

bool ClientRepository::replaceAll(QVector<Client> &clients, QString *error)
{
    if (!ensureOpen(error))
        return false;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.transaction()) {
        setError(error, db.lastError().text());
        return false;
    }
    auto fail = [&db]() {
        db.rollback();
        return false;
    };

    // 1. Какие клиенты сейчас есть в базе.
    QSet<qint64> existing;
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT id FROM clients"));
        if (!exec(q, error))
            return fail();
        while (q.next())
            existing.insert(q.value(0).toLongLong());
    }

    // 2. Удаляем тех, кого больше нет в списке приложения.
    QSet<qint64> incoming;
    for (const Client &c : clients) {
        if (c.id > 0)
            incoming.insert(c.id);
    }
    for (qint64 id : std::as_const(existing)) {
        if (incoming.contains(id))
            continue;
        QSqlQuery del(db);
        del.prepare(QStringLiteral("DELETE FROM clients WHERE id = :id"));
        del.bindValue(QStringLiteral(":id"), id);
        if (!exec(del, error))
            return fail();
    }

    // 3. Обновляем существующих и добавляем новых.
    for (Client &c : clients) {
        if (c.id > 0 && existing.contains(c.id)) {
            bool found = false;
            if (!updateClientRow(c, &found, error))
                return fail();
        } else {
            // Новый клиент, либо его id уже удалили на сервере — создаём заново.
            c.id = 0;
            if (!insertClient(c, error))
                return fail();
        }
    }

    if (!db.commit()) {
        setError(error, db.lastError().text());
        return fail();
    }
    return true;
}

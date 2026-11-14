#include "client.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStringList>

namespace {
const int kFormatVersion = 1;
}

// ----------------------------- Order -----------------------------

QJsonObject Order::toJson() const
{
    QJsonObject obj;
    obj["date"] = date.toString(Qt::ISODate);
    obj["description"] = description;
    obj["amount"] = amount;
    return obj;
}

Order Order::fromJson(const QJsonObject &obj)
{
    Order o;
    o.date = QDate::fromString(obj["date"].toString(), Qt::ISODate);
    o.description = obj["description"].toString();
    o.amount = obj["amount"].toDouble();
    return o;
}

// ----------------------------- Client -----------------------------

double Client::totalAmount() const
{
    double sum = 0.0;
    for (const Order &o : orders)
        sum += o.amount;
    return sum;
}

QString Client::summary() const
{
    QStringList parts;
    if (!company.isEmpty()) parts << company;
    if (!phone.isEmpty())   parts << phone;
    if (!email.isEmpty())   parts << email;
    parts << QStringLiteral("заказов: %1").arg(orders.size());
    return name + QStringLiteral(" — ") + parts.join(QStringLiteral(", "));
}

QJsonObject Client::toJson() const
{
    QJsonObject obj;
    if (id > 0)
        obj["id"] = id;   // JSON хранит числа как double: id до 2^53 передаются без потерь
    obj["name"] = name;
    obj["phone"] = phone;
    obj["email"] = email;
    obj["company"] = company;
    obj["notes"] = notes;
    obj["created"] = created.toString(Qt::ISODate);

    QJsonArray arr;
    for (const Order &o : orders)
        arr.append(o.toJson());
    obj["orders"] = arr;
    return obj;
}

Client Client::fromJson(const QJsonObject &obj)
{
    Client c;
    c.id = static_cast<qint64>(obj["id"].toDouble());
    c.name = obj["name"].toString();
    c.phone = obj["phone"].toString();
    c.email = obj["email"].toString();
    c.company = obj["company"].toString();
    c.notes = obj["notes"].toString();
    c.created = QDateTime::fromString(obj["created"].toString(), Qt::ISODate);

    const QJsonArray arr = obj["orders"].toArray();
    for (const QJsonValue &v : arr)
        c.orders.append(Order::fromJson(v.toObject()));
    return c;
}

// ------------------------ Файловые операции ------------------------

bool saveClientsToFile(const QVector<Client> &clients, const QString &path, QString *error)
{
    QJsonArray arr;
    for (const Client &c : clients)
        arr.append(c.toJson());

    QJsonObject root;
    root["version"] = kFormatVersion;
    root["clients"] = arr;

    // QSaveFile пишет во временный файл и подменяет оригинал только после
    // успешной записи — база не испортится, если программа упадёт посередине.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

bool loadClientsFromFile(const QString &path, QVector<Client> &clients, QString *error)
{
    clients.clear();

    QFile file(path);
    if (!file.exists())
        return true;   // первый запуск: файла ещё нет — это не ошибка

    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) *error = QStringLiteral("Некорректный JSON: %1").arg(parseError.errorString());
        return false;
    }

    const QJsonArray arr = doc.object()["clients"].toArray();
    for (const QJsonValue &v : arr)
        clients.append(Client::fromJson(v.toObject()));
    return true;
}

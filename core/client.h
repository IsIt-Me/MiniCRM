#ifndef CLIENT_H
#define CLIENT_H

#include <QDate>
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QtGlobal>
#include <QVector>

// Заказ клиента — элемент истории заказов.
struct Order
{
    QDate   date;
    QString description;
    double  amount = 0.0;

    QJsonObject toJson() const;
    static Order fromJson(const QJsonObject &obj);
};

// Клиент — основная сущность мини-CRM.
struct Client
{
    qint64       id = 0;    // идентификатор в БД сервера (0 — ещё не сохранён на сервере)
    QString      name;      // ФИО или название клиента (обязательно)
    QString      phone;     // телефон (необязательно, но нужен телефон или email)
    QString      email;     // email
    QString      company;   // компания
    QString      notes;     // заметки менеджера
    QDateTime    created;   // дата добавления в базу
    QList<Order> orders;    // история заказов

    double  totalAmount() const;   // сумма всех заказов
    QString summary() const;       // краткое описание для подсказок и логов

    QJsonObject toJson() const;
    static Client fromJson(const QJsonObject &obj);
};

// Сохранение / загрузка базы клиентов в JSON-файл.
// При ошибке возвращают false и пишут текст ошибки в *error (если передан).
bool saveClientsToFile(const QVector<Client> &clients, const QString &path,
                       QString *error = nullptr);
bool loadClientsFromFile(const QString &path, QVector<Client> &clients,
                         QString *error = nullptr);

#endif // CLIENT_H

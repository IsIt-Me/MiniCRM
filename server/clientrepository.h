#ifndef CLIENTREPOSITORY_H
#define CLIENTREPOSITORY_H

#include <QString>
#include <QVector>

#include "client.h"

class QSqlQuery;

// Параметры подключения к PostgreSQL.
struct DbConfig
{
    QString host = QStringLiteral("localhost");
    int     port = 5432;
    QString name = QStringLiteral("minicrm");
    QString user = QStringLiteral("minicrm");
    QString password;
};

// Репозиторий клиентов: весь SQL сервера собран в одном месте.
// Таблицы clients и orders описаны в db/init.sql.
//
// Все методы возвращают false при ошибке БД и пишут её текст в *error.
// Если соединение с PostgreSQL потеряно (база перезапустилась), следующий
// запрос попробует подключиться заново.
class ClientRepository
{
public:
    explicit ClientRepository(const DbConfig &config);
    ~ClientRepository();

    bool open(QString *error);

    bool listAll(QVector<Client> &clients, QString *error);
    bool get(qint64 id, Client &client, bool *found, QString *error);
    bool create(Client &client, QString *error);                       // заполняет client.id
    bool update(const Client &client, bool *found, QString *error);
    bool remove(qint64 id, bool *found, QString *error);

    // Синхронизация всей базы с клиентским приложением (PUT /api/clients):
    // клиенты с id обновляются, без id создаются, отсутствующие в списке
    // удаляются. Всё в одной транзакции. Заполняет id у новых клиентов.
    bool replaceAll(QVector<Client> &clients, QString *error);

private:
    bool ensureOpen(QString *error);
    bool exec(QSqlQuery &query, QString *error);
    bool insertClient(Client &client, QString *error);
    bool updateClientRow(const Client &client, bool *found, QString *error);
    bool writeOrders(qint64 clientId, const QList<Order> &orders, QString *error);
    bool loadOrders(QVector<Client> &clients, QString *error);

    DbConfig m_config;
    QString  m_connectionName;
};

#endif // CLIENTREPOSITORY_H

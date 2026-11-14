#ifndef APICLIENT_H
#define APICLIENT_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVector>

#include "client.h"

class QJsonDocument;
class QNetworkAccessManager;

// HTTP-клиент к API-серверу мини-CRM (server/).
//
// Запросы синхронные: метод ждёт ответа (не дольше kTimeoutMs), но окно при
// этом перерисовывается — работает локальный QEventLoop. Ввод пользователя на
// время запроса блокируется, чтобы данные не поменялись посередине синхронизации.
class ApiClient : public QObject
{
    Q_OBJECT

public:
    explicit ApiClient(QObject *parent = nullptr);

    QString serverUrl() const;

    // GET /api/clients — загрузить всех клиентов.
    bool fetchClients(QVector<Client> &clients, QString *error);

    // PUT /api/clients — отправить весь список. Сервер вернёт его же с id,
    // назначенными новым клиентам; они записываются обратно в clients.
    bool syncClients(QVector<Client> &clients, QString *error);

    static constexpr int kTimeoutMs = 5000;

private:
    bool send(const QByteArray &verb, const QByteArray &body,
              QJsonDocument &response, QString *error);

    QNetworkAccessManager *m_network = nullptr;
};

#endif // APICLIENT_H

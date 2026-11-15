#include "apiclient.h"

#include <QDebug>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {

// Адрес API-сервера пока зашит в код.
const char kServerUrl[] = "http://192.168.50.135:8080";

void setError(QString *error, const QString &text)
{
    if (error)
        *error = text;
}

QVector<Client> clientsFromJson(const QJsonArray &arr)
{
    QVector<Client> clients;
    clients.reserve(arr.size());
    for (const QJsonValue &v : arr)
        clients.append(Client::fromJson(v.toObject()));
    return clients;
}

} // namespace

ApiClient::ApiClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

QString ApiClient::serverUrl() const
{
    return QString::fromLatin1(kServerUrl);
}

bool ApiClient::send(const QByteArray &verb, const QByteArray &body,
                     QJsonDocument &response, QString *error)
{
    QNetworkRequest request(QUrl(serverUrl() + QStringLiteral("/api/clients")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/json"));
    request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));
    request.setTransferTimeout(kTimeoutMs);   // запрос прервётся, если сервер молчит

    QNetworkReply *reply = (verb == "GET")
                               ? m_network->get(request)
                               : m_network->sendCustomRequest(request, verb, body);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec(QEventLoop::ExcludeUserInputEvents);

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data = reply->readAll();
    const QNetworkReply::NetworkError netError = reply->error();
    const QString netErrorText = reply->errorString();
    reply->deleteLater();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (netError != QNetworkReply::NoError) {
        // Сервер ответил ошибкой с JSON {"error": "..."} — показываем его текст.
        const QString serverMessage = doc.isObject()
                                          ? doc.object().value(QStringLiteral("error")).toString()
                                          : QString();
        if (status > 0 && !serverMessage.isEmpty())
            setError(error, QStringLiteral("HTTP %1: %2").arg(status).arg(serverMessage));
        else if (status > 0)
            setError(error, QStringLiteral("HTTP %1: %2").arg(status).arg(netErrorText));
        else
            setError(error, QStringLiteral("Сервер %1 недоступен: %2").arg(serverUrl(), netErrorText));
        qWarning().noquote() << "[API]" << verb << "ошибка:" << (error ? *error : netErrorText);
        return false;
    }

    if (parseError.error != QJsonParseError::NoError) {
        setError(error, QStringLiteral("Некорректный ответ сервера: %1").arg(parseError.errorString()));
        return false;
    }

    qDebug().noquote() << "[API]" << verb << "/api/clients ->" << status;
    response = doc;
    return true;
}

bool ApiClient::fetchClients(QVector<Client> &clients, QString *error)
{
    QJsonDocument doc;
    if (!send(QByteArrayLiteral("GET"), QByteArray(), doc, error))
        return false;
    if (!doc.isArray()) {
        setError(error, QStringLiteral("Сервер вернул не список клиентов"));
        return false;
    }
    clients = clientsFromJson(doc.array());
    return true;
}

bool ApiClient::syncClients(QVector<Client> &clients, QString *error)
{
    QJsonArray arr;
    for (const Client &c : clients)
        arr.append(c.toJson());

    QJsonDocument doc;
    if (!send(QByteArrayLiteral("PUT"), QJsonDocument(arr).toJson(QJsonDocument::Compact),
              doc, error))
        return false;
    if (!doc.isArray()) {
        setError(error, QStringLiteral("Сервер вернул не список клиентов"));
        return false;
    }

    const QVector<Client> saved = clientsFromJson(doc.array());
    if (saved.size() != clients.size()) {
        setError(error, QStringLiteral("Сервер вернул %1 клиентов вместо %2")
                            .arg(saved.size()).arg(clients.size()));
        return false;
    }
    for (int i = 0; i < clients.size(); ++i)
        clients[i].id = saved.at(i).id;
    return true;
}

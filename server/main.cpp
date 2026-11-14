// API-сервер мини-CRM: хранит клиентов и заказы в PostgreSQL и отдаёт их
// настольному клиенту по HTTP в формате JSON.
//
// Эндпоинты:
//   GET    /api/clients        — список клиентов с заказами
//   PUT    /api/clients        — синхронизация всего списка (так сохраняет Qt-клиент)
//   POST   /api/clients        — создать клиента
//   GET    /api/clients/<id>   — один клиент
//   PUT    /api/clients/<id>   — изменить клиента
//   DELETE /api/clients/<id>   — удалить клиента
//
// Параметры запуска — см. ./minicrm-server --help

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QHostAddress>
#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>

#include "clientrepository.h"
#include "validator.h"

namespace {

using Status = QHttpServerResponse::StatusCode;

QHttpServerResponse json(const QJsonDocument &doc, Status status = Status::Ok)
{
    return QHttpServerResponse(QByteArrayLiteral("application/json"),
                               doc.toJson(QJsonDocument::Compact), status);
}

QHttpServerResponse json(const QJsonObject &obj, Status status = Status::Ok)
{
    return json(QJsonDocument(obj), status);
}

QHttpServerResponse errorResponse(Status status, const QString &message)
{
    return json(QJsonObject{{QStringLiteral("error"), message}}, status);
}

QHttpServerResponse dbError(const QString &error)
{
    // Подробности — в лог сервера, клиенту — общий текст.
    qWarning().noquote() << "[API] ошибка базы данных:" << error;
    return errorResponse(Status::InternalServerError,
                         QStringLiteral("Ошибка базы данных, подробности в логе сервера"));
}

QString methodName(const QHttpServerRequest &request)
{
    switch (request.method()) {
    case QHttpServerRequest::Method::Get:    return QStringLiteral("GET");
    case QHttpServerRequest::Method::Post:   return QStringLiteral("POST");
    case QHttpServerRequest::Method::Put:    return QStringLiteral("PUT");
    case QHttpServerRequest::Method::Delete: return QStringLiteral("DELETE");
    default:                                 return QStringLiteral("OTHER");
    }
}

void logRequest(const QHttpServerRequest &request)
{
    qInfo().noquote() << "[HTTP]" << methodName(request) << request.url().path()
                      << "от" << request.remoteAddress().toString();
}

// Разбор и проверка клиента из тела запроса. При ошибке заполняет *response.
bool parseClient(const QJsonObject &obj, Client &client, QHttpServerResponse *response)
{
    client = Client::fromJson(obj);
    Validator validator;
    if (!validator.validateClient(client)) {
        *response = errorResponse(Status::BadRequest, validator.lastError());
        return false;
    }
    return true;
}

bool parseBodyObject(const QHttpServerRequest &request, QJsonObject &obj,
                     QHttpServerResponse *response)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(request.body(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        *response = errorResponse(Status::BadRequest,
                                  QStringLiteral("Ожидается JSON-объект клиента"));
        return false;
    }
    obj = doc.object();
    return true;
}

QJsonArray toJsonArray(const QVector<Client> &clients)
{
    QJsonArray arr;
    for (const Client &c : clients)
        arr.append(c.toJson());
    return arr;
}

void registerRoutes(QHttpServer &server, ClientRepository &repo)
{
    // ---------- GET /api/clients ----------
    server.route(QStringLiteral("/api/clients"), QHttpServerRequest::Method::Get,
                 [&repo](const QHttpServerRequest &request) {
        logRequest(request);
        QVector<Client> clients;
        QString error;
        if (!repo.listAll(clients, &error))
            return dbError(error);
        return json(QJsonDocument(toJsonArray(clients)));
    });

    // ---------- PUT /api/clients — синхронизация всего списка ----------
    server.route(QStringLiteral("/api/clients"), QHttpServerRequest::Method::Put,
                 [&repo](const QHttpServerRequest &request) {
        logRequest(request);
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(request.body(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isArray())
            return errorResponse(Status::BadRequest,
                                 QStringLiteral("Ожидается JSON-массив клиентов"));

        QVector<Client> clients;
        const QJsonArray arr = doc.array();
        for (const QJsonValue &v : arr) {
            Client c;
            QHttpServerResponse bad(Status::BadRequest);
            if (!parseClient(v.toObject(), c, &bad))
                return bad;
            clients.append(c);
        }

        QString error;
        if (!repo.replaceAll(clients, &error))
            return dbError(error);
        qInfo() << "[API] синхронизировано клиентов:" << clients.size();
        return json(QJsonDocument(toJsonArray(clients)));   // с присвоенными id
    });

    // ---------- POST /api/clients ----------
    server.route(QStringLiteral("/api/clients"), QHttpServerRequest::Method::Post,
                 [&repo](const QHttpServerRequest &request) {
        logRequest(request);
        QJsonObject obj;
        Client c;
        QHttpServerResponse bad(Status::BadRequest);
        if (!parseBodyObject(request, obj, &bad) || !parseClient(obj, c, &bad))
            return bad;

        c.id = 0;   // id назначает база
        QString error;
        if (!repo.create(c, &error))
            return dbError(error);
        return json(c.toJson(), Status::Created);
    });

    // ---------- GET /api/clients/<id> ----------
    server.route(QStringLiteral("/api/clients/<arg>"), QHttpServerRequest::Method::Get,
                 [&repo](qint64 id, const QHttpServerRequest &request) {
        logRequest(request);
        Client c;
        bool found = false;
        QString error;
        if (!repo.get(id, c, &found, &error))
            return dbError(error);
        if (!found)
            return errorResponse(Status::NotFound, QStringLiteral("Клиент не найден"));
        return json(c.toJson());
    });

    // ---------- PUT /api/clients/<id> ----------
    server.route(QStringLiteral("/api/clients/<arg>"), QHttpServerRequest::Method::Put,
                 [&repo](qint64 id, const QHttpServerRequest &request) {
        logRequest(request);
        QJsonObject obj;
        Client c;
        QHttpServerResponse bad(Status::BadRequest);
        if (!parseBodyObject(request, obj, &bad) || !parseClient(obj, c, &bad))
            return bad;

        c.id = id;
        bool found = false;
        QString error;
        if (!repo.update(c, &found, &error))
            return dbError(error);
        if (!found)
            return errorResponse(Status::NotFound, QStringLiteral("Клиент не найден"));
        return json(c.toJson());
    });

    // ---------- DELETE /api/clients/<id> ----------
    server.route(QStringLiteral("/api/clients/<arg>"), QHttpServerRequest::Method::Delete,
                 [&repo](qint64 id, const QHttpServerRequest &request) {
        logRequest(request);
        bool found = false;
        QString error;
        if (!repo.remove(id, &found, &error))
            return dbError(error);
        if (!found)
            return errorResponse(Status::NotFound, QStringLiteral("Клиент не найден"));
        return QHttpServerResponse(Status::NoContent);
    });
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("minicrm-server"));
    QCoreApplication::setApplicationVersion(QStringLiteral(MINICRM_VERSION));
    qSetMessagePattern(QStringLiteral("%{time yyyy-MM-dd hh:mm:ss} [%{type}] %{message}"));

    // Все настройки пока передаются аргументами командной строки.
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("API-сервер мини-CRM"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption portOpt(QStringLiteral("port"),
        QStringLiteral("HTTP-порт сервера (по умолчанию 8080)."), QStringLiteral("port"),
        QStringLiteral("8080"));
    const QCommandLineOption dbHostOpt(QStringLiteral("db-host"),
        QStringLiteral("Хост PostgreSQL."), QStringLiteral("host"), QStringLiteral("localhost"));
    const QCommandLineOption dbPortOpt(QStringLiteral("db-port"),
        QStringLiteral("Порт PostgreSQL."), QStringLiteral("port"), QStringLiteral("5432"));
    const QCommandLineOption dbNameOpt(QStringLiteral("db-name"),
        QStringLiteral("Имя базы данных."), QStringLiteral("name"), QStringLiteral("minicrm"));
    const QCommandLineOption dbUserOpt(QStringLiteral("db-user"),
        QStringLiteral("Пользователь PostgreSQL."), QStringLiteral("user"),
        QStringLiteral("minicrm"));
    const QCommandLineOption dbPasswordOpt(QStringLiteral("db-password"),
        QStringLiteral("Пароль PostgreSQL."), QStringLiteral("password"),
        QStringLiteral("minicrm"));
    parser.addOptions({portOpt, dbHostOpt, dbPortOpt, dbNameOpt, dbUserOpt, dbPasswordOpt});
    parser.process(app);

    DbConfig db;
    db.host = parser.value(dbHostOpt);
    db.port = parser.value(dbPortOpt).toInt();
    db.name = parser.value(dbNameOpt);
    db.user = parser.value(dbUserOpt);
    db.password = parser.value(dbPasswordOpt);
    const quint16 port = static_cast<quint16>(parser.value(portOpt).toUInt());

    ClientRepository repo(db);
    QString error;
    if (!repo.open(&error)) {
        qCritical().noquote() << "[DB] не удалось подключиться к PostgreSQL:" << error;
        return 1;
    }

    QHttpServer server;
    registerRoutes(server, repo);

    auto *tcp = new QTcpServer(&server);
    if (!tcp->listen(QHostAddress::Any, port)) {
        qCritical().noquote() << "[HTTP] не удалось открыть порт" << port << ":"
                              << tcp->errorString();
        return 1;
    }
    server.bind(tcp);
    qInfo().noquote() << "[HTTP] MiniCRM API слушает порт" << tcp->serverPort();

    return app.exec();
}

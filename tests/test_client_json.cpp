// Тесты сериализации Client/Order в JSON: этот формат клиент и сервер
// используют для обмена по HTTP, поэтому он не должен ломаться незаметно.
// Запуск: ctest --test-dir build --output-on-failure
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QtTest>

#include "client.h"

class TestClientJson : public QObject
{
    Q_OBJECT

private:
    static Client sampleClient()
    {
        Client c;
        c.id = 42;
        c.name = QStringLiteral("Иванов Иван");
        c.phone = QStringLiteral("+7 900 123-45-67");
        c.email = QStringLiteral("ivan@example.com");
        c.company = QStringLiteral("ООО «Вектор»");
        c.notes = QStringLiteral("Перезвонить в понедельник");
        c.created = QDateTime(QDate(2026, 11, 1), QTime(10, 30), QTimeZone::utc());

        Order o;
        o.date = QDate(2026, 11, 5);
        o.description = QStringLiteral("Поставка оборудования");
        o.amount = 125000.5;
        c.orders << o;
        return c;
    }

private slots:
    void roundTrip()
    {
        const Client original = sampleClient();
        const Client restored = Client::fromJson(original.toJson());

        QCOMPARE(restored.id, original.id);
        QCOMPARE(restored.name, original.name);
        QCOMPARE(restored.phone, original.phone);
        QCOMPARE(restored.email, original.email);
        QCOMPARE(restored.company, original.company);
        QCOMPARE(restored.notes, original.notes);
        QCOMPARE(restored.created, original.created);
        QCOMPARE(restored.orders.size(), 1);
        QCOMPARE(restored.orders.first().date, original.orders.first().date);
        QCOMPARE(restored.orders.first().description, original.orders.first().description);
        QCOMPARE(restored.orders.first().amount, original.orders.first().amount);
        QCOMPARE(restored.totalAmount(), 125000.5);
    }

    void newClientHasNoId()
    {
        // Клиент, ещё не сохранённый на сервере, не отправляет поле id.
        Client c = sampleClient();
        c.id = 0;
        QVERIFY(!c.toJson().contains(QStringLiteral("id")));
        QCOMPARE(Client::fromJson(c.toJson()).id, qint64(0));
    }

    void fileRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("clients.json"));

        QString error;
        QVERIFY2(saveClientsToFile({sampleClient()}, path, &error), qPrintable(error));

        QVector<Client> loaded;
        QVERIFY2(loadClientsFromFile(path, loaded, &error), qPrintable(error));
        QCOMPARE(loaded.size(), 1);
        QCOMPARE(loaded.first().name, QStringLiteral("Иванов Иван"));
    }

    void missingFileIsNotAnError()
    {
        QVector<Client> loaded;
        QString error;
        QVERIFY(loadClientsFromFile(QStringLiteral("/nonexistent/dir/clients.json"), loaded, &error));
        QVERIFY(loaded.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestClientJson)
#include "test_client_json.moc"

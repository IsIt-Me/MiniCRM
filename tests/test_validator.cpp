// Модульные тесты Validator — ядра бизнес-логики.
// Запуск: ctest --test-dir build --output-on-failure
#include <QSignalSpy>
#include <QtTest>

#include "validator.h"

class TestValidator : public QObject
{
    Q_OBJECT

private slots:
    void name_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("valid");

        QTest::newRow("обычное имя")     << QStringLiteral("Иванов Иван") << true;
        QTest::newRow("два символа")     << QStringLiteral("Ив") << true;
        QTest::newRow("один символ")     << QStringLiteral("И") << false;
        QTest::newRow("пустое")          << QString() << false;
        QTest::newRow("только пробелы")  << QStringLiteral("    ") << false;
        QTest::newRow("101 символ")      << QString(101, QLatin1Char('a')) << false;
    }
    void name()
    {
        QFETCH(QString, name);
        QFETCH(bool, valid);
        QCOMPARE(Validator::isNameValid(name), valid);
    }

    void phone_data()
    {
        QTest::addColumn<QString>("phone");
        QTest::addColumn<bool>("valid");

        QTest::newRow("пустой допустим")   << QString() << true;
        QTest::newRow("+7 с пробелами")    << QStringLiteral("+7 900 123-45-67") << true;
        QTest::newRow("8 со скобками")     << QStringLiteral("8 (495) 765-43-21") << true;
        QTest::newRow("10 цифр")           << QStringLiteral("9001234567") << true;
        QTest::newRow("слишком короткий")  << QStringLiteral("12345") << false;
        QTest::newRow("буквы")             << QStringLiteral("+7 900 ABC-45-67") << false;
        QTest::newRow("16 цифр")           << QStringLiteral("1234567890123456") << false;
    }
    void phone()
    {
        QFETCH(QString, phone);
        QFETCH(bool, valid);
        QCOMPARE(Validator::isPhoneValid(phone), valid);
    }

    void email_data()
    {
        QTest::addColumn<QString>("email");
        QTest::addColumn<bool>("valid");

        QTest::newRow("пустой допустим") << QString() << true;
        QTest::newRow("обычный")         << QStringLiteral("ivan@example.com") << true;
        QTest::newRow("с точкой и плюс") << QStringLiteral("ivan.petrov+crm@mail.ru") << true;
        QTest::newRow("без @")           << QStringLiteral("ivan.example.com") << false;
        QTest::newRow("без домена")      << QStringLiteral("ivan@") << false;
        QTest::newRow("без зоны")        << QStringLiteral("ivan@example") << false;
    }
    void email()
    {
        QFETCH(QString, email);
        QFETCH(bool, valid);
        QCOMPARE(Validator::isEmailValid(email), valid);
    }

    void clientNeedsContact()
    {
        Validator v;
        QSignalSpy failed(&v, &Validator::validationFailed);

        Client c;
        c.name = QStringLiteral("Иванов Иван");
        QVERIFY(!v.validateClient(c));            // нет ни телефона, ни email
        QCOMPARE(failed.count(), 1);
        QCOMPARE(v.lastField(), QStringLiteral("phone"));

        c.email = QStringLiteral("ivan@example.com");
        QVERIFY(v.validateClient(c));
        QVERIFY(v.lastError().isEmpty());
    }

    void orderRules()
    {
        Validator v;
        Order o;
        o.date = QDate(2026, 11, 1);
        o.description = QStringLiteral("Поставка");
        o.amount = 1000.0;
        QVERIFY(v.validateOrder(o));

        o.amount = -1.0;
        QVERIFY(!v.validateOrder(o));             // отрицательная сумма

        o.amount = 10.0;
        o.description = QStringLiteral("   ");
        QVERIFY(!v.validateOrder(o));             // пустое описание

        o.description = QStringLiteral("Поставка");
        o.date = QDate();
        QVERIFY(!v.validateOrder(o));             // нет даты
    }
};

QTEST_GUILESS_MAIN(TestValidator)
#include "test_validator.moc"

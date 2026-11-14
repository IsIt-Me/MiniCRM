#ifndef VALIDATOR_H
#define VALIDATOR_H

#include <QObject>
#include <QString>

#include "client.h"

// Validator — ядро бизнес-логики мини-CRM: проверка данных клиента
// перед добавлением в базу. Наследуется от QObject, чтобы сообщать об
// ошибках сигналом validationFailed (как Calculator::errorOccurred в ЛР1).
class Validator : public QObject
{
    Q_OBJECT

public:
    explicit Validator(QObject *parent = nullptr);

    // Проверки отдельных полей. При ошибке излучают validationFailed.
    bool validateName(const QString &name);
    bool validatePhone(const QString &phone);
    bool validateEmail(const QString &email);
    bool validateOrder(const Order &order);

    // Полная проверка клиента: имя, телефон/email, заказы.
    bool validateClient(const Client &client);

    QString lastError() const { return m_lastError; }
    QString lastField() const { return m_lastField; }

    // Статические «тихие» проверки без сигналов — для модели и делегата.
    static bool isNameValid(const QString &name);
    static bool isPhoneValid(const QString &phone);
    static bool isEmailValid(const QString &email);

    static constexpr int kMinNameLength = 2;
    static constexpr int kMaxNameLength = 100;

signals:
    // field — имя поля ("name", "phone", "email", "order"), message — текст ошибки.
    void validationFailed(const QString &field, const QString &message);
    // Клиент целиком прошёл проверку.
    void validationPassed(const QString &clientName);

private:
    bool fail(const QString &field, const QString &message);

    QString m_lastError;
    QString m_lastField;
};

#endif // VALIDATOR_H

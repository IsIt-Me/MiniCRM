#include "validator.h"

#include <QDebug>
#include <QRegularExpression>

Validator::Validator(QObject *parent)
    : QObject(parent)
{
}

// ------------------------ статические проверки ------------------------

bool Validator::isNameValid(const QString &name)
{
    const QString n = name.trimmed();
    return n.size() >= kMinNameLength && n.size() <= kMaxNameLength;
}

bool Validator::isPhoneValid(const QString &phone)
{
    const QString p = phone.trimmed();
    if (p.isEmpty())
        return true;   // телефон не обязателен

    // Разрешены цифры, пробелы, дефисы, скобки и «+» в начале.
    static const QRegularExpression allowed(QStringLiteral("^\\+?[0-9\\s\\-()]+$"));
    if (!allowed.match(p).hasMatch())
        return false;

    int digits = 0;
    for (const QChar ch : p) {
        if (ch.isDigit())
            ++digits;
    }
    return digits >= 10 && digits <= 15;   // 10 цифр — 9001234567, 11 — +7 900 123-45-67
}

bool Validator::isEmailValid(const QString &email)
{
    const QString e = email.trimmed();
    if (e.isEmpty())
        return true;   // email не обязателен

    static const QRegularExpression re(
        QStringLiteral("^[A-Za-z0-9._%+\\-]+@[A-Za-z0-9.\\-]+\\.[A-Za-z]{2,}$"));
    return re.match(e).hasMatch();
}

// -------------------- проверки с сигналами об ошибках --------------------

bool Validator::fail(const QString &field, const QString &message)
{
    m_lastField = field;
    m_lastError = message;
    qDebug() << "[VALIDATOR] ошибка в поле" << field << ":" << message;
    emit validationFailed(field, message);
    return false;
}

bool Validator::validateName(const QString &name)
{
    const QString n = name.trimmed();
    if (n.isEmpty())
        return fail(QStringLiteral("name"), tr("Имя клиента не может быть пустым"));
    if (n.size() < kMinNameLength)
        return fail(QStringLiteral("name"),
                    tr("Имя слишком короткое (минимум %1 символа)").arg(kMinNameLength));
    if (n.size() > kMaxNameLength)
        return fail(QStringLiteral("name"),
                    tr("Имя слишком длинное (максимум %1 символов)").arg(kMaxNameLength));
    return true;
}

bool Validator::validatePhone(const QString &phone)
{
    if (!isPhoneValid(phone))
        return fail(QStringLiteral("phone"),
                    tr("Некорректный телефон «%1». Пример: +7 900 123-45-67").arg(phone));
    return true;
}

bool Validator::validateEmail(const QString &email)
{
    if (!isEmailValid(email))
        return fail(QStringLiteral("email"),
                    tr("Некорректный email «%1». Пример: ivan@example.com").arg(email));
    return true;
}

bool Validator::validateOrder(const Order &order)
{
    if (order.description.trimmed().isEmpty())
        return fail(QStringLiteral("order"), tr("У заказа должно быть описание"));
    if (order.amount < 0.0)
        return fail(QStringLiteral("order"), tr("Сумма заказа не может быть отрицательной"));
    if (!order.date.isValid())
        return fail(QStringLiteral("order"), tr("У заказа некорректная дата"));
    return true;
}

bool Validator::validateClient(const Client &client)
{
    if (!validateName(client.name))
        return false;

    if (client.phone.trimmed().isEmpty() && client.email.trimmed().isEmpty())
        return fail(QStringLiteral("phone"),
                    tr("Укажите хотя бы один контакт: телефон или email"));

    if (!validatePhone(client.phone) || !validateEmail(client.email))
        return false;

    for (const Order &o : client.orders) {
        if (!validateOrder(o))
            return false;
    }

    m_lastError.clear();
    m_lastField.clear();
    emit validationPassed(client.name.trimmed());
    return true;
}

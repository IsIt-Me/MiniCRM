#include "itemmodel.h"

#include "validator.h"

#include <QDebug>
#include <QStringList>

ItemModel::ItemModel(QObject *parent)
    : QStringListModel(parent)
{
}

QVariant ItemModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};

    // Имя (Display/Edit) хранит сам QStringListModel.
    if (role == Qt::DisplayRole || role == Qt::EditRole)
        return QStringListModel::data(index, role);

    const int row = index.row();
    if (!isValidRow(row))
        return {};

    const Client &c = m_clients.at(row);
    switch (role) {
    case Qt::ToolTipRole:  return c.summary();
    case PhoneRole:        return c.phone;
    case EmailRole:        return c.email;
    case CompanyRole:      return c.company;
    case NotesRole:        return c.notes;
    case OrdersCountRole:  return c.orders.size();
    case TotalAmountRole:  return c.totalAmount();
    default:               return QStringListModel::data(index, role);
    }
}

bool ItemModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || (role != Qt::EditRole && role != Qt::DisplayRole))
        return false;

    const int row = index.row();
    const QString name = value.toString().trimmed();

    // Бизнес-правило из Validator: пустое/слишком короткое имя не сохраняем.
    if (!Validator::isNameValid(name)) {
        qDebug() << "[MODEL] setData отклонён: некорректное имя" << name;
        return false;
    }
    if (!isValidRow(row))
        return false;

    const QString oldName = m_clients[row].name;
    m_clients[row].name = name;
    if (!QStringListModel::setData(index, name, role)) {   // излучит dataChanged
        m_clients[row].name = oldName;
        return false;
    }

    qDebug() << "[MODEL] строка" << row << ":" << oldName << "->" << name;
    emit contentsModified();
    return true;
}

bool ItemModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if (count < 1 || row < 0 || row > m_clients.size() || parent.isValid())
        return false;

    // Вектор обновляем ДО базового вызова: к моменту сигнала rowsInserted
    // данные строк и карточки клиентов уже согласованы.
    m_clients.insert(row, count, Client());
    if (!QStringListModel::insertRows(row, count, parent)) {
        m_clients.remove(row, count);
        return false;
    }
    emit contentsModified();
    return true;
}

bool ItemModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (count < 1 || row < 0 || row + count > m_clients.size() || parent.isValid())
        return false;

    const QVector<Client> backup = m_clients.mid(row, count);
    m_clients.remove(row, count);
    if (!QStringListModel::removeRows(row, count, parent)) {
        for (int i = 0; i < backup.size(); ++i)
            m_clients.insert(row + i, backup.at(i));
        return false;
    }
    emit contentsModified();
    return true;
}

Qt::ItemFlags ItemModel::flags(const QModelIndex &index) const
{
    // Перетаскивание отключаем: оно переставляет строки в обход m_clients.
    return QStringListModel::flags(index) & ~(Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
}

int ItemModel::addClient(const Client &client)
{
    const int row = rowCount();
    if (!insertRows(row, 1))
        return -1;

    m_clients[row] = client;
    m_clients[row].name = client.name.trimmed();
    if (!m_clients[row].created.isValid())
        m_clients[row].created = QDateTime::currentDateTime();

    // Базовый setData: имя уже проверено Validator'ом в главном окне.
    QStringListModel::setData(index(row, 0), m_clients[row].name, Qt::EditRole);
    return row;
}

Client ItemModel::client(int row) const
{
    return isValidRow(row) ? m_clients.at(row) : Client();
}

void ItemModel::removeClient(int row)
{
    removeRows(row, 1);
}

void ItemModel::clearAll()
{
    m_clients.clear();
    setStringList({});   // modelReset -> view очистится сам
    emit contentsModified();
}

void ItemModel::updateIds(const QVector<Client> &clients)
{
    const int count = qMin(clients.size(), m_clients.size());
    for (int i = 0; i < count; ++i)
        m_clients[i].id = clients.at(i).id;
}

void ItemModel::setClients(const QVector<Client> &clients)
{
    m_clients = clients;
    QStringList names;
    names.reserve(clients.size());
    for (const Client &c : clients)
        names << c.name;
    setStringList(names);   // modelReset
}

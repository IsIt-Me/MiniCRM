#ifndef ITEMMODEL_H
#define ITEMMODEL_H

#include <QStringListModel>
#include <QVector>

#include "client.h"

// Модель списка клиентов.
//
// Основа — QStringListModel: он хранит отображаемые строки (имена клиентов),
// сам излучает rowsInserted / rowsRemoved / dataChanged / modelReset и
// поддерживает редактирование (EditRole). Полные карточки клиентов лежат
// в параллельном векторе m_clients, который синхронизируется в
// insertRows / removeRows / setData. Дополнительные поля (телефон, email…)
// отдаются через пользовательские роли — их читает делегат.
class ItemModel : public QStringListModel
{
    Q_OBJECT

public:
    enum Roles {
        PhoneRole = Qt::UserRole + 1,
        EmailRole,
        CompanyRole,
        NotesRole,
        OrdersCountRole,
        TotalAmountRole
    };

    explicit ItemModel(QObject *parent = nullptr);

    // --- переопределения QStringListModel ---
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole) override;
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // --- API для главного окна ---
    int addClient(const Client &client);      // возвращает номер новой строки
    Client client(int row) const;
    void removeClient(int row);
    void clearAll();
    void setClients(const QVector<Client> &clients);
    QVector<Client> clients() const { return m_clients; }
    int clientCount() const { return m_clients.size(); }

    // Записать id, которые сервер назначил новым клиентам (порядок строк тот же).
    // Сигналов не излучает: отображаемые данные не меняются.
    void updateIds(const QVector<Client> &clients);

signals:
    // Любое изменение данных — для флага «есть несохранённые изменения».
    void contentsModified();

private:
    bool isValidRow(int row) const { return row >= 0 && row < m_clients.size(); }

    QVector<Client> m_clients;   // карточки клиентов, строка i <-> m_clients[i]
};

#endif // ITEMMODEL_H

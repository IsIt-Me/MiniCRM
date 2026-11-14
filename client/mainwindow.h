#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QModelIndex>   // полный тип нужен moc для аргумента слота (Qt 6)

#include "client.h"

class QAction;
class QLabel;
class QTimer;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class Validator;
class ItemModel;
class ApiClient;
class QListView;

// Главное окно мини-CRM: меню, панель инструментов, статус-бар,
// список клиентов (Model-View), карточка клиента, автосохранение.
// Данные хранятся на API-сервере (PostgreSQL), обмен — через ApiClient.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;   // сохранение при выходе

private slots:
    void saveData();            // ручная синхронизация с сервером (Ctrl+S)
    void reloadData();          // F5: заново загрузить клиентов с сервера
    void onAutosaveTimeout();   // срабатывание таймера автосохранения
    void showAbout();
    void newClient();           // Ctrl+N: очистить карточку для нового клиента
    void addClient();           // добавить клиента из карточки
    void deleteClient();        // Ctrl+D: удалить выбранного клиента
    void openSettings();        // настройки (интервал автосохранения)
    void clearAll();            // очистить базу с подтверждением
    void onCurrentChanged(const QModelIndex &current);   // показать карточку выбранного

private:
    void setupActions();
    void setupMenus();
    void setupToolBar();
    void setupCentral();
    void setupStatusBar();
    void setupActionLogging();   // лямбда-слоты логирования (ЛР3)
    void setupConnections();
    void setupAutosave();

    bool loadData();
    bool syncToServer(QString *error = nullptr);
    QVector<Client> currentClients() const;
    int clientCount() const;
    void setDirty(bool dirty);
    void updateStatus();
    Client clientFromForm() const;
    void clearForm();

    static void logAction(const QString &text);

    // --- данные ---
    ItemModel *m_model = nullptr;          // единственный источник данных о клиентах
    ApiClient *m_api = nullptr;            // HTTP-клиент к серверу
    bool m_dirty = false;                  // есть несохранённые изменения
    bool m_loaded = false;                 // данные успешно загружены с сервера
    bool m_syncing = false;                // идёт запрос к серверу

    // --- автосохранение ---
    QTimer *m_autosaveTimer = nullptr;
    int m_autosaveIntervalSec = 60;

    // --- команды меню и панели инструментов ---
    QAction *m_actSave = nullptr;
    QAction *m_actReload = nullptr;
    QAction *m_actExit = nullptr;
    QAction *m_actAbout = nullptr;
    QAction *m_actNew = nullptr;
    QAction *m_actDelete = nullptr;
    QAction *m_actSettings = nullptr;
    QAction *m_actClear = nullptr;

    // --- бизнес-логика ---
    Validator *m_validator = nullptr;

    // --- карточка клиента ---
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_phoneEdit = nullptr;
    QLineEdit *m_emailEdit = nullptr;
    QLineEdit *m_companyEdit = nullptr;
    QPlainTextEdit *m_notesEdit = nullptr;

    // --- кнопки ---
    QPushButton *m_addBtn = nullptr;
    QPushButton *m_deleteBtn = nullptr;
    QPushButton *m_saveBtn = nullptr;
    QPushButton *m_settingsBtn = nullptr;
    QPushButton *m_clearBtn = nullptr;

    // --- список клиентов ---
    QListView *m_listView = nullptr;
    QLabel *m_detailsLabel = nullptr;

    // --- статус-бар ---
    QLabel *m_countLabel = nullptr;
    QLabel *m_autosaveLabel = nullptr;
};

#endif // MAINWINDOW_H

#include "mainwindow.h"

#include "validator.h"
#include "itemmodel.h"
#include "itemdelegate.h"
#include "apiclient.h"

#include <QAction>
#include <QCloseEvent>
#include <QDateTime>
#include <QDebug>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QStyle>
#include <QTime>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QItemSelectionModel>
#include <QListView>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // [*] — место, где Qt покажет звёздочку при setWindowModified(true).
    // Заголовок задаём до loadData(): там вызывается setWindowModified().
    setWindowTitle(tr("Мини-CRM — база клиентов[*]"));

    m_validator = new Validator(this);   // родитель = окно (ЛР2)
    m_api = new ApiClient(this);         // HTTP-клиент к API-серверу

    setupActions();
    setupMenus();
    setupToolBar();
    setupCentral();
    setupStatusBar();
    setupActionLogging();   // логирующие лямбды подключаем ПЕРВЫМИ — лог идёт до действия
    setupConnections();

    loadData();
    setupAutosave();
    updateStatus();

    resize(900, 600);

    logAction(tr("Приложение запущено, сервер: %1").arg(m_api->serverUrl()));
}

MainWindow::~MainWindow()
{
    qDebug() << "[DESTROY] MainWindow";
}

// ============================================================================
//                              Построение интерфейса
// ============================================================================

void MainWindow::setupActions()
{
    m_actSave = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton),
                            tr("&Сохранить"), this);
    m_actSave->setShortcut(QKeySequence::Save);                    // Ctrl+S
    m_actSave->setStatusTip(tr("Сохранить изменения на сервере"));

    m_actReload = new QAction(style()->standardIcon(QStyle::SP_BrowserReload),
                              tr("О&бновить с сервера"), this);
    m_actReload->setShortcut(QKeySequence::Refresh);               // F5
    m_actReload->setStatusTip(tr("Заново загрузить клиентов с сервера"));

    m_actExit = new QAction(tr("В&ыход"), this);
    m_actExit->setShortcut(QKeySequence(tr("Ctrl+Q")));
    m_actExit->setStatusTip(tr("Закрыть программу"));

    m_actAbout = new QAction(tr("&О программе"), this);

    m_actNew = new QAction(style()->standardIcon(QStyle::SP_FileIcon),
                           tr("&Новый клиент"), this);
    m_actNew->setShortcut(QKeySequence::New);                      // Ctrl+N
    m_actNew->setStatusTip(tr("Очистить карточку для ввода нового клиента"));

    m_actDelete = new QAction(style()->standardIcon(QStyle::SP_TrashIcon),
                              tr("&Удалить клиента"), this);
    m_actDelete->setShortcut(QKeySequence(tr("Ctrl+D")));
    m_actDelete->setStatusTip(tr("Удалить выбранного клиента"));

    m_actSettings = new QAction(style()->standardIcon(QStyle::SP_FileDialogDetailedView),
                                tr("&Настройки…"), this);
    m_actSettings->setStatusTip(tr("Интервал автосохранения"));

    m_actClear = new QAction(tr("&Очистить базу…"), this);
    m_actClear->setShortcut(QKeySequence(tr("Ctrl+Shift+Del")));
    m_actClear->setStatusTip(tr("Удалить всех клиентов"));
}

void MainWindow::setupMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&Файл"));
    fileMenu->addAction(m_actNew);
    fileMenu->addAction(m_actSave);
    fileMenu->addAction(m_actReload);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actExit);

    QMenu *editMenu = menuBar()->addMenu(tr("&Правка"));
    editMenu->addAction(m_actDelete);
    editMenu->addAction(m_actClear);

    QMenu *toolsMenu = menuBar()->addMenu(tr("&Сервис"));
    toolsMenu->addAction(m_actSettings);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Справка"));
    helpMenu->addAction(m_actAbout);
}

void MainWindow::setupToolBar()
{
    QToolBar *toolBar = addToolBar(tr("Основная панель"));
    toolBar->setObjectName(QStringLiteral("mainToolBar"));
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolBar->addAction(m_actNew);
    toolBar->addAction(m_actSave);
    toolBar->addAction(m_actReload);
    toolBar->addAction(m_actDelete);
    toolBar->addSeparator();
    toolBar->addAction(m_actSettings);
}

void MainWindow::setupCentral()
{
    auto *central = new QWidget(this);

    // Корневой вертикальный layout: [содержимое] / [панель кнопок].
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    // Горизонтальный layout содержимого: список слева, карточка справа.
    auto *content = new QHBoxLayout();
    content->setSpacing(12);

    // ---------- Список клиентов (Model-View) ----------
    auto *listBox = new QGroupBox(tr("Клиенты"), central);
    auto *listLayout = new QVBoxLayout(listBox);

    m_model = new ItemModel(this);               // данные живут в модели
    m_listView = new QListView(listBox);         // view только показывает
    m_listView->setModel(m_model);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setUniformItemSizes(true);
    // Редактирование имени — только по двойному клику, через наш делегат.
    m_listView->setEditTriggers(QAbstractItemView::DoubleClicked);
    m_listView->setItemDelegate(new ItemDelegate(m_listView));   // родитель = view

    m_detailsLabel = new QLabel(listBox);
    m_detailsLabel->setWordWrap(true);
    m_detailsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_detailsLabel->setMinimumHeight(70);
    m_detailsLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    listLayout->addWidget(m_listView, 1);        // список тянется по высоте
    listLayout->addWidget(m_detailsLabel);
    content->addWidget(listBox, 3);              // список шире карточки (3 : 2)

    // ---------- Карточка клиента (QFormLayout) ----------
    auto *formBox = new QGroupBox(tr("Карточка клиента"), central);
    auto *form = new QFormLayout(formBox);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);   // поля тянутся
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);                // узкое окно: метка над полем

    m_nameEdit = new QLineEdit(formBox);
    m_nameEdit->setPlaceholderText(tr("Иванов Иван / ООО «Ромашка»"));
    m_nameEdit->setMaxLength(Validator::kMaxNameLength);

    m_phoneEdit = new QLineEdit(formBox);
    m_phoneEdit->setPlaceholderText(tr("+7 900 123-45-67"));

    m_emailEdit = new QLineEdit(formBox);
    m_emailEdit->setPlaceholderText(tr("ivan@example.com"));

    m_companyEdit = new QLineEdit(formBox);
    m_companyEdit->setPlaceholderText(tr("Компания (необязательно)"));

    m_notesEdit = new QPlainTextEdit(formBox);
    m_notesEdit->setPlaceholderText(tr("Заметки менеджера…"));
    m_notesEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    form->addRow(tr("&Имя*:"), m_nameEdit);
    form->addRow(tr("&Телефон:"), m_phoneEdit);
    form->addRow(tr("&Email:"), m_emailEdit);
    form->addRow(tr("&Компания:"), m_companyEdit);
    form->addRow(tr("&Заметки:"), m_notesEdit);

    auto *hint = new QLabel(tr("* — обязательное поле. Нужен телефон или email."), formBox);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: gray;"));
    form->addRow(hint);

    content->addWidget(formBox, 2);
    root->addLayout(content, 1);   // содержимое забирает всю свободную высоту

    // ---------- Панель кнопок (QHBoxLayout) ----------
    // [Добавить][Удалить]...пружина...[Сохранить][Настройки]
    auto *buttons = new QHBoxLayout();
    m_addBtn = new QPushButton(style()->standardIcon(QStyle::SP_DialogApplyButton),
                               tr("Добавить"), central);
    m_deleteBtn = new QPushButton(style()->standardIcon(QStyle::SP_TrashIcon),
                                  tr("Удалить"), central);
    m_clearBtn = new QPushButton(style()->standardIcon(QStyle::SP_DialogResetButton),
                                 tr("Очистить"), central);
    m_saveBtn = new QPushButton(style()->standardIcon(QStyle::SP_DialogSaveButton),
                                tr("Сохранить"), central);
    m_settingsBtn = new QPushButton(style()->standardIcon(QStyle::SP_FileDialogDetailedView),
                                    tr("Настройки"), central);
    m_addBtn->setDefault(true);

    buttons->addWidget(m_addBtn);
    buttons->addWidget(m_deleteBtn);
    buttons->addWidget(m_clearBtn);
    buttons->addStretch();          // прижимает «Сохранить/Настройки» вправо
    buttons->addWidget(m_saveBtn);
    buttons->addWidget(m_settingsBtn);
    root->addLayout(buttons);

    setCentralWidget(central);
}

void MainWindow::setupStatusBar()
{
    m_countLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_countLabel);
    m_autosaveLabel = new QLabel(tr("Автосохранение: каждые %1 с").arg(m_autosaveIntervalSec), this);
    statusBar()->addPermanentWidget(m_autosaveLabel);
    statusBar()->showMessage(tr("Готово"), 3000);
}

// ЛР3: лямбда-слоты для логирования действий пользователя в консоль.
// Лямбда захватывает только указатель на отправителя, контекст = this:
// при уничтожении окна соединения разорвутся автоматически.
void MainWindow::setupActionLogging()
{
    const QList<QAction*> actions = {
        m_actSave, m_actReload, m_actExit, m_actAbout,
        m_actNew, m_actDelete, m_actSettings,
        m_actClear,
    };
    for (QAction *action : actions) {
        connect(action, &QAction::triggered, this, [action]() {
            QString text = action->text();
            text.remove(QLatin1Char('&'));
            const QString keys = action->shortcut().toString(QKeySequence::NativeText);
            logAction(keys.isEmpty() ? QStringLiteral("Команда «%1»").arg(text)
                                     : QStringLiteral("Команда «%1» (%2)").arg(text, keys));
        });
    }

    const QList<QPushButton*> buttons = {
        m_addBtn, m_deleteBtn, m_saveBtn, m_settingsBtn,
        m_clearBtn,
    };
    for (QPushButton *button : buttons) {
        connect(button, &QPushButton::clicked, this, [button]() {
            logAction(QStringLiteral("Кнопка «%1»").arg(button->text()));
        });
    }

    connect(m_listView, &QAbstractItemView::clicked, this, [](const QModelIndex &index) {
        logAction(QStringLiteral("Выбран клиент «%1» (строка %2)")
                      .arg(index.data(Qt::DisplayRole).toString())
                      .arg(index.row()));
    });
    connect(m_listView, &QAbstractItemView::doubleClicked, this, [](const QModelIndex &index) {
        logAction(QStringLiteral("Двойной клик: редактирование «%1»")
                      .arg(index.data(Qt::DisplayRole).toString()));
    });
}

void MainWindow::setupConnections()
{
    connect(m_actSave,  &QAction::triggered, this, &MainWindow::saveData);
    connect(m_actReload, &QAction::triggered, this, &MainWindow::reloadData);
    connect(m_actExit,  &QAction::triggered, this, &MainWindow::close);
    connect(m_actAbout, &QAction::triggered, this, &MainWindow::showAbout);
    connect(m_actNew,      &QAction::triggered, this, &MainWindow::newClient);
    connect(m_actDelete,   &QAction::triggered, this, &MainWindow::deleteClient);
    connect(m_actSettings, &QAction::triggered, this, &MainWindow::openSettings);

    connect(m_addBtn,      &QPushButton::clicked, this, &MainWindow::addClient);
    connect(m_deleteBtn,   &QPushButton::clicked, this, &MainWindow::deleteClient);
    connect(m_saveBtn,     &QPushButton::clicked, this, &MainWindow::saveData);
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::openSettings);

    // Enter в любом однострочном поле карточки = «Добавить».
    for (QLineEdit *edit : {m_nameEdit, m_phoneEdit, m_emailEdit, m_companyEdit})
        connect(edit, &QLineEdit::returnPressed, this, &MainWindow::addClient);

    // Ошибки валидации: сообщение пользователю и фокус на проблемное поле.
    connect(m_validator, &Validator::validationFailed, this,
            [this](const QString &field, const QString &message) {
                QLineEdit *target = m_nameEdit;
                if (field == QLatin1String("phone")) target = m_phoneEdit;
                if (field == QLatin1String("email")) target = m_emailEdit;
                statusBar()->showMessage(message, 5000);
                QMessageBox::warning(this, tr("Проверка данных"), message);
                target->setFocus();
                target->selectAll();
            });
    connect(m_actClear, &QAction::triggered,   this, &MainWindow::clearAll);
    connect(m_clearBtn, &QPushButton::clicked, this, &MainWindow::clearAll);

    // Модель изменилась -> есть несохранённые данные, обновить статус.
    connect(m_model, &ItemModel::contentsModified, this, [this]() {
        setDirty(true);
        updateStatus();
    });
    // Выбор строки -> показать подробности клиента.
    connect(m_listView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MainWindow::onCurrentChanged);
    // Имя поменяли делегатом -> обновить блок подробностей.
    connect(m_model, &QAbstractItemModel::dataChanged, this, [this]() {
        onCurrentChanged(m_listView->currentIndex());
    });
    connect(m_model, &QAbstractItemModel::modelReset, this, [this]() {
        onCurrentChanged(QModelIndex());
    });
}

// ============================================================================
//                       ЛР3: автосохранение через QTimer
// ============================================================================

void MainWindow::setupAutosave()
{
    m_autosaveTimer = new QTimer(this);   // родитель = окно: удалится вместе с ним
    m_autosaveTimer->setInterval(m_autosaveIntervalSec * 1000);
    connect(m_autosaveTimer, &QTimer::timeout, this, &MainWindow::onAutosaveTimeout);
    m_autosaveTimer->start();
    qDebug() << "[AUTOSAVE] таймер запущен, интервал" << m_autosaveIntervalSec << "с";
}

void MainWindow::onAutosaveTimeout()
{
    const QString now = QTime::currentTime().toString(QStringLiteral("hh:mm:ss"));

    if (m_syncing)
        return;   // идёт запрос к серверу — попробуем в следующий раз

    if (!m_dirty) {
        qDebug() << "[AUTOSAVE]" << now << "изменений нет — запись не нужна";
        m_autosaveLabel->setText(tr("Автосохранение: проверено в %1").arg(now));
        return;
    }

    QString error;
    if (syncToServer(&error)) {
        qDebug() << "[AUTOSAVE]" << now << "сохранено на сервере клиентов:" << clientCount();
        m_autosaveLabel->setText(tr("Автосохранение: %1").arg(now));
        statusBar()->showMessage(tr("Данные автоматически сохранены"), 2000);
    } else {
        qWarning() << "[AUTOSAVE] ошибка сохранения на сервере:" << error;
        m_autosaveLabel->setText(tr("Автосохранение: ОШИБКА в %1").arg(now));
    }
}

// ============================================================================
//                                 Данные
// ============================================================================

bool MainWindow::loadData()
{
    QVector<Client> loaded;
    QString error;
    m_syncing = true;
    const bool ok = m_api->fetchClients(loaded, &error);
    m_syncing = false;

    if (!ok) {
        // Без загруженных данных сохранение отключено: иначе синхронизация
        // (PUT /api/clients) заменила бы всю базу на сервере пустым списком.
        m_loaded = false;
        m_model->setClients({});
        setDirty(false);
        qWarning() << "[DATA] не удалось загрузить клиентов с сервера:" << error;
        statusBar()->showMessage(tr("Нет связи с сервером"), 5000);
        QMessageBox::warning(this, tr("Загрузка данных"),
                             tr("Не удалось загрузить клиентов с сервера %1:\n%2\n\n"
                                "Запустите сервер и нажмите «Обновить с сервера» (F5).")
                                 .arg(m_api->serverUrl(), error));
        return false;
    }

    m_loaded = true;
    m_model->setClients(loaded);
    setDirty(false);
    qDebug() << "[DATA] загружено клиентов с сервера:" << loaded.size();
    return true;
}

bool MainWindow::syncToServer(QString *error)
{
    if (m_syncing) {
        if (error) *error = tr("Предыдущий запрос к серверу ещё не завершён");
        return false;
    }
    if (!m_loaded) {
        if (error) *error = tr("Данные не загружены с сервера, сохранение отключено. "
                               "Нажмите «Обновить с сервера» (F5).");
        return false;
    }

    QVector<Client> clients = m_model->clients();
    m_syncing = true;
    const bool ok = m_api->syncClients(clients, error);
    m_syncing = false;
    if (!ok)
        return false;

    m_model->updateIds(clients);   // новые клиенты получили id на сервере
    setDirty(false);
    return true;
}

void MainWindow::reloadData()
{
    if (m_syncing)
        return;
    if (m_dirty) {
        const auto answer = QMessageBox::question(
            this, tr("Обновить с сервера"),
            tr("Есть несохранённые изменения, они будут потеряны. Продолжить?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }
    if (loadData())
        statusBar()->showMessage(tr("Загружено клиентов: %1").arg(clientCount()), 3000);
    updateStatus();
}

QVector<Client> MainWindow::currentClients() const
{
    return m_model->clients();
}

int MainWindow::clientCount() const
{
    return m_model->clientCount();
}

void MainWindow::setDirty(bool dirty)
{
    m_dirty = dirty;
    setWindowModified(dirty);   // звёздочка в заголовке
}

void MainWindow::updateStatus()
{
    m_countLabel->setText(tr("Клиентов: %1").arg(clientCount()));

    const bool hasSelection = m_listView->currentIndex().isValid();
    m_deleteBtn->setEnabled(hasSelection);
    m_actDelete->setEnabled(hasSelection);
    m_clearBtn->setEnabled(clientCount() > 0);
    m_actClear->setEnabled(clientCount() > 0);
}

void MainWindow::saveData()
{
    QString error;
    if (syncToServer(&error)) {
        statusBar()->showMessage(tr("Сохранено на сервере клиентов: %1").arg(clientCount()), 3000);
        qDebug() << "[DATA] сохранено вручную на сервере" << m_api->serverUrl();
    } else {
        QMessageBox::critical(this, tr("Сохранение"),
                              tr("Не удалось сохранить данные:\n%1").arg(error));
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_dirty) {
        QString error;
        if (syncToServer(&error)) {
            qDebug() << "[DATA] несохранённые изменения отправлены на сервер при выходе";
        } else {
            qWarning() << "[DATA] ошибка сохранения при выходе:" << error;
            const auto answer = QMessageBox::warning(
                this, tr("Выход"),
                tr("Не удалось сохранить изменения на сервере:\n%1\n\nВыйти без сохранения?")
                    .arg(error),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                event->ignore();
                return;
            }
        }
    }
    logAction(tr("Приложение закрыто"));
    event->accept();
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("О программе"),
                       tr("<b>Мини-CRM</b> — база клиентов<br>"
                          "Сквозной проект по Qt, вариант 20.<br><br>"
                          "Сервер:<br><code>%1</code>")
                           .arg(m_api->serverUrl().toHtmlEscaped()));
}

void MainWindow::logAction(const QString &text)
{
    qInfo().noquote() << QStringLiteral("[ACTION %1] %2")
                             .arg(QTime::currentTime().toString(QStringLiteral("hh:mm:ss")), text);
}

// ============================================================================
//                              Операции с клиентами
// ============================================================================

Client MainWindow::clientFromForm() const
{
    Client c;
    c.name = m_nameEdit->text().trimmed();
    c.phone = m_phoneEdit->text().trimmed();
    c.email = m_emailEdit->text().trimmed();
    c.company = m_companyEdit->text().trimmed();
    c.notes = m_notesEdit->toPlainText().trimmed();
    c.created = QDateTime::currentDateTime();
    return c;
}

void MainWindow::clearForm()
{
    m_nameEdit->clear();
    m_phoneEdit->clear();
    m_emailEdit->clear();
    m_companyEdit->clear();
    m_notesEdit->clear();
    m_nameEdit->setFocus();
}

void MainWindow::newClient()
{
    clearForm();
    statusBar()->showMessage(tr("Заполните карточку и нажмите «Добавить»"), 3000);
}

void MainWindow::addClient()
{
    const Client c = clientFromForm();

    // Ядро бизнес-логики из ЛР1: при ошибке Validator излучит validationFailed,
    // обработчик покажет сообщение и поставит фокус на нужное поле.
    if (!m_validator->validateClient(c))
        return;

    // Меняем МОДЕЛЬ, а не view: список обновится сам по сигналу rowsInserted.
    const int row = m_model->addClient(c);
    m_listView->setCurrentIndex(m_model->index(row, 0));

    clearForm();
    updateStatus();
    statusBar()->showMessage(tr("Клиент «%1» добавлен").arg(c.name), 3000);
    qDebug() << "[CRUD] добавлен клиент:" << c.summary();
}

void MainWindow::deleteClient()
{
    const QModelIndex current = m_listView->currentIndex();
    if (!current.isValid()) {
        statusBar()->showMessage(tr("Выберите клиента в списке"), 3000);
        return;
    }

    const QString name = current.data(Qt::DisplayRole).toString();
    const auto answer = QMessageBox::question(
        this, tr("Удаление клиента"),
        tr("Удалить клиента «%1»?").arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        qDebug() << "[CRUD] удаление отменено пользователем";
        return;
    }

    m_model->removeClient(current.row());   // view обновится по rowsRemoved
    updateStatus();
    statusBar()->showMessage(tr("Клиент «%1» удалён").arg(name), 3000);
    qDebug() << "[CRUD] удалён клиент" << name << ", осталось:" << m_model->clientCount();
}

void MainWindow::clearAll()
{
    const int count = m_model->clientCount();
    if (count == 0)
        return;

    const auto answer = QMessageBox::warning(
        this, tr("Очистка базы"),
        tr("Удалить всех клиентов (%1)?\nДействие нельзя отменить.").arg(count),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        qDebug() << "[CRUD] очистка отменена пользователем";
        return;
    }

    m_model->clearAll();   // modelReset -> список очистится сам
    updateStatus();
    statusBar()->showMessage(tr("База клиентов очищена"), 3000);
    qDebug() << "[CRUD] база очищена, удалено клиентов:" << count;
}

void MainWindow::openSettings()
{
    bool ok = false;
    const int seconds = QInputDialog::getInt(
        this, tr("Настройки"), tr("Интервал автосохранения, секунд:"),
        m_autosaveIntervalSec, 10, 3600, 10, &ok);
    if (!ok)
        return;

    m_autosaveIntervalSec = seconds;
    m_autosaveTimer->start(seconds * 1000);   // start(ms) перезапускает с новым интервалом
    m_autosaveLabel->setText(tr("Автосохранение: каждые %1 с").arg(seconds));
    qDebug() << "[SETTINGS] интервал автосохранения ->" << seconds << "с";
}

void MainWindow::onCurrentChanged(const QModelIndex &current)
{
    if (!current.isValid()) {
        m_detailsLabel->setText(tr("Выберите клиента, чтобы увидеть подробности."));
        updateStatus();
        return;
    }

    const Client c = m_model->client(current.row());
    QStringList lines;
    lines << QStringLiteral("<b>%1</b>").arg(c.name.toHtmlEscaped());
    if (!c.company.isEmpty())
        lines << tr("Компания: %1").arg(c.company.toHtmlEscaped());
    if (!c.phone.isEmpty())
        lines << tr("Телефон: %1").arg(c.phone.toHtmlEscaped());
    if (!c.email.isEmpty())
        lines << tr("Email: %1").arg(c.email.toHtmlEscaped());
    lines << tr("Заказов: %1 на сумму %2 ₽")
                 .arg(c.orders.size())
                 .arg(QString::number(c.totalAmount(), 'f', 2));
    if (c.created.isValid())
        lines << tr("В базе с %1").arg(c.created.toString(QStringLiteral("dd.MM.yyyy")));
    if (!c.notes.isEmpty())
        lines << QStringLiteral("<i>%1</i>").arg(c.notes.toHtmlEscaped());

    m_detailsLabel->setText(lines.join(QStringLiteral("<br>")));
    updateStatus();
}

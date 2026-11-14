#include "itemdelegate.h"

#include "validator.h"

#include <QDebug>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QColor>
#include <QFontMetrics>
#include <QHash>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QStyle>

#include "itemmodel.h"

namespace {
// Инициалы для «аватара»: первые буквы двух первых слов имени.
QString initialsOf(const QString &name)
{
    const QStringList words = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QString result;
    for (int i = 0; i < words.size() && i < 2; ++i)
        result += words.at(i).at(0).toUpper();
    return result.isEmpty() ? QStringLiteral("?") : result;
}

} // namespace

ItemDelegate::ItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
    qDebug() << "[DELEGATE] создан ItemDelegate";
}

QWidget *ItemDelegate::createEditor(QWidget *parent,
                                    const QStyleOptionViewItem & /*option*/,
                                    const QModelIndex &index) const
{
    // Родитель редактора — viewport списка: view сам удалит редактор (ЛР2).
    auto *editor = new QLineEdit(parent);
    editor->setMaxLength(Validator::kMaxNameLength);
    editor->setPlaceholderText(QStringLiteral("Имя клиента"));
    qDebug() << "[EDITOR] createEditor: строка" << index.row();
    return editor;
}

void ItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    // Из МОДЕЛИ в редактор: иначе редактор откроется пустым.
    if (auto *le = qobject_cast<QLineEdit*>(editor)) {
        const QString value = index.data(Qt::EditRole).toString();
        le->setText(value);
        le->selectAll();
        qDebug() << "[EDITOR] setEditorData:" << value;
    }
}

void ItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                const QModelIndex &index) const
{
    // Из редактора в МОДЕЛЬ. Модель сама проверит имя и излучит dataChanged,
    // после чего QListView перерисует строку.
    if (auto *le = qobject_cast<QLineEdit*>(editor)) {
        const QString newValue = le->text().trimmed();
        if (model->setData(index, newValue, Qt::EditRole))
            qDebug() << "[EDITOR] setModelData: сохранено в модель:" << newValue;
        else
            qDebug() << "[EDITOR] setModelData: значение отклонено моделью:" << newValue;
    }
}

void ItemDelegate::updateEditorGeometry(QWidget *editor,
                                        const QStyleOptionViewItem &option,
                                        const QModelIndex & /*index*/) const
{
    // Редактор — поверх области имени (правее аватара), а не в углу окна.
    editor->setGeometry(option.rect.adjusted(kGutter, 6, -8, -6));
}

bool ItemDelegate::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Escape)
            qDebug() << "[EDITOR] Escape: редактирование отменено, модель не изменена";
    }
    return QStyledItemDelegate::eventFilter(object, event);
}

QSize ItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(qMax(size.height(), kRowHeight));
    return size;
}

void ItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    const QRect r = option.rect;
    const bool selected = option.state & QStyle::State_Selected;
    const QString name = index.data(Qt::DisplayRole).toString();

    // Контакт справа: телефон, а если его нет — email.
    QString contact = index.data(ItemModel::PhoneRole).toString();
    if (contact.isEmpty())
        contact = index.data(ItemModel::EmailRole).toString();

    QFont smallFont = option.font;
    if (smallFont.pointSizeF() > 1.0)
        smallFont.setPointSizeF(smallFont.pointSizeF() - 1.0);
    const int contactWidth = contact.isEmpty()
                                 ? 0
                                 : QFontMetrics(smallFont).horizontalAdvance(contact) + 16;

    // 1. Фон выделения на всю ширину строки (иначе слева останется щель).
    if (selected) {
        painter->save();
        painter->fillRect(r, option.palette.brush(QPalette::Highlight));
        painter->restore();
    }

    // 2. Базовая отрисовка имени в суженном прямоугольнике: слева место под
    //    аватар, справа — под контакт. Шрифт имени — полужирный.
    QStyleOptionViewItem opt = option;
    opt.rect.adjust(kGutter, 0, -contactWidth, 0);
    opt.font.setBold(true);
    QStyledItemDelegate::paint(painter, opt, index);

    // 3. Свой рисунок. ВСЕ изменения пера/кисти/шрифта — между save() и
    //    restore(): view рисует все строки одним QPainter, и без restore()
    //    наши настройки «утекли» бы в соседние ячейки.
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // 3a. Аватар: круг цвета, зависящего от имени, и инициалы.
    const QRect avatar(r.left() + (kGutter - kAvatar) / 2,
                       r.top() + (r.height() - kAvatar) / 2,
                       kAvatar, kAvatar);
    const int hue = static_cast<int>(qHash(name) % 360u);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor::fromHsv(hue, 140, 200));
    painter->drawEllipse(avatar);

    QFont avatarFont = option.font;
    avatarFont.setBold(true);
    painter->setFont(avatarFont);
    painter->setPen(Qt::white);
    painter->drawText(avatar, Qt::AlignCenter, initialsOf(name));

    // 3b. Контакт клиента серым мелким шрифтом справа.
    if (!contact.isEmpty()) {
        painter->setFont(smallFont);
        painter->setPen(selected ? option.palette.color(QPalette::HighlightedText)
                                 : option.palette.color(QPalette::Disabled, QPalette::Text));
        painter->drawText(QRect(r.right() - contactWidth, r.top(), contactWidth - 8, r.height()),
                          Qt::AlignRight | Qt::AlignVCenter, contact);
    }

    // 3c. Разделитель между строками.
    painter->setPen(QPen(option.palette.color(QPalette::Mid), 1));
    painter->drawLine(r.left() + kGutter, r.bottom(), r.right(), r.bottom());

    painter->restore();
}

#ifndef ITEMDELEGATE_H
#define ITEMDELEGATE_H

#include <QStyledItemDelegate>

// Делегат строки списка клиентов.
// * Редактирование имени по двойному клику: встроенный QLineEdit,
//   сохранение в модель через setModelData(), отмена — Escape.
// * Отрисовка: «аватар» с инициалами, имя и контакт клиента.
//   Все изменения QPainter закрыты в save()/restore().
// Q_OBJECT не нужен: делегат только переопределяет виртуальные методы
// и не объявляет собственных сигналов/слотов.
class ItemDelegate : public QStyledItemDelegate
{
public:
    explicit ItemDelegate(QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

protected:
    // Перехватываем клавиши редактора только для лога: саму отмену по Escape
    // выполняет базовый QStyledItemDelegate (закрывает редактор без setModelData).
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    static constexpr int kRowHeight = 44;   // высота строки
    static constexpr int kAvatar    = 30;   // диаметр «аватара»
    static constexpr int kGutter    = 46;   // ширина левой полосы под аватар
};

#endif // ITEMDELEGATE_H

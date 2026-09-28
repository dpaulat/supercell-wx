#include <scwx/qt/ui/checkable_combo_box.hpp>

#include <QAbstractItemView>
#include <QEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QStandardItemModel>

namespace scwx::qt::ui
{

CheckableComboBox::CheckableComboBox(QWidget* parent) : QComboBox(parent)
{
   setModel(new QStandardItemModel(this));

   // A normal (non-editable) QComboBox always displays whichever item is
   // currentIndex() -- there's no hook to show arbitrary summary text
   // instead. Making it editable with a read-only line edit is the usual
   // workaround: the visible text becomes whatever this class sets on
   // the line edit directly, independent of which row (if any) is
   // "current".
   setEditable(true);
   lineEdit()->setReadOnly(true);
   lineEdit()->setAlignment(Qt::AlignLeft);

   // QComboBox closes its popup on any item click by default -- fine for
   // single-select, wrong here, since checking one item shouldn't end
   // the selection. Intercepting the viewport's own mouse release (rather
   // than connecting to activated()/currentIndexChanged(), which fire
   // *after* the popup has already started closing) is what actually
   // stops that.
   view()->viewport()->installEventFilter(this);

   UpdateDisplayText();
}

CheckableComboBox::~CheckableComboBox() = default;

void CheckableComboBox::SetItems(const std::vector<std::string>& items)
{
   auto* standardModel = qobject_cast<QStandardItemModel*>(model());

   standardModel->clear();

   for (const auto& item : items)
   {
      auto* standardItem = new QStandardItem(QString::fromStdString(item));
      standardItem->setCheckable(true);
      standardItem->setCheckState(Qt::Unchecked);
      standardModel->appendRow(standardItem);
   }

   UpdateDisplayText();
}

void CheckableComboBox::SetChecked(const std::string& item, bool checked)
{
   auto* standardModel = qobject_cast<QStandardItemModel*>(model());

   for (int row = 0; row < standardModel->rowCount(); ++row)
   {
      auto* standardItem = standardModel->item(row);
      if (standardItem->text().toStdString() == item)
      {
         standardItem->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
         UpdateDisplayText();
         return;
      }
   }
}

bool CheckableComboBox::IsChecked(const std::string& item) const
{
   auto* standardModel = qobject_cast<QStandardItemModel*>(model());

   for (int row = 0; row < standardModel->rowCount(); ++row)
   {
      auto* standardItem = standardModel->item(row);
      if (standardItem->text().toStdString() == item)
      {
         return standardItem->checkState() == Qt::Checked;
      }
   }

   return false;
}

std::vector<std::string> CheckableComboBox::CheckedItems() const
{
   std::vector<std::string> checked;

   auto* standardModel = qobject_cast<QStandardItemModel*>(model());
   for (int row = 0; row < standardModel->rowCount(); ++row)
   {
      auto* standardItem = standardModel->item(row);
      if (standardItem->checkState() == Qt::Checked)
      {
         checked.push_back(standardItem->text().toStdString());
      }
   }

   return checked;
}

void CheckableComboBox::ToggleRow(int row)
{
   auto* standardModel = qobject_cast<QStandardItemModel*>(model());
   if (row < 0 || row >= standardModel->rowCount())
   {
      return;
   }

   auto* standardItem = standardModel->item(row);
   standardItem->setCheckState(
      standardItem->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);

   UpdateDisplayText();
   Q_EMIT CheckedItemsChanged();
}

bool CheckableComboBox::eventFilter(QObject* watched, QEvent* event)
{
   if (watched == view()->viewport() &&
       event->type() == QEvent::MouseButtonRelease)
   {
      auto*      mouseEvent = static_cast<QMouseEvent*>(event);
      const auto index      = view()->indexAt(mouseEvent->pos());

      if (index.isValid())
      {
         ToggleRow(index.row());
      }

      return true; // consumed -- keeps the popup open
   }

   return QComboBox::eventFilter(watched, event);
}

void CheckableComboBox::UpdateDisplayText()
{
   const std::vector<std::string> checked = CheckedItems();

   if (checked.empty())
   {
      lineEdit()->setText(tr("(none selected)"));
      return;
   }

   std::string text;
   for (std::size_t i = 0; i < checked.size(); ++i)
   {
      if (i > 0)
      {
         text += ", ";
      }
      text += checked[i];
   }

   lineEdit()->setText(QString::fromStdString(text));
}

} // namespace scwx::qt::ui

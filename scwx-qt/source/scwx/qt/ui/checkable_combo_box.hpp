#pragma once

#include <string>
#include <vector>

#include <QComboBox>

namespace scwx::qt::ui
{

// A QComboBox whose dropdown items are checkboxes rather than a single
// exclusive selection -- clicking an item toggles it without closing the
// popup, and the box's own display text summarizes what's checked (e.g.
// "2m Temperature, 2m Dewpoint" or "3 selected" once too many to fit).
// Standard QComboBox has no such mode; this is the usual Qt technique for
// it (a checkable QStandardItemModel + an event filter so the popup
// survives a click) rather than a bespoke one-off.
class CheckableComboBox : public QComboBox
{
   Q_OBJECT

public:
   explicit CheckableComboBox(QWidget* parent = nullptr);
   ~CheckableComboBox() override;

   // Replaces every item. Nothing is checked initially -- call
   // SetChecked() after, if anything should start checked.
   void SetItems(const std::vector<std::string>& items);

   void               SetChecked(const std::string& item, bool checked);
   [[nodiscard]] bool IsChecked(const std::string& item) const;
   [[nodiscard]] std::vector<std::string> CheckedItems() const;

signals:
   // Fires once per toggle, after both the model and the display text
   // have been updated.
   void CheckedItemsChanged();

protected:
   bool eventFilter(QObject* watched, QEvent* event) override;

private:
   void UpdateDisplayText();

   void ToggleRow(int row);
};

} // namespace scwx::qt::ui

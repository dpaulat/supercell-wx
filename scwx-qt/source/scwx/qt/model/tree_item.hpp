#pragma once

#include <memory>
#include <vector>

#include <QVariant>

namespace scwx
{
namespace qt
{
namespace model
{

class TreeItem
{
public:
   explicit TreeItem(const std::vector<QVariant>& data,
                     TreeItem*                    parent = nullptr);
   virtual ~TreeItem();

   TreeItem(const TreeItem&)            = delete;
   TreeItem& operator=(const TreeItem&) = delete;

   TreeItem(TreeItem&&) noexcept;
   TreeItem& operator=(TreeItem&&) noexcept;

   void AppendChild(TreeItem* child);

   const TreeItem* child(int row) const;
   int             child_count() const;
   int             column_count() const;
   QVariant        data(int column) const;
   int             row() const;
   const TreeItem* parent_item() const;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace model
} // namespace qt
} // namespace scwx

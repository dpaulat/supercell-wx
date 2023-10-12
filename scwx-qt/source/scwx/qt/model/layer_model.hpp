#pragma once

#include <scwx/qt/types/text_event_key.hpp>
#include <scwx/common/geographic.hpp>

#include <memory>

#include <QAbstractTableModel>

namespace scwx
{
namespace qt
{
namespace model
{

class LayerModelImpl;

class LayerModel : public QAbstractTableModel
{
public:
   enum class Column : int
   {
      Order       = 0,
      EnabledMap1 = 1,
      EnabledMap2 = 2,
      EnabledMap3 = 3,
      EnabledMap4 = 4,
      Type        = 5,
      Description = 6
   };

   enum class LayerType
   {
      Map,
      Radar,
      Alert,
      Placefile
   };

   explicit LayerModel(QObject* parent = nullptr);
   ~LayerModel();

   int rowCount(const QModelIndex& parent = QModelIndex()) const override;
   int columnCount(const QModelIndex& parent = QModelIndex()) const override;

   Qt::ItemFlags flags(const QModelIndex& index) const override;

   QVariant data(const QModelIndex& index,
                 int                role = Qt::DisplayRole) const override;
   QVariant headerData(int             section,
                       Qt::Orientation orientation,
                       int             role = Qt::DisplayRole) const override;

   bool setData(const QModelIndex& index,
                const QVariant&    value,
                int                role = Qt::EditRole) override;

public slots:
   void HandlePlacefileRemoved(const std::string& name);
   void HandlePlacefileRenamed(const std::string& oldName,
                               const std::string& newName);
   void HandlePlacefileUpdate(const std::string& name);

private:
   friend class LayerModelImpl;
   std::unique_ptr<LayerModelImpl> p;
};

} // namespace model
} // namespace qt
} // namespace scwx

#pragma once

#include <scwx/qt/model/tree_model.hpp>

namespace scwx
{
namespace qt
{
namespace model
{

class RadarProductModelImpl;

class RadarProductModel : public TreeModel
{
public:
   explicit RadarProductModel(QObject* parent = nullptr);
   ~RadarProductModel();

protected:
   const std::shared_ptr<TreeItem> root_item() const override;

private:
   std::unique_ptr<RadarProductModelImpl> p;
};

} // namespace model
} // namespace qt
} // namespace scwx

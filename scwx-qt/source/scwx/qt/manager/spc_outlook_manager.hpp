#pragma once

#include <scwx/spc/spc_types.hpp>

#include <memory>

#include <QObject>
#include <QTimer>

namespace scwx::qt::manager
{

class SpcOutlookManager : public QObject
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(SpcOutlookManager)

public:
   explicit SpcOutlookManager();
   ~SpcOutlookManager();

   void SelectDay(scwx::spc::OutlookDay day);
   void SelectProduct(scwx::spc::OutlookProduct product);
   void SetOpacity(int opacity);
   void SetAutoRefresh(bool enabled);
   void RefreshNow();

   scwx::spc::OutlookDay     GetSelectedDay() const;
   scwx::spc::OutlookProduct GetSelectedProduct() const;
   int                       GetOpacity() const;
   bool                      IsAutoRefreshEnabled() const;

   std::shared_ptr<scwx::spc::OutlookData> GetOutlookData() const;

   static SpcOutlookManager& Instance();

signals:
   void OutlookDataUpdated();
   void FetchError(const QString& message);

private slots:
   void OnRefreshTimer();

private:
   void FetchOutlookAsync(scwx::spc::OutlookDay     day,
                          scwx::spc::OutlookProduct product);

   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::manager

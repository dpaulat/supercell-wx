#pragma once

#include <scwx/spc/spc_md_types.hpp>

#include <memory>

#include <QObject>
#include <QTimer>

namespace scwx::qt::manager
{

class SpcMdManager : public QObject
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(SpcMdManager)

public:
   explicit SpcMdManager();
   ~SpcMdManager();

   void SetAutoRefresh(bool enabled);
   void RefreshNow();

   std::shared_ptr<scwx::spc::MdData> GetMdData() const;

   static SpcMdManager& Instance();

signals:
   void MdDataUpdated();
   void FetchError(const QString& message);

private slots:
   void OnRefreshTimer();

private:
   void FetchMdAsync();

   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::manager

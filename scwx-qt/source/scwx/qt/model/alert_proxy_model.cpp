#include <scwx/qt/model/alert_proxy_model.hpp>
#include <scwx/qt/model/alert_model.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/threads.hpp>
#include <scwx/util/time.hpp>

#include <chrono>
#include <mutex>

#include <boost/asio/steady_timer.hpp>

namespace scwx::qt::model
{

static const std::string logPrefix_ = "scwx::qt::model::alert_proxy_model";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class AlertProxyModel::Impl
{
public:
   explicit Impl(AlertProxyModel* self);
   ~Impl();

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   void UpdateAlerts();

   AlertProxyModel* self_;

   bool alertActiveFilterEnabled_;

   boost::asio::steady_timer alertUpdateTimer_;
   std::mutex                alertMutex_ {};
};

AlertProxyModel::AlertProxyModel(QObject* parent) :
    QSortFilterProxyModel(parent), p(std::make_unique<Impl>(this))
{
}
AlertProxyModel::~AlertProxyModel() = default;

void AlertProxyModel::SetAlertActiveFilter(bool enabled)
{
   beginFilterChange();
   p->alertActiveFilterEnabled_ = enabled;
   endFilterChange(Direction::Rows);
}

bool AlertProxyModel::filterAcceptsRow(int                sourceRow,
                                       const QModelIndex& sourceParent) const
{
   bool acceptAlertActiveFilter = true;

   if (p->alertActiveFilterEnabled_)
   {
      // Get source model index
      QModelIndex endTimeIndex =
         sourceModel()->index(sourceRow,
                              static_cast<int>(AlertModel::Column::EndTime),
                              sourceParent);

      // Get source end time
      auto endTime = sourceModel()
                        ->data(endTimeIndex, types::TimePointRole)
                        .value<std::chrono::system_clock::time_point>();

      // Compare end time to current
      if (endTime < scwx::util::time::now())
      {
         acceptAlertActiveFilter = false;
      }
   }

   return acceptAlertActiveFilter &&
          QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}

AlertProxyModel::Impl::Impl(AlertProxyModel* self) :
    self_ {self},
    alertActiveFilterEnabled_ {false},
    alertUpdateTimer_ {scwx::util::io_context()}
{
   // Schedule alert update
   UpdateAlerts();
}

AlertProxyModel::Impl::~Impl()
{
   try
   {
      const std::unique_lock lock(alertMutex_);
      alertUpdateTimer_.cancel();
   }
   catch (const std::exception& ex)
   {
      logger_->error(ex.what());
   }
}

void AlertProxyModel::Impl::UpdateAlerts()
{
   logger_->trace("UpdateAlerts");

   // Take a unique lock before modifying feature lists
   const std::unique_lock lock(alertMutex_);

   // Re-evaluate for expired alerts
   if (alertActiveFilterEnabled_)
   {
      QMetaObject::invokeMethod(self_,
                                static_cast<void (QSortFilterProxyModel::*)()>(
                                   &QSortFilterProxyModel::invalidate));
   }

   using namespace std::chrono;

   // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers): Readability
   alertUpdateTimer_.expires_after(15s);
   alertUpdateTimer_.async_wait(
      [this](const boost::system::error_code& e)
      {
         if (e == boost::asio::error::operation_aborted)
         {
            logger_->debug("Alert update timer cancelled");
         }
         else if (e != boost::system::errc::success)
         {
            logger_->warn("Alert update timer error: {}", e.message());
         }
         else
         {
            try
            {
               UpdateAlerts();
            }
            catch (const std::exception& ex)
            {
               logger_->error(ex.what());
            }
         }
      });
}

} // namespace scwx::qt::model

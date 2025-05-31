#include <scwx/deriver/base_deriver.hpp>
#include <scwx/util/queue_counter.hpp>

#include <memory>
#include <unordered_map>
#include <utility>
#include <shared_mutex>

// Avoid circular refrence errors in boost
// NOLINTBEGIN(misc-header-include-cycle)
#if defined(_MSC_VER)
#   pragma warning(push, 0)
#endif

#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

namespace scwx::deriver
{

class BaseDeriver::Impl
{
public:
   explicit Impl(BaseDeriver* self) : self_ {self} {};

   BaseDeriver* const self_;

   std::shared_ptr<wsr88d::Ar2vFile> level2File_ = nullptr;
   std::shared_mutex                 level2FileMutex_ {};
   std::unordered_map<std::string, std::shared_ptr<wsr88d::Level3File>>
                     level3Files_ {};
   std::shared_mutex level3FilesMutex_ {};

   // If 2 are queued/processing, that means that once the second one goes, it
   // will pull the most recent data, so another run is not useful.
   util::QueueCounter       calculateQueueCounter_ {2};
   boost::asio::thread_pool calculateThreadPool_ {1};

   std::shared_ptr<data::DerivedData> output_ = nullptr;
   std::shared_mutex                  outputMutex_ {};

   boost::signals2::signal<void()> updateSignal_ {};

   void UpdateOutput();
};

BaseDeriver::BaseDeriver() : p {std::make_unique<Impl>(this)} {}
BaseDeriver::~BaseDeriver() = default;

boost::signals2::signal<void()>& BaseDeriver::update_signal() const
{
   return p->updateSignal_;
}

void BaseDeriver::SetLevel2InputFile(std::shared_ptr<wsr88d::Ar2vFile> file)
{
   if (NeedsLevel2Input())
   {
      {
         std::unique_lock lock {p->level2FileMutex_};
         p->level2File_ = std::move(file);
      }
      p->UpdateOutput();
   }
}

void BaseDeriver::SetLevel3InputFile(const std::string& product,
                                     std::shared_ptr<wsr88d::Level3File> file)
{
   if (GetLevel3InputProducts().contains(product))
   {
      {
         std::unique_lock lock {p->level3FilesMutex_};
         p->level3Files_.insert_or_assign(product, file);
      }
      p->UpdateOutput();
   }
}

std::shared_ptr<data::DerivedData> BaseDeriver::GetOutput()
{
   std::shared_lock lock {p->outputMutex_};
   return p->output_;
}

std::shared_ptr<wsr88d::Ar2vFile> BaseDeriver::GetLevel2File()
{
   std::shared_lock lock {p->level2FileMutex_};
   return p->level2File_;
}

std::shared_ptr<wsr88d::Level3File>
BaseDeriver::GetLevel3File(const std::string& product)
{
   std::shared_lock lock {p->level3FilesMutex_};
   auto             file = p->level3Files_.find(product);
   if (file == p->level3Files_.end())
   {
      return nullptr;
   }
   return file->second;
}

void BaseDeriver::Impl::UpdateOutput()
{
   // If 2 are queued/processing, that means that once the second one goes, it
   // will pull the most recent data, so another run is not useful.
   if (!calculateQueueCounter_.add())
   {
      return;
   }

   boost::asio::post(
      [this]()
      {
         std::shared_ptr<data::DerivedData> output = self_->CalculateData();

         {
            std::unique_lock lock {outputMutex_};
            output_ = output;
         }
         updateSignal_();

         // This should be called last to minimize extra calculations being
         // done.
         calculateQueueCounter_.remove();
      });
}

} // namespace scwx::deriver

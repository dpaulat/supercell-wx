#include <scwx/qt/map/mesoscale_discussion_layer.hpp>
#include <scwx/qt/gl/draw/geo_lines.hpp>
#include <scwx/qt/manager/spc_md_manager.hpp>
#include <scwx/qt/util/tooltip.hpp>
#include <scwx/util/logger.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include <boost/gil.hpp>
#include <QEvent>

namespace scwx::qt::map
{

static const std::string logPrefix_ =
   "scwx::qt::map::mesoscale_discussion_layer";
static const auto logger_ = scwx::util::Logger::Create(logPrefix_);

static const boost::gil::rgba32f_pixel_t kFillColor_ {
   0.0f, 0.0f, 139.0f / 255.0f, 0.15f};
static const boost::gil::rgba32f_pixel_t kLineColor_ {
   0.0f, 0.0f, 139.0f / 255.0f, 1.0f};

class MesoscaleDiscussionLayer::Impl
{
public:
   explicit Impl(MesoscaleDiscussionLayer*             self,
                 const std::shared_ptr<gl::GlContext>& glContext) :
       self_ {self}, geoLines_ {std::make_shared<gl::draw::GeoLines>(glContext)}
   {
   }
   ~Impl() { std::unique_lock lock(linesMutex_); }

   Impl(const Impl&)            = delete;
   Impl& operator=(const Impl&) = delete;

   void ConnectSignals();
   void BuildGeometry();

   void HandleGeoLinesEvent(std::weak_ptr<gl::draw::GeoLineDrawItem>& diWeak,
                            QEvent*                                   ev);
   void
   HandleGeoLinesHover(const std::shared_ptr<gl::draw::GeoLineDrawItem>& di,
                       const QPointF& mouseGlobalPos);

   MesoscaleDiscussionLayer* self_;

   std::shared_ptr<gl::draw::GeoLines> geoLines_;

   std::mutex linesMutex_;

   std::vector<scwx::spc::MesoscaleDiscussion> discussions_;

   // Mapping from GeoLineDrawItem to mdNumber for pickable lines
   std::unordered_map<std::shared_ptr<gl::draw::GeoLineDrawItem>, int>
      lineToMd_;

   std::shared_ptr<const gl::draw::GeoLineDrawItem> lastHoverDi_ {nullptr};
   std::string                                      tooltip_ {};

   bool dataLoaded_ {false};
};

MesoscaleDiscussionLayer::MesoscaleDiscussionLayer(
   std::shared_ptr<gl::GlContext> glContext) :
    DrawLayer(glContext, "MesoscaleDiscussionLayer"),
    p(std::make_unique<Impl>(this, glContext))
{
   AddDrawItem(p->geoLines_);
}

MesoscaleDiscussionLayer::~MesoscaleDiscussionLayer() = default;

void MesoscaleDiscussionLayer::Initialize(
   const std::shared_ptr<MapContext>& mapContext)
{
   DrawLayer::Initialize(mapContext);

   p->ConnectSignals();

   // Load any existing data that may have been fetched before initialization
   auto data = manager::SpcMdManager::Instance().GetMdData();
   if (data != nullptr && !data->discussions_.empty())
   {
      const std::lock_guard<std::mutex> lock(p->linesMutex_);
      p->discussions_ = data->discussions_;
      p->dataLoaded_  = true;
   }
}

void MesoscaleDiscussionLayer::Render(
   const std::shared_ptr<MapContext>&            mapContext,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   // Rebuild geometry if data has changed
   p->BuildGeometry();

   DrawLayer::Render(mapContext, params);

   SCWX_GL_CHECK_ERROR();
}

void MesoscaleDiscussionLayer::Deinitialize()
{
   DrawLayer::Deinitialize();
}

void MesoscaleDiscussionLayer::Impl::ConnectSignals()
{
   QObject::connect(
      &manager::SpcMdManager::Instance(),
      &manager::SpcMdManager::MdDataUpdated,
      self_,
      [this]()
      {
         auto data = manager::SpcMdManager::Instance().GetMdData();
         if (data != nullptr)
         {
            {
               const std::lock_guard<std::mutex> lock(linesMutex_);
               discussions_ = data->discussions_;
               dataLoaded_  = true;
            }
            Q_EMIT self_->NeedsRendering();
         }
      });
}

void MesoscaleDiscussionLayer::Impl::BuildGeometry()
{
   if (!dataLoaded_)
   {
      return;
   }

   const std::lock_guard<std::mutex> lock(linesMutex_);
   dataLoaded_ = false;

   // Clear existing geometry
   geoLines_->StartLines();

   lineToMd_.clear();

   for (const auto& md : discussions_)
   {
      // For each ring in the discussion
      for (const auto& ring : md.rings_)
      {
         if (ring.size() < 2)
         {
            continue;
         }

         // Build line segments from ring points
         for (std::size_t i = 0, j = 1; i < ring.size(); ++i, ++j)
         {
            if (j >= ring.size())
            {
               j = 0;

               // Skip if closing point duplicates start
               if (ring[i].first == ring[j].first &&
                   ring[i].second == ring[j].second)
               {
                  break;
               }
            }

            // Fill segment (pickable — wide hit zone)
            auto fillDi = geoLines_->AddLine();
            geoLines_->SetLineLocation(fillDi,
                                       static_cast<float>(ring[i].first),
                                       static_cast<float>(ring[i].second),
                                       static_cast<float>(ring[j].first),
                                       static_cast<float>(ring[j].second));
            geoLines_->SetLineModulate(fillDi, kFillColor_);
            geoLines_->SetLineWidth(fillDi, 10.0f);

            // Register hover/click on fill lines (wide area)
            geoLines_->SetLineHoverCallback(
               fillDi,
               std::bind(&MesoscaleDiscussionLayer::Impl::HandleGeoLinesHover,
                         this,
                         std::placeholders::_1,
                         std::placeholders::_2));

            const std::weak_ptr<gl::draw::GeoLineDrawItem> fillDiWeak = fillDi;
            gl::draw::GeoLines::RegisterEventHandler(
               fillDi,
               std::bind(&MesoscaleDiscussionLayer::Impl::HandleGeoLinesEvent,
                         this,
                         fillDiWeak,
                         std::placeholders::_1));

            lineToMd_[fillDi] = md.mdNumber_;

            // Border segment (purely visual — no callbacks)
            auto lineDi = geoLines_->AddLine();
            geoLines_->SetLineLocation(lineDi,
                                       static_cast<float>(ring[i].first),
                                       static_cast<float>(ring[i].second),
                                       static_cast<float>(ring[j].first),
                                       static_cast<float>(ring[j].second));
            geoLines_->SetLineModulate(lineDi, kLineColor_);
            geoLines_->SetLineWidth(lineDi, 2.0f);
         }
      }
   }

   geoLines_->FinishLines();
}

void MesoscaleDiscussionLayer::Impl::HandleGeoLinesEvent(
   std::weak_ptr<gl::draw::GeoLineDrawItem>& diWeak, QEvent* ev)
{
   const std::shared_ptr<gl::draw::GeoLineDrawItem> di = diWeak.lock();
   if (di == nullptr)
   {
      return;
   }

   switch (ev->type())
   {
   case QEvent::Type::MouseButtonRelease:
   {
      auto it = lineToMd_.find(di);
      if (it != lineToMd_.cend())
      {
         Q_EMIT self_->MdSelected(it->second);
      }
      break;
   }

   default:
      break;
   }
}

void MesoscaleDiscussionLayer::Impl::HandleGeoLinesHover(
   const std::shared_ptr<gl::draw::GeoLineDrawItem>& di,
   const QPointF&                                    mouseGlobalPos)
{
   if (di != lastHoverDi_)
   {
      auto it = lineToMd_.find(di);
      if (it != lineToMd_.cend())
      {
         tooltip_ = "Mesoscale Discussion #" + std::to_string(it->second);
      }
      else
      {
         tooltip_.clear();
      }

      lastHoverDi_ = di;
   }

   if (!tooltip_.empty())
   {
      util::tooltip::Show(tooltip_, mouseGlobalPos);
   }
}

bool MesoscaleDiscussionLayer::RunMousePicking(
   const std::shared_ptr<MapContext>& /* mapContext */,
   const QMapLibre::CustomLayerRenderParameters& params,
   const QPointF&                                mouseLocalPos,
   const QPointF&                                mouseGlobalPos,
   const glm::vec2&                              mouseCoords,
   const common::Coordinate&                     mouseGeoCoords,
   std::shared_ptr<types::EventHandler>&         eventHandler)
{
   return p->geoLines_->RunMousePicking(params,
                                        mouseLocalPos,
                                        mouseGlobalPos,
                                        mouseCoords,
                                        mouseGeoCoords,
                                        eventHandler);
}

} // namespace scwx::qt::map

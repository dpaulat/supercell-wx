#include <scwx/qt/map/placefile_layer.hpp>
#include <scwx/qt/gl/draw/placefile_icons.hpp>
#include <scwx/qt/gl/draw/placefile_images.hpp>
#include <scwx/qt/gl/draw/placefile_lines.hpp>
#include <scwx/qt/gl/draw/placefile_polygons.hpp>
#include <scwx/qt/gl/draw/placefile_triangles.hpp>
#include <scwx/qt/gl/draw/placefile_text.hpp>
#include <scwx/qt/manager/placefile_manager.hpp>
#include <scwx/qt/manager/timeline_manager.hpp>
#include <scwx/util/logger.hpp>

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

static const std::string logPrefix_ = "scwx::qt::map::placefile_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class PlacefileLayer::Impl
{
public:
   explicit Impl(PlacefileLayer*                    self,
                 const std::shared_ptr<MapContext>& context,
                 const std::string&                 placefileName) :
       self_ {self},
       placefileName_ {placefileName},
       placefileIcons_ {std::make_shared<gl::draw::PlacefileIcons>(context)},
       placefileImages_ {std::make_shared<gl::draw::PlacefileImages>(context)},
       placefileLines_ {std::make_shared<gl::draw::PlacefileLines>(context)},
       placefilePolygons_ {
          std::make_shared<gl::draw::PlacefilePolygons>(context)},
       placefileTriangles_ {
          std::make_shared<gl::draw::PlacefileTriangles>(context)},
       placefileText_ {
          std::make_shared<gl::draw::PlacefileText>(context, placefileName)}
   {
      ConnectSignals();
   }
   ~Impl() { threadPool_.join(); }

   void ConnectSignals();
   void ReloadDataSync();

   boost::asio::thread_pool threadPool_ {1};

   PlacefileLayer* self_;

   std::string placefileName_;
   std::mutex  dataMutex_ {};

   std::shared_ptr<gl::draw::PlacefileIcons>     placefileIcons_;
   std::shared_ptr<gl::draw::PlacefileImages>    placefileImages_;
   std::shared_ptr<gl::draw::PlacefileLines>     placefileLines_;
   std::shared_ptr<gl::draw::PlacefilePolygons>  placefilePolygons_;
   std::shared_ptr<gl::draw::PlacefileTriangles> placefileTriangles_;
   std::shared_ptr<gl::draw::PlacefileText>      placefileText_;

   std::chrono::system_clock::time_point selectedTime_ {};
};

PlacefileLayer::PlacefileLayer(const std::shared_ptr<MapContext>& context,
                               const std::string& placefileName) :
    DrawLayer(context),
    p(std::make_unique<PlacefileLayer::Impl>(this, context, placefileName))
{
   AddDrawItem(p->placefileImages_);
   AddDrawItem(p->placefilePolygons_);
   AddDrawItem(p->placefileTriangles_);
   AddDrawItem(p->placefileLines_);
   AddDrawItem(p->placefileIcons_);
   AddDrawItem(p->placefileText_);

   ReloadData();
}

PlacefileLayer::~PlacefileLayer() = default;

void PlacefileLayer::Impl::ConnectSignals()
{
   auto placefileManager = manager::PlacefileManager::Instance();
   auto timelineManager  = manager::TimelineManager::Instance();

   QObject::connect(placefileManager.get(),
                    &manager::PlacefileManager::PlacefileUpdated,
                    self_,
                    [this](const std::string& name)
                    {
                       if (name == placefileName_)
                       {
                          self_->ReloadData();
                       }
                    });

   QObject::connect(timelineManager.get(),
                    &manager::TimelineManager::SelectedTimeUpdated,
                    self_,
                    [this](std::chrono::system_clock::time_point dateTime)
                    { selectedTime_ = dateTime; });
}

std::string PlacefileLayer::placefile_name() const
{
   return p->placefileName_;
}

void PlacefileLayer::set_placefile_name(const std::string& placefileName)
{
   p->placefileName_ = placefileName;
   p->placefileText_->set_placefile_name(placefileName);

   ReloadData();
}

void PlacefileLayer::Initialize()
{
   logger_->debug("Initialize()");

   DrawLayer::Initialize();
}

void PlacefileLayer::Render(
   const QMapLibre::CustomLayerRenderParameters& params)
{
   gl::OpenGLFunctions& gl = context()->gl();

   // Set OpenGL blend mode for transparency
   gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   std::shared_ptr<manager::PlacefileManager> placefileManager =
      manager::PlacefileManager::Instance();

   auto placefile = placefileManager->placefile(p->placefileName_);

   // Render placefile
   if (placefile != nullptr)
   {
      bool thresholded =
         placefileManager->placefile_thresholded(placefile->name());
      p->placefileIcons_->set_thresholded(thresholded);
      p->placefileImages_->set_thresholded(thresholded);
      p->placefileLines_->set_thresholded(thresholded);
      p->placefilePolygons_->set_thresholded(thresholded);
      p->placefileTriangles_->set_thresholded(thresholded);
      p->placefileText_->set_thresholded(thresholded);

      p->placefileIcons_->set_selected_time(p->selectedTime_);
      p->placefileImages_->set_selected_time(p->selectedTime_);
      p->placefileLines_->set_selected_time(p->selectedTime_);
      p->placefilePolygons_->set_selected_time(p->selectedTime_);
      p->placefileTriangles_->set_selected_time(p->selectedTime_);
      p->placefileText_->set_selected_time(p->selectedTime_);
   }

   DrawLayer::Render(params);

   SCWX_GL_CHECK_ERROR();
}

void PlacefileLayer::Deinitialize()
{
   logger_->debug("Deinitialize()");

   DrawLayer::Deinitialize();
}

void PlacefileLayer::ReloadData()
{
   boost::asio::post(p->threadPool_,
                     [this]()
                     {
                        try
                        {
                           p->ReloadDataSync();
                        }
                        catch (const std::exception& ex)
                        {
                           logger_->error(ex.what());
                        }
                     });
}

void PlacefileLayer::Impl::ReloadDataSync()
{
   logger_->debug("ReloadData: {}", placefileName_);

   std::unique_lock lock {dataMutex_};

   std::shared_ptr<manager::PlacefileManager> placefileManager =
      manager::PlacefileManager::Instance();

   auto placefile = placefileManager->placefile(placefileName_);
   if (placefile == nullptr)
   {
      return;
   }

   // Start draw items
   placefileIcons_->StartIcons();
   placefileImages_->StartImages(placefile->name());
   placefileLines_->StartLines();
   placefilePolygons_->StartPolygons();
   placefileTriangles_->StartTriangles();
   placefileText_->StartText();

   placefileIcons_->SetIconFiles(placefile->icon_files(), placefile->name());
   placefileText_->SetFonts(placefileManager->placefile_fonts(placefileName_));

   for (auto& drawItem : placefile->GetDrawItems())
   {
      switch (drawItem->itemType_)
      {
      case gr::Placefile::ItemType::Text:
         placefileText_->AddText(
            std::static_pointer_cast<gr::Placefile::TextDrawItem>(drawItem));
         break;

      case gr::Placefile::ItemType::Icon:
         placefileIcons_->AddIcon(
            std::static_pointer_cast<gr::Placefile::IconDrawItem>(drawItem));
         break;

      case gr::Placefile::ItemType::Line:
         placefileLines_->AddLine(
            std::static_pointer_cast<gr::Placefile::LineDrawItem>(drawItem));
         break;

      case gr::Placefile::ItemType::Polygon:
         placefilePolygons_->AddPolygon(
            std::static_pointer_cast<gr::Placefile::PolygonDrawItem>(drawItem));
         break;

      case gr::Placefile::ItemType::Image:
         placefileImages_->AddImage(
            std::static_pointer_cast<gr::Placefile::ImageDrawItem>(drawItem));
         break;

      case gr::Placefile::ItemType::Triangles:
         placefileTriangles_->AddTriangles(
            std::static_pointer_cast<gr::Placefile::TrianglesDrawItem>(
               drawItem));
         break;

      default:
         break;
      }
   }

   // Finish draw items
   placefileIcons_->FinishIcons();
   placefileImages_->FinishImages();
   placefileLines_->FinishLines();
   placefilePolygons_->FinishPolygons();
   placefileTriangles_->FinishTriangles();
   placefileText_->FinishText();

   Q_EMIT self_->DataReloaded();
}

} // namespace map
} // namespace qt
} // namespace scwx

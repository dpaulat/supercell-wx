#pragma once

#include <scwx/sounding/sounding_data.hpp>

#include <memory>

#include <QOpenGLWidget>

namespace scwx::qt::view
{

class SkewtWidget : public QOpenGLWidget
{
   Q_OBJECT

public:
   explicit SkewtWidget(QWidget* parent = nullptr);
   ~SkewtWidget();

   SkewtWidget(const SkewtWidget&)            = delete;
   SkewtWidget& operator=(const SkewtWidget&) = delete;

   void SetSounding(std::shared_ptr<sounding::SoundingData> sounding);

protected:
   void initializeGL() override;
   void resizeGL(int w, int h) override;
   void paintGL() override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::view

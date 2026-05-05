#pragma once

#include <memory>
#include <string>

#include <QMapLibre/Map>
#include <QObject>

namespace scwx::qt::map
{

class ConvectiveOutlookLayer : public QObject
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(ConvectiveOutlookLayer)

public:
   explicit ConvectiveOutlookLayer();
   ~ConvectiveOutlookLayer();

   void Add(std::shared_ptr<QMapLibre::Map> map, const std::string& before);
   void Remove(std::shared_ptr<QMapLibre::Map> map);
   void Update(std::shared_ptr<QMapLibre::Map> map);

   static const std::string& sourceId();
   static const std::string& fillLayerId();
   static const std::string& cigFillLayerId();
   static const std::string& lineLayerId();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::map

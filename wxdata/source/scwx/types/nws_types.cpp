#include <scwx/types/nws_types.hpp>
#include <scwx/util/logger.hpp>

#include <boost/json/value_to.hpp>

namespace scwx::types::nws
{

static const std::string logPrefix_ = "scwx::types::nws_types";
static const auto        logger_    = util::Logger::Create(logPrefix_);

static double GetDouble(const boost::json::value& v)
{
   return v.is_double() ? v.as_double() : static_cast<double>(v.as_int64());
}

QuantitativeValue tag_invoke(boost::json::value_to_tag<QuantitativeValue>,
                             const boost::json::value& jv)
{
   auto& jo = jv.as_object();

   QuantitativeValue qv {};

   // Optional parameters
   if (jo.contains("value") && !jo.at("value").is_null())
   {
      qv.value_ = GetDouble(jo.at("value"));
   }
   if (jo.contains("maxValue"))
   {
      qv.maxValue_ = GetDouble(jo.at("maxValue"));
   }
   if (jo.contains("minValue"))
   {
      qv.minValue_ = GetDouble(jo.at("minValue"));
   }
   if (jo.contains("unitCode"))
   {
      qv.unitCode_ = jo.at("unitCode").as_string();
   }
   if (jo.contains("qualityControl"))
   {
      qv.qualityControl_ = jo.at("qualityControl").as_string();
   }

   return qv;
}

Latency tag_invoke(boost::json::value_to_tag<Latency>,
                   const boost::json::value& jv)
{
   auto& jo = jv.as_object();

   Latency latency {};

   // Optional parameters
   if (jo.contains("current") && !jo.at("current").is_null())
   {
      latency.current_ =
         boost::json::value_to<QuantitativeValue>(jo.at("current"));
   }
   if (jo.contains("average") && !jo.at("average").is_null())
   {
      latency.average_ =
         boost::json::value_to<QuantitativeValue>(jo.at("average"));
   }
   if (jo.contains("max") && !jo.at("max").is_null())
   {
      latency.max_ = boost::json::value_to<QuantitativeValue>(jo.at("max"));
   }
   if (jo.contains("levelTwoLastReceivedTime") &&
       !jo.at("levelTwoLastReceivedTime").is_null())
   {
      latency.levelTwoLastReceivedTime_ =
         jo.at("levelTwoLastReceivedTime").as_string();
   }
   if (jo.contains("maxLatencyTime") && !jo.at("maxLatencyTime").is_null())
   {
      latency.maxLatencyTime_ = jo.at("maxLatencyTime").as_string();
   }
   if (jo.contains("reportingHost") && !jo.at("reportingHost").is_null())
   {
      latency.reportingHost_ = jo.at("reportingHost").as_string();
   }
   if (jo.contains("host") && !jo.at("host").is_null())
   {
      latency.host_ = jo.at("host").as_string();
   }

   return latency;
}

ObservationStation tag_invoke(boost::json::value_to_tag<ObservationStation>,
                              const boost::json::value& jv)
{
   auto& jo = jv.as_object();

   ObservationStation os {};

   // Required parameters
   os._id_   = jo.at("@id").as_string();
   os._type_ = jo.at("@type").as_string();

   os.name_     = jo.at("name").as_string();
   os.geometry_ = jo.at("geometry").as_string();
   if (jo.contains("timeZone"))
   {
      const auto& timeZone = jo.at("timeZone");
      if (timeZone.is_string())
      {
         os.timeZone_ = timeZone.as_string();
      }
   }

   // Optional parameters
   if (jo.contains("id"))
   {
      os.id_ = jo.at("id").as_string();
   }
   if (jo.contains("stationType"))
   {
      os.stationType_ = jo.at("stationType").as_string();
   }
   if (jo.contains("elevation"))
   {
      os.elevation_ =
         boost::json::value_to<QuantitativeValue>(jo.at("elevation"));
   }
   if (jo.contains("latency"))
   {
      os.latency_ = boost::json::value_to<Latency>(jo.at("latency"));
   }

   return os;
}

ObservationStationCollection
tag_invoke(boost::json::value_to_tag<ObservationStationCollection>,
           const boost::json::value& jv)
{
   auto& jo = jv.as_object();

   ObservationStationCollection collection {};

   // Required parameters
   collection.graph_ =
      boost::json::value_to<std::vector<ObservationStation>>(jo.at("@graph"));

   return collection;
}

ParameterError tag_invoke(boost::json::value_to_tag<ParameterError>,
                          const boost::json::value& jv)
{
   auto& jo = jv.as_object();

   ParameterError parameterError {};

   // Required parameters
   parameterError.parameter_ = jo.at("parameter").as_string();
   parameterError.message_   = jo.at("message").as_string();

   return parameterError;
}

ErrorResponse tag_invoke(boost::json::value_to_tag<ErrorResponse>,
                         const boost::json::value& jv)
{
   auto& jo = jv.as_object();

   ErrorResponse errorResponse {};

   // Required parameters
   errorResponse.type_          = jo.at("type").as_string();
   errorResponse.title_         = jo.at("title").as_string();
   errorResponse.status_        = jo.at("status").as_int64();
   errorResponse.detail_        = jo.at("detail").as_string();
   errorResponse.instance_      = jo.at("instance").as_string();
   errorResponse.correlationId_ = jo.at("correlationId").as_string();

   if (jo.contains("parameterErrors"))
   {
      errorResponse.parameterErrors_ =
         boost::json::value_to<std::vector<ParameterError>>(
            jo.at("parameterErrors"));
   }

   return errorResponse;
}

std::chrono::milliseconds
GetQuantitativeTime(const scwx::types::nws::QuantitativeValue& value)
{
   const double rawValue = value.value_.value_or(0);
   auto&        unitCode = value.unitCode_;

   // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)

   if (unitCode.ends_with(":min"))
   {
      // Convert minutes to milliseconds
      return std::chrono::milliseconds(
         static_cast<long long>(rawValue * 60 * 1000));
   }
   else if (unitCode.ends_with(":h"))
   {
      // Convert hours to milliseconds
      return std::chrono::milliseconds(
         static_cast<long long>(rawValue * 60 * 60 * 1000));
   }
   else if (unitCode.ends_with(":d"))
   {
      // Convert days to milliseconds
      return std::chrono::milliseconds(
         static_cast<long long>(rawValue * 24 * 60 * 60 * 1000));
   }
   else
   {
      if (!unitCode.empty() && !unitCode.ends_with(":s"))
      {
         logger_->warn("Unknown unit code, assuming seconds: {}", unitCode);
      }

      // Convert seconds to milliseconds
      return std::chrono::milliseconds(static_cast<long long>(rawValue * 1000));
   }

   // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
}

} // namespace scwx::types::nws

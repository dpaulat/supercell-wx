#pragma once

#include <chrono>
#include <optional>
#include <string>

#include <boost/json/value.hpp>

/**
@brief Types for the NWS API
*
* <https://www.weather.gov/documentation/services-web-api>
 */
namespace scwx::types::nws
{

struct QuantitativeValue
{
   std::optional<double> value_ {};
   std::optional<double> maxValue_ {};
   std::optional<double> minValue_ {};
   std::string           unitCode_ {};
   std::string           qualityControl_ {};
};

struct Latency
{
   std::optional<QuantitativeValue> current_ {};
   std::optional<QuantitativeValue> average_ {};
   std::optional<QuantitativeValue> max_ {};
   std::string                      levelTwoLastReceivedTime_ {};
   std::string                      maxLatencyTime_ {};
   std::string                      reportingHost_ {};
   std::string                      host_ {};
};

struct ObservationStation
{
   std::string                      _id_ {};
   std::string                      _type_ {};
   std::string                      id_ {};
   std::string                      name_ {};
   std::string                      stationType_ {};
   std::string                      geometry_ {};
   std::optional<QuantitativeValue> elevation_ {};
   std::string                      timeZone_ {};
   std::optional<Latency>           latency_ {};
};

struct ObservationStationCollection
{
   std::vector<ObservationStation> graph_ {};
};

struct ParameterError
{
   std::string parameter_ {};
   std::string message_ {};
};

struct ErrorResponse
{
   std::string                 type_ {};
   std::string                 title_ {};
   std::int64_t                status_ {};
   std::string                 detail_ {};
   std::string                 instance_ {};
   std::string                 correlationId_ {};
   std::vector<ParameterError> parameterErrors_ {};
};

QuantitativeValue  tag_invoke(boost::json::value_to_tag<QuantitativeValue>,
                              const boost::json::value& jv);
Latency            tag_invoke(boost::json::value_to_tag<Latency>,
                              const boost::json::value& jv);
ObservationStation tag_invoke(boost::json::value_to_tag<ObservationStation>,
                              const boost::json::value& jv);
ObservationStationCollection
tag_invoke(boost::json::value_to_tag<ObservationStationCollection>,
           const boost::json::value& jv);
ParameterError tag_invoke(boost::json::value_to_tag<ParameterError>,
                          const boost::json::value& jv);
ErrorResponse  tag_invoke(boost::json::value_to_tag<ErrorResponse>,
                          const boost::json::value& jv);

std::chrono::milliseconds GetQuantitativeTime(const QuantitativeValue& value);

} // namespace scwx::types::nws

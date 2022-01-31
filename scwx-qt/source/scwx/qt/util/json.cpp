#include <scwx/qt/util/json.hpp>

#include <fstream>

#include <boost/log/trivial.hpp>
#include <QFile>
#include <QTextStream>

namespace scwx
{
namespace qt
{
namespace util
{
namespace json
{

static const std::string logPrefix_ = "[scwx::qt::util::json] ";

/* Adapted from:
 * https://www.boost.org/doc/libs/1_77_0/libs/json/doc/html/json/examples.html#json.examples.pretty
 *
 * Copyright (c) 2019, 2020 Vinnie Falco
 * Copyright (c) 2020 Krystian Stasiowski
 * Distributed under the Boost Software License, Version 1.0. (See
 * http://www.boost.org/LICENSE_1_0.txt)
 */
static void PrettyPrintJson(std::ostream&             os,
                            boost::json::value const& jv,
                            std::string*              indent = nullptr);

static boost::json::value ReadJsonFile(QFile& file);
static boost::json::value ReadJsonStream(std::istream& is);

bool FromJsonInt64(const boost::json::object& json,
                   const std::string&         key,
                   int64_t&                   value,
                   const int64_t              defaultValue,
                   std::optional<int64_t>     minValue,
                   std::optional<int64_t>     maxValue)
{
   const boost::json::value* jv    = json.if_contains(key);
   bool                      dirty = true;

   if (jv != nullptr)
   {
      if (jv->is_int64())
      {
         value = boost::json::value_to<int64_t>(*jv);

         if (minValue.has_value() && value < *minValue)
         {
            BOOST_LOG_TRIVIAL(warning)
               << logPrefix_ << key << " less than minimum (" << value << " < "
               << *minValue << "), setting to: " << *minValue;
            value = *minValue;
         }
         else if (maxValue.has_value() && value > *maxValue)
         {
            BOOST_LOG_TRIVIAL(warning)
               << logPrefix_ << key << " greater than maximum (" << value
               << " > " << *maxValue << "), setting to: " << *maxValue;
            value = *maxValue;
         }
         else
         {
            dirty = false;
         }
      }
      else
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << key << " is not an int64 (" << jv->kind()
            << "), setting to default:" << defaultValue;
         value = defaultValue;
      }
   }
   else
   {
      BOOST_LOG_TRIVIAL(debug)
         << logPrefix_ << key
         << " is not present, setting to default: " << defaultValue;
      value = defaultValue;
   }

   return !dirty;
}

bool FromJsonString(const boost::json::object& json,
                    const std::string&         key,
                    std::string&               value,
                    const std::string&         defaultValue)
{
   const boost::json::value* jv    = json.if_contains(key);
   bool                      dirty = true;

   if (jv != nullptr)
   {
      if (jv->is_string())
      {
         value = boost::json::value_to<std::string>(*jv);
         dirty = false;
      }
      else
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << key
            << " is not a string, setting to default: " << defaultValue;
         value = defaultValue;
      }
   }
   else
   {
      BOOST_LOG_TRIVIAL(debug)
         << logPrefix_ << key
         << " is not present, setting to default: " << defaultValue;
      value = defaultValue;
   }

   return !dirty;
}

boost::json::value ReadJsonFile(const std::string& path)
{
   boost::json::value json;

   if (path.starts_with(":"))
   {
      QFile file(path.c_str());
      json = ReadJsonFile(file);
   }
   else
   {
      std::ifstream ifs {path};
      json = ReadJsonStream(ifs);
   }

   return json;
}

static boost::json::value ReadJsonFile(QFile& file)
{
   boost::json::value json;

   if (file.open(QIODevice::ReadOnly))
   {
      QTextStream jsonStream(&file);
      jsonStream.setEncoding(QStringConverter::Utf8);

      std::string        jsonSource = jsonStream.readAll().toStdString();
      std::istringstream is {jsonSource};

      json = ReadJsonStream(is);

      file.close();
   }
   else
   {
      BOOST_LOG_TRIVIAL(warning)
         << logPrefix_ << "Could not open file for reading: \""
         << file.fileName().toStdString() << "\"";
   }

   return json;
}

static boost::json::value ReadJsonStream(std::istream& is)
{
   std::string line;

   boost::json::stream_parser p;
   boost::json::error_code    ec;

   while (std::getline(is, line))
   {
      p.write(line, ec);
      if (ec)
      {
         BOOST_LOG_TRIVIAL(warning) << logPrefix_ << ec.message();
         return nullptr;
      }
   }

   p.finish(ec);
   if (ec)
   {
      BOOST_LOG_TRIVIAL(warning) << logPrefix_ << ec.message();
      return nullptr;
   }

   return p.release();
}

void WriteJsonFile(const std::string&        path,
                   const boost::json::value& json,
                   bool                      prettyPrint)
{
   std::ofstream ofs {path};

   if (!ofs.is_open())
   {
      BOOST_LOG_TRIVIAL(warning)
         << logPrefix_ << "Cannot write JSON file: \"" << path << "\"";
   }
   else
   {
      if (prettyPrint)
      {
         PrettyPrintJson(ofs, json);
      }
      else
      {
         ofs << json;
      }
      ofs.close();
   }
}

static void PrettyPrintJson(std::ostream&             os,
                            boost::json::value const& jv,
                            std::string*              indent)
{
   std::string indent_;
   if (!indent)
      indent = &indent_;
   switch (jv.kind())
   {
   case boost::json::kind::object:
   {
      os << "{\n";
      indent->append(4, ' ');
      auto const& obj = jv.get_object();
      if (!obj.empty())
      {
         auto it = obj.begin();
         for (;;)
         {
            os << *indent << boost::json::serialize(it->key()) << " : ";
            PrettyPrintJson(os, it->value(), indent);
            if (++it == obj.end())
               break;
            os << ",\n";
         }
      }
      os << "\n";
      indent->resize(indent->size() - 4);
      os << *indent << "}";
      break;
   }

   case boost::json::kind::array:
   {
      os << "[\n";
      indent->append(4, ' ');
      auto const& arr = jv.get_array();
      if (!arr.empty())
      {
         auto it = arr.begin();
         for (;;)
         {
            os << *indent;
            PrettyPrintJson(os, *it, indent);
            if (++it == arr.end())
               break;
            os << ",\n";
         }
      }
      os << "\n";
      indent->resize(indent->size() - 4);
      os << *indent << "]";
      break;
   }

   case boost::json::kind::string:
   {
      os << boost::json::serialize(jv.get_string());
      break;
   }

   case boost::json::kind::uint64: os << jv.get_uint64(); break;

   case boost::json::kind::int64: os << jv.get_int64(); break;

   case boost::json::kind::double_: os << jv.get_double(); break;

   case boost::json::kind::bool_:
      if (jv.get_bool())
         os << "true";
      else
         os << "false";
      break;

   case boost::json::kind::null: os << "null"; break;
   }

   if (indent->empty())
      os << "\n";
}

} // namespace json
} // namespace util
} // namespace qt
} // namespace scwx

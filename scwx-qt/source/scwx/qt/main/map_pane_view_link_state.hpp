#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QString>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace scwx::qt::main
{

static constexpr int64_t kMaxMapPaneCount {1024};

namespace detail
{
[[nodiscard]] inline bool MapPaneViewLinkJsonValueToBool(const QJsonValue& v,
                                                         bool* elementOk)
{
   if (v.isBool())
   {
      *elementOk = true;
      return v.toBool();
   }
   if (v.isDouble())
   {
      *elementOk = true;
      return v.toInt() != 0;
   }
   if (v.isString())
   {
      const QString s = v.toString().trimmed();
      if (s.compare(QStringLiteral("0"), Qt::CaseInsensitive) == 0)
      {
         *elementOk = true;
         return false;
      }
      if (s.compare(QStringLiteral("1"), Qt::CaseInsensitive) == 0)
      {
         *elementOk = true;
         return true;
      }
      if (s.compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0)
      {
         *elementOk = true;
         return false;
      }
      if (s.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0)
      {
         *elementOk = true;
         return true;
      }
   }
   *elementOk = false;
   return false;
}
} // namespace detail

// Parses "map_pane_view_link_state" JSON. nullopt: invalid, grid mismatch,
// bad length, or unrecognised element. |linked| may use bool, 0/1, or
// "true"/"false" / "0"/"1" strings. Rejects gw*gh > 1024 to cap allocation.
[[nodiscard]] inline std::optional<std::vector<bool>>
TryParseMapPaneViewLinkStateJson(const std::string& json,
                                 int64_t            expectedGw,
                                 int64_t            expectedGh)
{
   if (json.empty() || expectedGw < 1 || expectedGh < 1)
   {
      return std::nullopt;
   }
   {
      const int64_t en =
         static_cast<int64_t>(expectedGw) * static_cast<int64_t>(expectedGh);
      if (en < 1 || en > kMaxMapPaneCount)
      {
         return std::nullopt;
      }
   }

   QJsonParseError     err {};
   const QJsonDocument doc =
      QJsonDocument::fromJson(QByteArray::fromStdString(json), &err);
   if (err.error != QJsonParseError::NoError || !doc.isObject())
   {
      return std::nullopt;
   }

   const QJsonObject o    = doc.object();
   const QJsonValue  jgwV = o.value("gw");
   const QJsonValue  jghV = o.value("gh");
   const QJsonValue  lnk  = o.value("linked");
   if (jgwV.isNull() || jghV.isNull() || !lnk.isArray())
   {
      return std::nullopt;
   }

   const auto jgw = static_cast<int64_t>(jgwV.toDouble(0.0));
   const auto jgh = static_cast<int64_t>(jghV.toDouble(0.0));
   if (jgw != expectedGw || jgh != expectedGh || jgw < 1 || jgh < 1)
   {
      return std::nullopt;
   }

   const int64_t n64 = static_cast<int64_t>(jgw) * static_cast<int64_t>(jgh);
   if (n64 < 1 || n64 > kMaxMapPaneCount)
   {
      return std::nullopt;
   }
   const int countI = static_cast<int>(n64);
   if (static_cast<int64_t>(countI) != n64)
   {
      return std::nullopt;
   }

   const QJsonArray arr = lnk.toArray();
   if (arr.size() != countI)
   {
      return std::nullopt;
   }

   std::vector<bool> out;
   out.reserve(static_cast<std::size_t>(countI));
   for (int i = 0; i < countI; ++i)
   {
      bool       elOk = false;
      const bool b = detail::MapPaneViewLinkJsonValueToBool(arr.at(i), &elOk);
      if (!elOk)
      {
         return std::nullopt;
      }
      out.push_back(b);
   }

   return out;
}

[[nodiscard]] inline std::string SerializeMapPaneViewLinkStateJson(
   int64_t gw, int64_t gh, const std::vector<bool>& linked)
{
   if (gw < 1 || gh < 1)
   {
      return {};
   }
   const int64_t n64 = static_cast<int64_t>(gw) * static_cast<int64_t>(gh);
   if (n64 < 1 || n64 > kMaxMapPaneCount)
   {
      return {};
   }
   const int expectI = static_cast<int>(n64);
   if (static_cast<int64_t>(expectI) != n64)
   {
      return {};
   }
   if (static_cast<int>(linked.size()) != expectI)
   {
      return {};
   }
   QJsonObject root;
   root["gw"] = static_cast<double>(gw);
   root["gh"] = static_cast<double>(gh);
   QJsonArray a;
   for (const bool v : linked)
   {
      a.append(v);
   }
   root["linked"] = a;
   return QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString();
}

} // namespace scwx::qt::main

#include <scwx/provider/blitzortung_provider.hpp>
#include <scwx/util/json.hpp>
#include <scwx/util/logger.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>

#pragma warning(push)
#pragma warning(disable : 4267)
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#pragma warning(pop)

#include <boost/json.hpp>
#include <boost/system/error_code.hpp>

namespace scwx::provider
{

static const std::string logPrefix_ = "scwx::provider::blitzortung_provider";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

namespace beast     = boost::beast;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
namespace ssl       = net::ssl;
namespace json      = boost::json;

using tcp = net::ip::tcp;

static constexpr std::size_t kReceiveBufferSize_ {8192};

static const std::vector<std::string> kServers_ {"ws1.blitzortung.org",
                                                 "ws2.blitzortung.org",
                                                 "ws7.blitzortung.org",
                                                 "ws8.blitzortung.org"};

static constexpr auto kReconnectDelay_ = std::chrono::seconds(3);

class BlitzortungProvider::Impl
{
public:
   explicit Impl(BlitzortungProvider* self);
   ~Impl();

   Impl(const Impl&)            = delete;
   Impl& operator=(const Impl&) = delete;
   Impl(Impl&&)                 = delete;
   Impl& operator=(Impl&&)      = delete;

   void Start();
   void Stop();
   bool IsActive() const;

   void SetStrikeCallback(StrikeCallback callback);

private:
   void RunLoop(std::stop_token stopToken);
   void Connect(std::stop_token stopToken);
   void ProcessMessage(const std::string& payload);

   BlitzortungProvider* self_;

   std::unique_ptr<ssl::context>                                      sslCtx_;
   std::unique_ptr<net::io_context>                                   ioCtx_;
   std::unique_ptr<websocket::stream<beast::ssl_stream<tcp::socket>>> ws_;

   StrikeCallback strikeCallback_;

   std::atomic<bool> connected_ {false};
   std::size_t       serverIndex_ {0};

   std::mutex                startStopMutex_;
   std::jthread              worker_;
   boost::beast::flat_buffer buffer_ {kReceiveBufferSize_};
};

BlitzortungProvider::Impl::Impl(BlitzortungProvider* self) :
    self_(self),
    sslCtx_(std::make_unique<ssl::context>(ssl::context::tlsv12_client))
{
   // On Windows, default CA paths may not work with OpenSSL.
   // Certificate verification is disabled; the Blitzortung WebSocket
   // endpoint does not require client certificates.
   sslCtx_->set_verify_mode(ssl::verify_none);
}

BlitzortungProvider::Impl::~Impl()
{
   Stop();
}

void BlitzortungProvider::Impl::Start()
{
   const std::lock_guard<std::mutex> lock(startStopMutex_);

   if (worker_.joinable())
   {
      return;
   }

   serverIndex_ = 0;

   worker_ =
      std::jthread([this](std::stop_token st) { RunLoop(std::move(st)); });
}

void BlitzortungProvider::Impl::Stop()
{
   const std::lock_guard<std::mutex> lock(startStopMutex_);

   if (!worker_.joinable())
   {
      return;
   }

   worker_.request_stop();

   if (ws_)
   {
      try
      {
         ws_->next_layer().next_layer().cancel();
      }
      catch (...)
      {
      }
   }

   worker_.join();

   ws_.reset();
   ioCtx_.reset();
   connected_ = false;
}

bool BlitzortungProvider::Impl::IsActive() const
{
   return worker_.joinable();
}

void BlitzortungProvider::Impl::SetStrikeCallback(StrikeCallback callback)
{
   strikeCallback_ = callback;
}

void BlitzortungProvider::Impl::RunLoop(std::stop_token stopToken)
{
   ioCtx_ = std::make_unique<net::io_context>();

   while (!stopToken.stop_requested())
   {
      if (connected_)
      {
         buffer_.clear();

         try
         {
            ws_->read(buffer_);

            if (stopToken.stop_requested())
            {
               return;
            }

            std::string message = beast::buffers_to_string(buffer_.data());
            ProcessMessage(message);
         }
         catch (const boost::system::system_error& e)
         {
            if (!stopToken.stop_requested())
            {
               logger_->warn("WebSocket read error: {}", e.what());
            }
            connected_ = false;
         }
         catch (const std::exception& e)
         {
            if (!stopToken.stop_requested())
            {
               logger_->warn("WebSocket read exception: {}", e.what());
            }
            connected_ = false;
         }

         if (!connected_)
         {
            ws_.reset();
         }
      }

      if (!connected_ && !stopToken.stop_requested())
      {
         std::this_thread::sleep_for(kReconnectDelay_);
         Connect(stopToken);
      }
   }

   logger_->debug("Blitzortung provider thread stopped");
}

void BlitzortungProvider::Impl::Connect(std::stop_token stopToken)
{
   if (stopToken.stop_requested())
   {
      return;
   }

   const std::string& server = kServers_[serverIndex_ % kServers_.size()];
   ++serverIndex_;

   logger_->info("Connecting to wss://{}", server);

   try
   {
      ws_ = std::make_unique<websocket::stream<beast::ssl_stream<tcp::socket>>>(
         *ioCtx_, *sslCtx_);

      tcp::resolver resolver {*ioCtx_};
      auto          results = resolver.resolve(server, "443");

      net::connect(ws_->next_layer().next_layer(), results);

      ws_->next_layer().handshake(ssl::stream_base::client);
      // Set Origin header to match browser behavior
      ws_->set_option(websocket::stream_base::decorator(
         [](websocket::request_type& req)
         {
            req.set(beast::http::field::origin, "https://www.blitzortung.org");
         }));

      ws_->handshake(server, "/");

      // Send subscribe message (reverse-engineered Blitzortung protocol)
      ws_->write(net::buffer(std::string("{\"a\":111}")));

      connected_ = true;
      logger_->info("Connected to wss://{}", server);
   }
   catch (const std::exception& e)
   {
      logger_->warn("Connection to {} failed: {}", server, e.what());
      ws_.reset();
      connected_ = false;
   }
}

void BlitzortungProvider::Impl::ProcessMessage(const std::string& payload)
{
   try
   {
      // Decode UTF-8 to code points, then LZW decode.
      // The JS LZW decoder uses a FIXED threshold of 256 (never incremented).
      // Codes < 256 are literals; codes >= 256 are dictionary references.
      std::vector<int> codes;
      for (std::size_t i = 0; i < payload.size();)
      {
         unsigned char c = static_cast<unsigned char>(payload[i]);
         int           cp;
         std::size_t   len;
         if (c < 0x80)
         {
            cp  = c;
            len = 1;
         }
         else if ((c & 0xE0) == 0xC0 && i + 1 < payload.size())
         {
            cp  = (c & 0x1F) << 6 | (payload[i + 1] & 0x3F);
            len = 2;
         }
         else if ((c & 0xF0) == 0xE0 && i + 2 < payload.size())
         {
            cp = (c & 0x0F) << 12 | (payload[i + 1] & 0x3F) << 6 |
                 (payload[i + 2] & 0x3F);
            len = 3;
         }
         else if ((c & 0xF8) == 0xF0 && i + 3 < payload.size())
         {
            cp = (c & 0x07) << 18 | (payload[i + 1] & 0x3F) << 12 |
                 (payload[i + 2] & 0x3F) << 6 | (payload[i + 3] & 0x3F);
            len = 4;
         }
         else
         {
            ++i;
            continue;
         }
         i += len;
         codes.push_back(cp);
      }

      if (codes.size() < 2)
         return;

      std::unordered_map<int, std::vector<int>> dict;
      int                                       firstCode  = codes[0];
      std::vector<int>                          firstEntry = {firstCode};
      std::vector<int>                          prevEntry  = firstEntry;
      std::vector<int>                          result     = firstEntry;
      int                                       dictKey    = 256;

      for (std::size_t i = 1; i < codes.size(); ++i)
      {
         int              code = codes[i];
         std::vector<int> entry;

         if (code < 256)
         {
            entry = {code};
         }
         else if (auto it = dict.find(code); it != dict.end())
         {
            entry = it->second;
         }
         else
         {
            entry = prevEntry;
            entry.push_back(prevEntry.front());
         }

         result.insert(result.end(), entry.begin(), entry.end());

         std::vector<int> newEntry = prevEntry;
         newEntry.push_back(entry.front());
         dict[dictKey] = std::move(newEntry);
         ++dictKey;

         prevEntry = std::move(entry);
      }

      std::string output;
      output.reserve(result.size());
      for (int cp : result)
         output += static_cast<char>(cp & 0xFF);

      json::value value = json::parse(output);

      if (!value.is_object())
      {
         return;
      }

      const auto& obj = value.as_object();

      auto latIt   = obj.find("lat");
      auto lonIt   = obj.find("lon");
      auto delayIt = obj.find("delay");

      if (latIt == obj.end() || lonIt == obj.end() || delayIt == obj.end())
      {
         return;
      }

      if (!latIt->value().is_number() || !lonIt->value().is_number())
      {
         return;
      }

      StrikeData strike;
      strike.latitude  = latIt->value().to_number<double>();
      strike.longitude = lonIt->value().to_number<double>();

      if (auto latcIt = obj.find("latc");
          latcIt != obj.end() && latcIt->value().is_number())
      {
         strike.latitude += latcIt->value().to_number<double>();
      }
      if (auto loncIt = obj.find("lonc");
          loncIt != obj.end() && loncIt->value().is_number())
      {
         strike.longitude += loncIt->value().to_number<double>();
      }

      if (auto timeIt = obj.find("time");
          timeIt != obj.end() && timeIt->value().is_number())
      {
         strike.time_ns = timeIt->value().to_number<int64_t>();
      }

      if (auto polIt = obj.find("pol");
          polIt != obj.end() && polIt->value().is_number())
      {
         strike.polarity = static_cast<int>(polIt->value().to_number<double>());
      }

      if (delayIt->value().is_number())
      {
         strike.delay = static_cast<int>(delayIt->value().to_number<double>());
      }

      if (strikeCallback_)
      {
         strikeCallback_(strike);
      }
   }
   catch (const std::exception& e)
   {
      logger_->warn("Failed to process message: {}", e.what());
   }
}

BlitzortungProvider::BlitzortungProvider() : p(std::make_unique<Impl>(this)) {}

BlitzortungProvider::~BlitzortungProvider() = default;

void BlitzortungProvider::Start()
{
   p->Start();
}

void BlitzortungProvider::Stop()
{
   p->Stop();
}

bool BlitzortungProvider::IsActive() const
{
   return p->IsActive();
}

void BlitzortungProvider::SetStrikeCallback(StrikeCallback callback)
{
   p->SetStrikeCallback(callback);
}

} // namespace scwx::provider

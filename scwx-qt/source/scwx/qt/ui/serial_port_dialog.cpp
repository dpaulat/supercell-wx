#define __STDC_WANT_LIB_EXT1__ 1

#include "serial_port_dialog.hpp"
#include "ui_serial_port_dialog.h"

#include <scwx/util/logger.hpp>
#include <scwx/util/strings.hpp>

#include <unordered_map>

#include <QPushButton>
#include <QSerialPortInfo>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>

#if defined(_WIN32)
#   include <Windows.h>
#   include <SetupAPI.h>
#   include <initguid.h>
#   include <devguid.h>
#   include <devpkey.h>
#   include <tchar.h>
#   include <cstdlib>
#endif

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::serial_port_dialog";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class SerialPortDialog::Impl
{
public:
   struct PortProperties
   {
      std::string busReportedDeviceDescription_ {};
   };

   struct PortSettings
   {
      int         baudRate_ {-1}; // Positive
      std::string parity_ {"n"};  // [e]ven, [o]dd, [m]ark, [s]pace, [n]one
      int         dataBits_ {8};  // [4-8]
      float       stopBits_ {1};  // 1, 1.5, 2
      std::string
         flowControl_ {}; // "" (none), "p" (hardware), "x" (Xon / Xoff)
   };

   typedef std::unordered_map<std::string, QSerialPortInfo> PortInfoMap;
   typedef std::unordered_map<std::string, PortProperties>  PortPropertiesMap;
   typedef std::unordered_map<std::string, PortSettings>    PortSettingsMap;

   explicit Impl(SerialPortDialog* self) :
       self_ {self},
       model_ {new QStandardItemModel(self)},
       proxyModel_ {new QSortFilterProxyModel(self)}
   {
   }
   ~Impl() = default;

   void LogSerialPortInfo(const QSerialPortInfo& info);
   void RefreshSerialDevices();
   void UpdateModel();

   static void ReadComPortProperties(PortPropertiesMap& portPropertiesMap);
   static void ReadComPortSettings(PortSettingsMap& portSettingsMap);
   static void StorePortSettings(const std::string& portName,
                                 const std::string& settingsString,
                                 PortSettingsMap&   portSettingsMap);

#if defined(_WIN32)
   static std::string GetDevicePropertyString(HDEVINFO&        deviceInfoSet,
                                              SP_DEVINFO_DATA& deviceInfoData,
                                              DEVPROPKEY       propertyKey);
   static std::string GetRegistryValueDataString(HKEY hKey, LPCTSTR lpValue);
#endif

   SerialPortDialog*      self_;
   QStandardItemModel*    model_;
   QSortFilterProxyModel* proxyModel_;

   std::string selectedSerialPort_ {"?"};

   PortInfoMap       portInfoMap_ {};
   PortPropertiesMap portPropertiesMap_ {};
   PortSettingsMap   portSettingsMap_ {};
};

SerialPortDialog::SerialPortDialog(QWidget* parent) :
    QDialog(parent),
    p {std::make_unique<Impl>(this)},
    ui(new Ui::SerialPortDialog)
{
   ui->setupUi(this);

   p->proxyModel_->setSourceModel(p->model_);
   ui->serialPortView->setModel(p->proxyModel_);
   ui->serialPortView->setEditTriggers(
      QAbstractItemView::EditTrigger::NoEditTriggers);
   ui->serialPortView->sortByColumn(0, Qt::SortOrder::AscendingOrder);

   p->RefreshSerialDevices();

   ui->buttonBox->button(QDialogButtonBox::StandardButton::Ok)
      ->setEnabled(false);

   connect(ui->refreshButton,
           &QAbstractButton::clicked,
           this,
           [this]() { p->RefreshSerialDevices(); });

   connect(ui->serialPortView,
           &QTreeView::doubleClicked,
           this,
           [this]() { Q_EMIT accept(); });
   connect(
      ui->serialPortView->selectionModel(),
      &QItemSelectionModel::selectionChanged,
      this,
      [this](const QItemSelection& selected, const QItemSelection& deselected)
      {
         if (selected.size() == 0 && deselected.size() == 0)
         {
            // Items which stay selected but change their index are not
            // included in selected and deselected. Thus, this signal might
            // be emitted with both selected and deselected empty, if only
            // the indices of selected items change.
            return;
         }

         ui->buttonBox->button(QDialogButtonBox::Ok)
            ->setEnabled(selected.size() > 0);

         if (selected.size() > 0)
         {
            QModelIndex selectedIndex =
               p->proxyModel_->mapToSource(selected[0].indexes()[0]);
            selectedIndex        = p->model_->index(selectedIndex.row(), 0);
            QVariant variantData = p->model_->data(selectedIndex);
            if (variantData.typeId() == QMetaType::QString)
            {
               p->selectedSerialPort_ = variantData.toString().toStdString();
            }
            else
            {
               logger_->warn("Unexpected selection data type");
               p->selectedSerialPort_ = std::string {"?"};
            }
         }
         else
         {
            p->selectedSerialPort_ = std::string {"?"};
         }

         logger_->debug("Selected: {}", p->selectedSerialPort_);
      });
}

SerialPortDialog::~SerialPortDialog()
{
   delete ui;
}

std::string SerialPortDialog::serial_port()
{
   std::string serialPort = p->selectedSerialPort_;

#if !defined(_WIN32)
   auto it = p->portInfoMap_.find(p->selectedSerialPort_);
   if (it != p->portInfoMap_.cend())
   {
      serialPort = it->second.systemLocation().toStdString();
   }
#endif

   return serialPort;
}

int SerialPortDialog::baud_rate()
{
   int baudRate = -1;

   auto it = p->portSettingsMap_.find(p->selectedSerialPort_);
   if (it != p->portSettingsMap_.cend())
   {
      baudRate = it->second.baudRate_;
   }

   return baudRate;
}

void SerialPortDialog::Impl::LogSerialPortInfo(const QSerialPortInfo& info)
{
   logger_->trace("Serial Port:    {}", info.portName().toStdString());
   logger_->trace("  Description:  {}", info.description().toStdString());
   logger_->trace("  System Loc:   {}", info.systemLocation().toStdString());
   logger_->trace("  Manufacturer: {}", info.manufacturer().toStdString());
   logger_->trace("  Vendor ID:    {}", info.vendorIdentifier());
   logger_->trace("  Product ID:   {}", info.productIdentifier());
   logger_->trace("  Serial No:    {}", info.serialNumber().toStdString());
}

void SerialPortDialog::Impl::RefreshSerialDevices()
{
   QList<QSerialPortInfo> availablePorts = QSerialPortInfo::availablePorts();

   PortInfoMap       newPortInfoMap {};
   PortPropertiesMap newPortPropertiesMap {};
   PortSettingsMap   newPortSettingsMap {};

   for (auto& port : availablePorts)
   {
      LogSerialPortInfo(port);
      newPortInfoMap.insert_or_assign(port.portName().toStdString(), port);
   }

   ReadComPortProperties(newPortPropertiesMap);
   ReadComPortSettings(newPortSettingsMap);

   portInfoMap_.swap(newPortInfoMap);
   portPropertiesMap_.swap(newPortPropertiesMap);
   portSettingsMap_.swap(newPortSettingsMap);

   UpdateModel();
}

void SerialPortDialog::Impl::UpdateModel()
{
#if defined(_WIN32)
   static const QStringList headerLabels {
      tr("Port"), tr("Description"), tr("Device")};
#else
   static const QStringList headerLabels {
      tr("Port"), tr("Location"), tr("Description")};
#endif

   // Clear existing serial ports
   model_->clear();

   // Reset selected serial port and disable OK button
   selectedSerialPort_ = std::string {"?"};
   self_->ui->buttonBox->button(QDialogButtonBox::StandardButton::Ok)
      ->setEnabled(false);

   // Reset headers
   model_->setHorizontalHeaderLabels(headerLabels);

   QStandardItem* root = model_->invisibleRootItem();

   for (auto& port : portInfoMap_)
   {
      const QString portName    = port.second.portName();
      const QString description = port.second.description();

#if defined(_WIN32)
      QString device {};

      auto portPropertiesIt = portPropertiesMap_.find(port.first);
      if (portPropertiesIt != portPropertiesMap_.cend())
      {
         device = QString::fromStdString(
            portPropertiesIt->second.busReportedDeviceDescription_);
      }

      root->appendRow({new QStandardItem(portName),
                       new QStandardItem(description),
                       new QStandardItem(device)});
#else
      const QString systemLocation = port.second.systemLocation();

      root->appendRow({new QStandardItem(portName),
                       new QStandardItem(systemLocation),
                       new QStandardItem(description)});
#endif
   }

   for (int column = 0; column < model_->columnCount(); column++)
   {
      self_->ui->serialPortView->resizeColumnToContents(column);
   }
}

void SerialPortDialog::Impl::ReadComPortProperties(
   [[maybe_unused]] PortPropertiesMap& portPropertiesMap)
{
#if defined(_WIN32)
   GUID     classGuid  = GUID_DEVCLASS_PORTS;
   PCWSTR   enumerator = nullptr;
   HWND     hwndParent = nullptr;
   DWORD    flags      = DIGCF_PRESENT;
   HDEVINFO deviceInfoSet;

   // Retrieve COM port devices
   deviceInfoSet =
      SetupDiGetClassDevs(&classGuid, enumerator, hwndParent, flags);
   if (deviceInfoSet == INVALID_HANDLE_VALUE)
   {
      logger_->error("Error getting COM port devices");
      return;
   }

   DWORD           memberIndex = 0;
   SP_DEVINFO_DATA deviceInfoData {};
   deviceInfoData.cbSize = sizeof(deviceInfoData);
   flags                 = 0;

   // For each COM port device
   while (SetupDiEnumDeviceInfo(deviceInfoSet, memberIndex++, &deviceInfoData))
   {
      DWORD  scope      = DICS_FLAG_GLOBAL;
      DWORD  hwProfile  = 0;
      DWORD  keyType    = DIREG_DEV;
      REGSAM samDesired = KEY_READ;
      HKEY   devRegKey  = SetupDiOpenDevRegKey(
         deviceInfoSet, &deviceInfoData, scope, hwProfile, keyType, samDesired);

      if (devRegKey == INVALID_HANDLE_VALUE)
      {
         logger_->error("Unable to open device registry key: {}",
                        GetLastError());
         continue;
      }

      // Read Port Name and Device Description
      std::string portName =
         GetRegistryValueDataString(devRegKey, TEXT("PortName"));

      if (portName.empty())
      {
         // Ignore device without port name
         continue;
      }

      PortProperties properties {};
      properties.busReportedDeviceDescription_ = GetDevicePropertyString(
         deviceInfoSet, deviceInfoData, DEVPKEY_Device_BusReportedDeviceDesc);

      logger_->trace(
         "Port: {} ({})", portName, properties.busReportedDeviceDescription_);

      portPropertiesMap.emplace(portName, std::move(properties));

      RegCloseKey(devRegKey);
   }
#endif
}

void SerialPortDialog::Impl::ReadComPortSettings(
   [[maybe_unused]] PortSettingsMap& portSettingsMap)
{
#if defined(_WIN32)
   const LPCTSTR lpSubKey =
      TEXT("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Ports");
   DWORD   ulOptions  = 0;
   REGSAM  samDesired = KEY_READ;
   HKEY    hkResult;
   LSTATUS status;

   // Open Port Settings Key
   status = RegOpenKeyEx(
      HKEY_LOCAL_MACHINE, lpSubKey, ulOptions, samDesired, &hkResult);

   if (status == ERROR_SUCCESS)
   {
      DWORD   dwIndex = 0;
      TCHAR   valueName[MAX_PATH];
      LPDWORD lpReserved = nullptr;
      DWORD   type;
      TCHAR   valueData[64];
      char    buffer[MAX_PATH]; // Buffer for string conversion

      // Number of characters, not including terminating null
      static constexpr DWORD maxValueNameSize =
         sizeof(valueName) / sizeof(TCHAR) - 1;
      DWORD valueNameSize = maxValueNameSize;

      // Number of bytes
      DWORD valueDataSize = sizeof(valueData);

      static constexpr std::size_t bufferSize = sizeof(buffer);

      // Enumerate each port value
      while ((status = RegEnumValue(hkResult,
                                    dwIndex++,
                                    valueName,
                                    &valueNameSize,
                                    lpReserved,
                                    &type,
                                    reinterpret_cast<LPBYTE>(&valueData),
                                    &valueDataSize)) == ERROR_SUCCESS ||
             status == ERROR_MORE_DATA)
      {
         // Validate port value
         if (status == ERROR_SUCCESS &&            //
             type == REG_SZ &&                     //
             valueNameSize >= 5 &&                 // COM#:
             valueNameSize < sizeof(buffer) - 1 && // Strip off :
             valueDataSize > sizeof(TCHAR) &&      // Null character
             _tcsncmp(valueName, TEXT("COM"), 3) == 0)
         {
            errno_t     error;
            std::size_t returnValue;

            // Get port name
            if ((error = wcstombs_s(&returnValue,
                                    buffer,
                                    sizeof(buffer),
                                    valueName,
                                    valueNameSize - 1)) != 0)
            {
               logger_->error(
                  "Error converting registry value name to string: {}",
                  returnValue);
               continue;
            }

            std::string portName = buffer;

            // Get port data
            if ((error = wcstombs_s(&returnValue,
                                    buffer,
                                    sizeof(buffer),
                                    valueData,
                                    sizeof(buffer) - 1)) != 0)
            {
               logger_->error(
                  "Error converting registry value data to string: {}",
                  returnValue);
               continue;
            }

            std::string portData = buffer;

            logger_->trace("Port Settings: {} ({})", portName, portData);

            StorePortSettings(portName, portData, portSettingsMap);
         }

         valueNameSize = maxValueNameSize;
         valueDataSize = sizeof(valueData);
      }

      RegCloseKey(hkResult);
   }
   else
   {
      logger_->error("Could not open COM port settings registry key: {}",
                     status);
   }
#endif
}

void SerialPortDialog::Impl::StorePortSettings(
   const std::string& portName,
   const std::string& settingsString,
   PortSettingsMap&   portSettingsMap)
{
   PortSettings portSettings {};

   std::vector<std::string> tokenList =
      util::ParseTokens(settingsString, {",", ",", ",", ",", ","});

   try
   {
      if (tokenList.size() >= 1)
      {
         // Positive integer
         portSettings.baudRate_ = std::stoi(tokenList.at(0));
      }
      if (tokenList.size() >= 2)
      {
         // [e]ven, [o]dd, [m]ark, [s]pace, [n]one
         portSettings.parity_ = tokenList.at(1);
      }
      if (tokenList.size() >= 3)
      {
         // [4-8]
         portSettings.dataBits_ = std::stoi(tokenList.at(2));
      }
      if (tokenList.size() >= 4)
      {
         // 1, 1.5, 2
         portSettings.stopBits_ = std::stof(tokenList.at(3));
      }
      if (tokenList.size() >= 5)
      {
         // "" (none), "p" (hardware), "x" (Xon / Xoff)
         portSettings.flowControl_ = tokenList.at(4);
      }

      portSettingsMap.emplace(portName, std::move(portSettings));
   }
   catch (const std::exception&)
   {
      logger_->error(
         "Could not parse {} port settings: {}", portName, settingsString);
   }
}

#if defined(_WIN32)
std::string
SerialPortDialog::Impl::GetDevicePropertyString(HDEVINFO&        deviceInfoSet,
                                                SP_DEVINFO_DATA& deviceInfoData,
                                                DEVPROPKEY       propertyKey)
{
   std::string devicePropertyString {};

   DEVPROPTYPE        propertyType = 0;
   std::vector<TCHAR> propertyBuffer {};
   DWORD              requiredSize = 0;
   DWORD              flags        = 0;

   BOOL status = SetupDiGetDeviceProperty(deviceInfoSet,
                                          &deviceInfoData,
                                          &propertyKey,
                                          &propertyType,
                                          nullptr,
                                          0,
                                          &requiredSize,
                                          flags);

   if (requiredSize > 0)
   {
      propertyBuffer.reserve(requiredSize / sizeof(TCHAR));

      status = SetupDiGetDeviceProperty(
         deviceInfoSet,
         &deviceInfoData,
         &propertyKey,
         &propertyType,
         reinterpret_cast<PBYTE>(propertyBuffer.data()),
         static_cast<DWORD>(propertyBuffer.capacity() * sizeof(TCHAR)),
         &requiredSize,
         flags);
   }

   if (status && requiredSize > 0)
   {
      errno_t     error;
      std::size_t returnValue;

      devicePropertyString.resize(requiredSize / sizeof(TCHAR));

      if ((error = wcstombs_s(&returnValue,
                              devicePropertyString.data(),
                              devicePropertyString.size(),
                              propertyBuffer.data(),
                              _TRUNCATE)) != 0)
      {
         logger_->error("Error converting device property string: {}",
                        returnValue);
         devicePropertyString.clear();
      }
      else if (!devicePropertyString.empty())
      {
         // Remove trailing null
         devicePropertyString.erase(devicePropertyString.size() - 1);
      }
   }

   return devicePropertyString;
}

std::string SerialPortDialog::Impl::GetRegistryValueDataString(HKEY    hKey,
                                                               LPCTSTR lpValue)
{
   std::string dataString {};

   LPCTSTR lpSubKey = nullptr;
   DWORD   dwFlags  = RRF_RT_REG_SZ; // Restrict type to REG_SZ
   DWORD   dwType;

   std::vector<TCHAR> dataBuffer {};
   DWORD              dataBufferSize = 0;

   LSTATUS status = RegGetValue(
      hKey, lpSubKey, lpValue, dwFlags, &dwType, nullptr, &dataBufferSize);

   if (status == ERROR_SUCCESS && dataBufferSize > 0)
   {
      dataBuffer.reserve(dataBufferSize / sizeof(TCHAR));

      status = RegGetValue(hKey,
                           lpSubKey,
                           lpValue,
                           dwFlags,
                           &dwType,
                           reinterpret_cast<PVOID>(dataBuffer.data()),
                           &dataBufferSize);
   }

   if (status == ERROR_SUCCESS && dataBufferSize > 0)
   {
      errno_t     error;
      std::size_t returnValue;

      dataString.resize(dataBufferSize / sizeof(TCHAR));

      if ((error = wcstombs_s(&returnValue,
                              dataString.data(),
                              dataString.size(),
                              dataBuffer.data(),
                              _TRUNCATE)) != 0)
      {
         logger_->error("Error converting registry value data string: {}",
                        returnValue);
         dataString.clear();
      }
      else if (!dataString.empty())
      {
         // Remove trailing null
         dataString.erase(dataString.size() - 1);
      }
   }

   return dataString;
}
#endif

} // namespace ui
} // namespace qt
} // namespace scwx

#define __STDC_WANT_LIB_EXT1__ 1

#include "serial_port_dialog.hpp"
#include "ui_serial_port_dialog.h"

#include <scwx/util/logger.hpp>

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
   explicit Impl(SerialPortDialog* self) :
       self_ {self}, model_ {new QStandardItemModel(self)}
   {
   }
   ~Impl() = default;

   void LogSerialPortInfo(const QSerialPortInfo& info);
   void ReadComPortProperties();
   void ReadComPortSettings();
   void RefreshSerialDevices();

#if defined(_WIN32)
   static std::string GetDevicePropertyString(HDEVINFO&        deviceInfoSet,
                                              SP_DEVINFO_DATA& deviceInfoData,
                                              DEVPROPKEY       propertyKey);
   static std::string GetRegistryValueDataString(HKEY hKey, LPCTSTR lpValue);
#endif

   SerialPortDialog*   self_;
   QStandardItemModel* model_;

   std::string selectedSerialPort_ {"?"};
};

SerialPortDialog::SerialPortDialog(QWidget* parent) :
    QDialog(parent),
    p {std::make_unique<Impl>(this)},
    ui(new Ui::SerialPortDialog)
{
   ui->setupUi(this);

   connect(ui->refreshButton,
           &QAbstractButton::clicked,
           this,
           [this]() { p->RefreshSerialDevices(); });
}

SerialPortDialog::~SerialPortDialog()
{
   delete ui;
}

std::string SerialPortDialog::serial_port()
{
   return p->selectedSerialPort_;
}

void SerialPortDialog::Impl::LogSerialPortInfo(const QSerialPortInfo& info)
{
   logger_->debug("Serial Port:    {}", info.portName().toStdString());
   logger_->debug("  Description:  {}", info.description().toStdString());
   logger_->debug("  System Loc:   {}", info.systemLocation().toStdString());
   logger_->debug("  Manufacturer: {}", info.manufacturer().toStdString());
   logger_->debug("  Vendor ID:    {}", info.vendorIdentifier());
   logger_->debug("  Product ID:   {}", info.productIdentifier());
   logger_->debug("  Serial No:    {}", info.serialNumber().toStdString());
}

void SerialPortDialog::Impl::RefreshSerialDevices()
{
   QList<QSerialPortInfo> availablePorts = QSerialPortInfo::availablePorts();

   for (auto& port : availablePorts)
   {
      LogSerialPortInfo(port);
   }

   ReadComPortProperties();
   ReadComPortSettings();
}

void SerialPortDialog::Impl::ReadComPortProperties()
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

      std::string busReportedDeviceDesc = GetDevicePropertyString(
         deviceInfoSet, deviceInfoData, DEVPKEY_Device_BusReportedDeviceDesc);

      logger_->debug("Port: {} ({})", portName, busReportedDeviceDesc);

      RegCloseKey(devRegKey);
   }
#endif
}

void SerialPortDialog::Impl::ReadComPortSettings()
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

            logger_->debug("Port Settings: {} ({})", portName, portData);
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

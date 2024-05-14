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
#   include <cstdlib>
#   include <tchar.h>
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
   void ReadComPortSettings();
   void RefreshSerialDevices();

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

   ReadComPortSettings();
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
                                    dwIndex,
                                    valueName,
                                    &valueNameSize,
                                    lpReserved,
                                    &type,
                                    (LPBYTE) &valueData,
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

            logger_->debug("Port {} has data \"{}\"", portName, portData);
         }

         valueNameSize = maxValueNameSize;
         valueDataSize = sizeof(valueData);
         ++dwIndex;
      }

      RegCloseKey(hkResult);
   }
   else
   {
      logger_->warn("Could not open COM port settings registry key: {}",
                    status);
   }
#endif
}

} // namespace ui
} // namespace qt
} // namespace scwx

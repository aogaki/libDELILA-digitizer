#ifndef DIGITIZER_HPP
#define DIGITIZER_HPP

#include <deque>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

#include "ConfigurationManager.hpp"
#include "EventData.hpp"
#include "PSD2Data.hpp"
#include "ParameterValidator.hpp"
#include "RawData.hpp"
#include "RawToPSD2.hpp"

namespace DELILA
{
namespace Digitizer
{

enum class DigitizerType {
  PSD1,
  PSD2,
  PHA1,
  PHA2,
  QDC1,
  SCOPE1,
  SCOPE2,
  UNKNOWN
};

class Digitizer
{
 public:
  // Constructor/Destructor
  Digitizer();
  ~Digitizer();

  // Main lifecycle methods
  bool Initialize(const ConfigurationManager &config);
  bool Configure();
  bool StartAcquisition();
  bool StopAcquisition();

  // Data access
  std::unique_ptr<std::vector<std::unique_ptr<EventData>>> GetEventData();

  // Device information
  const nlohmann::json &GetDeviceTreeJSON() const { return fDeviceTree; }
  void PrintDeviceInfo();

  // Control methods
  bool SendSWTrigger();
  bool CheckStatus();

  // Getters
  uint64_t GetHandle() const { return fHandle; }
  uint8_t GetModuleNumber() const { return fModuleNumber; }

 private:
  // === Hardware Interface ===
  uint64_t fHandle = 0;
  uint64_t fReadDataHandle = 0;
  uint64_t fRecordLength = 0;
  size_t fMaxRawDataSize = 0;

  // === Configuration ===
  std::string fURL;
  bool fDebugFlag = false;
  uint32_t fNThreads = 1;
  uint8_t fModuleNumber = 0;
  std::vector<std::array<std::string, 2>> fConfig;

  // === Device Information ===
  nlohmann::json fDeviceTree;
  DigitizerType fDigitizerType = DigitizerType::UNKNOWN;

  // === Data Processing ===
  std::unique_ptr<RawToPSD2> fRawToPSD2;
  std::unique_ptr<ParameterValidator> fParameterValidator;
  bool fDataTakingFlag = false;
  std::vector<std::thread> fReadDataThreads;
  std::mutex fReadDataMutex;

  // === EventData Conversion ===
  std::thread fEventConversionThread;
  std::unique_ptr<std::vector<std::unique_ptr<EventData>>> fEventDataVec;
  std::mutex fEventDataMutex;

  // === Event Data Processing ===
  // Note: Event data processing is now handled by RawToPSD2

  // === Hardware Communication ===
  bool Open(const std::string &url);
  bool Close();
  bool CheckError(int err);
  bool SendCommand(const std::string &path);
  bool GetParameter(const std::string &path, std::string &value);
  bool SetParameter(const std::string &path, const std::string &value);

  // === Device Tree Management ===
  std::string GetDeviceTree();
  void DetermineDigitizerType();

  // === Configuration Validation ===
  bool ValidateParameters();

  // === Configuration Helpers ===
  bool ResetDigitizer();
  bool ApplyConfiguration();
  bool ConfigureRecordLength();
  bool ConfigureMaxRawDataSize();
  bool InitializeDataConverter();
  bool ConfigureSampleRate();

  // === Data Acquisition ===
  bool EndpointConfigure();
  nlohmann::json GetReadDataFormatRAW();
  void ReadDataThread();
  int ReadDataWithLock(std::unique_ptr<RawData_t> &rawData, int timeOut);

  // === EventData Conversion ===
  void EventConversionThread();
  std::unique_ptr<EventData> ConvertPSD2ToEventData(const PSD2Data_t& psd2Data);

  // === Note: All data is automatically converted to EventData ===
};

}  // namespace Digitizer
}  // namespace DELILA

#endif  // DIGITIZER_HPP
#ifndef DIGITIZER1_HPP
#define DIGITIZER1_HPP

#include <deque>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

#include "ConfigurationManager.hpp"
#include "EventData.hpp"
#include "IDigitizer.hpp"
#include "ParameterValidator.hpp"
#include "RawData.hpp"
#include "Dig1Decoder.hpp"

namespace DELILA
{
namespace Digitizer
{

class Digitizer1 : public IDigitizer
{
 public:
  // Constructor/Destructor
  Digitizer1();
  ~Digitizer1() override;

  // Main lifecycle methods
  bool Initialize(const ConfigurationManager &config) override;
  bool Configure() override;
  bool StartAcquisition() override;
  bool StopAcquisition() override;

  // Data access
  std::unique_ptr<std::vector<std::unique_ptr<EventData>>> GetEventData() override;

  // Device information
  const nlohmann::json &GetDeviceTreeJSON() const override { return fDeviceTree; }
  void PrintDeviceInfo() override;
  DigitizerType GetType() const override { return fDigitizerType; }

  // Control methods
  bool SendSWTrigger() override;
  bool CheckStatus() override;

  // Getters
  uint64_t GetHandle() const override { return fHandle; }
  uint8_t GetModuleNumber() const override { return fModuleNumber; }

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
  std::unique_ptr<Dig1Decoder> fDig1Decoder;
  std::unique_ptr<ParameterValidator> fParameterValidator;
  bool fDataTakingFlag = false;
  std::vector<std::thread> fReadDataThreads;
  std::mutex fReadDataMutex;

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
  bool ConfigureSampleRate();

  // === Data Acquisition ===
  bool EndpointConfigure();
  nlohmann::json GetReadDataFormatRAW();
  void ReadDataThread();
  int ReadDataWithLock(std::unique_ptr<RawData_t> &rawData, int timeOut);

  // === EventData Conversion (Dig1-specific) ===
  void EventConversionThread();
  std::unique_ptr<EventData> ConvertRawToEventData(const RawData_t &rawData);
  void ProcessDig1RawData(const std::vector<uint8_t> &data, 
                          std::vector<std::unique_ptr<EventData>> &eventDataVec);
  void DecodeDig1Event(const std::vector<uint8_t>::const_iterator &dataStart, 
                       size_t &wordIndex, EventData &eventData);
};

}  // namespace Digitizer
}  // namespace DELILA

#endif  // DIGITIZER1_HPP
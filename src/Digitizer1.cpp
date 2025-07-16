#include "Digitizer1.hpp"

#include <CAEN_FELib.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

namespace DELILA
{
namespace Digitizer
{

Digitizer1::Digitizer1() {}

Digitizer1::~Digitizer1()
{
  StopAcquisition();
  Close();
}

bool Digitizer1::Initialize(const ConfigurationManager &config)
{
  // Get URL from configuration
  fURL = config.GetParameter("URL");
  if (fURL.empty()) {
    std::cerr << "URL is not set in configuration" << std::endl;
    return false;
  }

  // Get debug flag if available
  auto debugStr = config.GetParameter("Debug");
  if (!debugStr.empty()) {
    std::transform(debugStr.begin(), debugStr.end(), debugStr.begin(),
                   ::tolower);
    fDebugFlag = (debugStr == "true" || debugStr == "1" || debugStr == "yes");
  }

  // Get number of threads if available
  auto threadsStr = config.GetParameter("Threads");
  if (!threadsStr.empty()) {
    try {
      fNThreads = std::stoi(threadsStr);
      if (fNThreads < 1) fNThreads = 1;
    } catch (...) {
      fNThreads = 1;
    }
  }

  // Get module ID if available
  auto modIdStr = config.GetParameter("ModID");
  if (!modIdStr.empty()) {
    try {
      int modId = std::stoi(modIdStr);
      if (modId >= 0 && modId <= 255) {
        fModuleNumber = static_cast<uint8_t>(modId);
        std::cout << "Module ID set to: " << static_cast<int>(fModuleNumber)
                  << std::endl;
      }
    } catch (...) {
      fModuleNumber = 0;  // Default to 0 if parsing fails
      std::cout << "Invalid ModID format, using default: 0" << std::endl;
    }
  } else {
    std::cout << "No ModID specified in config, using default: 0" << std::endl;
  }

  // Get all digitizer-specific configuration
  fConfig = config.GetDigitizerConfig();

  // Open the digitizer
  if (Open(fURL)) {
    GetDeviceTree();
    // std::cout << fDeviceTree.dump(2) << std::endl;
    return true;
  }
  return false;
}

bool Digitizer1::Configure()
{
  // Reset the digitizer to a known state
  if (!ResetDigitizer()) {
    return false;
  }

  // Validate parameters before applying them
  if (!ValidateParameters()) {
    std::cerr << "Parameter validation failed. Aborting configuration."
              << std::endl;
    return false;
  }

  // Apply all configuration parameters
  if (!ApplyConfiguration()) {
    return false;
  }

  // Configure record length
  if (!ConfigureRecordLength()) {
    return false;
  }

  // Configure endpoints for data readout
  if (!EndpointConfigure()) {
    return false;
  }

  // Configure max raw data size
  if (!ConfigureMaxRawDataSize()) {
    return false;
  }

  // Configure sample rate
  if (!ConfigureSampleRate()) {
    return false;
  }

  return true;
}

bool Digitizer1::StartAcquisition()
{
  std::cout << "Start acquisition" << std::endl;

  // Dig1Decoder should already be created in ConfigureSampleRate()
  if (!fDig1Decoder) {
    std::cerr << "Dig1Decoder not initialized - this should not happen!"
              << std::endl;
    return false;
  }

  // Start data acquisition threads
  fDataTakingFlag = true;
  for (uint32_t i = 0; i < fNThreads; i++) {
    fReadDataThreads.emplace_back(&Digitizer1::ReadDataThread, this);
  }

  // Note: Dig1Decoder handles data conversion internally in its threads

  bool status = true;
  // Only send software start command if startmode is set to START_MODE_SW
  std::string startMode;
  if (GetParameter("/par/startmode", startMode) &&
      startMode == "START_MODE_SW") {
    std::cout
        << "startmode is START_MODE_SW - waiting before sending software start "
           "command"
        << std::endl;
    // Short sleep to allow other digitizers to prepare
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "Sending software start command" << std::endl;
    status &= SendCommand("/cmd/ArmAcquisition");
  } else {
    std::cout << "startmode is not START_MODE_SW (" << startMode
              << ") - skipping software start command" << std::endl;
    // Arm the acquisition
    status &= SendCommand("/cmd/ArmAcquisition");
  }

  return status;
}

bool Digitizer1::StopAcquisition()
{
  std::cout << "Stop acquisition" << std::endl;

  auto status = SendCommand("/cmd/DisarmAcquisition");

  // Wait for any remaining data
  while (true) {
    if (CAEN_FELib_HasData(fReadDataHandle, 100) == CAEN_FELib_Timeout) {
      break;
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  fDataTakingFlag = false;

  // Stop data acquisition threads
  for (auto &thread : fReadDataThreads) {
    if (thread.joinable()) {
      thread.join();
    }
  }
  fReadDataThreads.clear();

  // Stop EventData conversion thread
  // Dig1Decoder will stop automatically when threads join

  return status;
}

std::unique_ptr<std::vector<std::unique_ptr<EventData>>>
Digitizer1::GetEventData()
{
  // Get EventData directly from Dig1Decoder
  if (!fDig1Decoder) {
    std::cerr << "Warning: Dig1Decoder not initialized in GetEventData()"
              << std::endl;
    return std::make_unique<std::vector<std::unique_ptr<EventData>>>();
  }

  auto data = fDig1Decoder->GetEventData();
  if (fDebugFlag && data && !data->empty()) {
    std::cout << "Retrieved " << data->size() << " events from Dig1Decoder"
              << std::endl;
  }
  return data;
}

void Digitizer1::PrintDeviceInfo()
{
  if (fDeviceTree.empty()) {
    std::cerr << "Device tree is empty. Initialize the digitizer first."
              << std::endl;
    return;
  }

  std::cout << "\n=== Device Information ===" << std::endl;

  // Print Model Name from par section
  if (fDeviceTree.contains("par") && fDeviceTree["par"].contains("modelname")) {
    std::string modelName = fDeviceTree["par"]["modelname"]["value"];
    std::cout << "Model Name: " << modelName << std::endl;
  } else {
    std::cout << "Model Name: Not found" << std::endl;
  }

  // Print Serial Number from par section
  if (fDeviceTree.contains("par") && fDeviceTree["par"].contains("serialnum")) {
    std::string serialNum = fDeviceTree["par"]["serialnum"]["value"];
    std::cout << "Serial Number: " << serialNum << std::endl;
  } else {
    std::cout << "Serial Number: Not found" << std::endl;
  }

  // Print Firmware Type from par section
  if (fDeviceTree.contains("par") && fDeviceTree["par"].contains("fwtype")) {
    std::string fwType = fDeviceTree["par"]["fwtype"]["value"];
    std::cout << "Firmware Type: " << fwType << std::endl;
  } else {
    std::cout << "Firmware Type: Not found" << std::endl;
  }

  // Print Digitizer Type
  std::string typeStr;
  switch (fDigitizerType) {
    case DigitizerType::PSD1:
      typeStr = "PSD1";
      break;
    case DigitizerType::PSD2:
      typeStr = "PSD2";
      break;
    case DigitizerType::PHA1:
      typeStr = "PHA1";
      break;
    case DigitizerType::PHA2:
      typeStr = "PHA2";
      break;
    case DigitizerType::QDC1:
      typeStr = "QDC1";
      break;
    case DigitizerType::SCOPE1:
      typeStr = "SCOPE1";
      break;
    case DigitizerType::SCOPE2:
      typeStr = "SCOPE2";
      break;
    case DigitizerType::UNKNOWN:
      typeStr = "UNKNOWN";
      break;
  }
  std::cout << "Digitizer Type: " << typeStr << std::endl;

  std::cout << "=========================" << std::endl;
}

bool Digitizer1::SendSWTrigger()
{
  auto err = CAEN_FELib_SendCommand(fHandle, "/cmd/SendSwTrigger");
  CheckError(err);
  return err == CAEN_FELib_Success;
}

bool Digitizer1::CheckStatus()
{
  std::cout << "Digitizer1::CheckStatus() - skeleton implementation"
            << std::endl;

  // TODO: Implement dig1 status check
  // - Check hardware connection
  // - Check acquisition status
  // - Check error conditions

  return false;  // Skeleton returns false
}

// === Private Implementation Methods ===

bool Digitizer1::Open(const std::string &url)
{
  std::cout << "Open URL: " << url << std::endl;
  auto err = CAEN_FELib_Open(url.c_str(), &fHandle);
  CheckError(err);
  return err == CAEN_FELib_Success;
}

bool Digitizer1::Close()
{
  std::cout << "Close digitizer" << std::endl;
  auto err = CAEN_FELib_Close(fHandle);
  CheckError(err);
  return err == CAEN_FELib_Success;
}

bool Digitizer1::CheckError(int err)
{
  auto errCode = static_cast<CAEN_FELib_ErrorCode>(err);
  if (errCode != CAEN_FELib_Success) {
    std::cout << "\x1b[31m";

    auto errName = std::string(32, '\0');
    CAEN_FELib_GetErrorName(errCode, errName.data());
    std::cerr << "Error code: " << errName << std::endl;

    auto errDesc = std::string(256, '\0');
    CAEN_FELib_GetErrorDescription(errCode, errDesc.data());
    std::cerr << "Error description: " << errDesc << std::endl;

    auto details = std::string(1024, '\0');
    CAEN_FELib_GetLastError(details.data());
    std::cerr << "Details: " << details << std::endl;

    std::cout << "\x1b[0m" << std::endl;
  }

  return errCode == CAEN_FELib_Success;
}

bool Digitizer1::SendCommand(const std::string &path)
{
  auto err = CAEN_FELib_SendCommand(fHandle, path.c_str());
  CheckError(err);

  return err == CAEN_FELib_Success;
}

bool Digitizer1::GetParameter(const std::string &path, std::string &value)
{
  char buf[256];
  auto err = CAEN_FELib_GetValue(fHandle, path.c_str(), buf);
  CheckError(err);
  value = std::string(buf);

  return err == CAEN_FELib_Success;
}

bool Digitizer1::SetParameter(const std::string &path, const std::string &value)
{
  auto err = CAEN_FELib_SetValue(fHandle, path.c_str(), value.c_str());
  CheckError(err);

  return err == CAEN_FELib_Success;
}

std::string Digitizer1::GetDeviceTree()
{
  if (fHandle == 0) {
    std::cerr << "Digitizer not initialized" << std::endl;
    return "";
  }

  auto jsonSize = CAEN_FELib_GetDeviceTree(fHandle, nullptr, 0) + 1;
  char *json = new char[jsonSize];
  CAEN_FELib_GetDeviceTree(fHandle, json, jsonSize);

  std::string jsonStr = json;
  delete[] json;

  // Parse and store the JSON
  try {
    fDeviceTree = nlohmann::json::parse(jsonStr);
    // Determine digitizer type after parsing device tree
    DetermineDigitizerType();
    // Create parameter validator with device tree
    fParameterValidator = std::make_unique<ParameterValidator>(fDeviceTree);
  } catch (const nlohmann::json::parse_error &e) {
    std::cerr << "Failed to parse device tree JSON: " << e.what() << std::endl;
    fDeviceTree = nlohmann::json();  // Reset to empty JSON
  }

  return jsonStr;
}

void Digitizer1::DetermineDigitizerType()
{
  fDigitizerType = DigitizerType::UNKNOWN;  // Default

  if (!fDeviceTree.contains("par")) {
    return;
  }

  std::string modelName = "";
  std::string fwType = "";

  // Get model name
  if (fDeviceTree["par"].contains("modelname")) {
    modelName = fDeviceTree["par"]["modelname"]["value"];
  }

  // Get firmware type
  if (fDeviceTree["par"].contains("fwtype")) {
    fwType = fDeviceTree["par"]["fwtype"]["value"];
  }

  // Convert to lowercase for easier comparison
  std::transform(modelName.begin(), modelName.end(), modelName.begin(),
                 ::tolower);
  std::transform(fwType.begin(), fwType.end(), fwType.begin(), ::tolower);

  // Debug output to see what we're working with
  std::cout << "DEBUG: Model Name: '" << modelName << "'" << std::endl;
  std::cout << "DEBUG: Firmware Type: '" << fwType << "'" << std::endl;

  // Determine digitizer type based on firmware type and model
  if (fwType.find("psd") != std::string::npos) {
    // Check for DPP_PSD (underscore) = PSD2, DPP-PSD (hyphen) = PSD1
    if (fwType.find("dpp_psd") != std::string::npos) {
      fDigitizerType = DigitizerType::PSD2;
    } else if (fwType.find("dpp-psd") != std::string::npos) {
      fDigitizerType = DigitizerType::PSD1;
    } else {
      // Fall back to model name logic for PSD - extract 4-digit number
      std::string modelNumber = "";
      for (char c : modelName) {
        if (std::isdigit(c)) {
          modelNumber += c;
        }
      }
      if (modelNumber.length() >= 4 && modelNumber[0] == '2') {
        fDigitizerType = DigitizerType::PSD2;
      } else {
        fDigitizerType = DigitizerType::PSD1;
      }
    }
  } else if (fwType.find("pha") != std::string::npos) {
    // Similar logic for PHA
    if (fwType.find("dpp_pha") != std::string::npos) {
      fDigitizerType = DigitizerType::PHA2;
    } else if (fwType.find("dpp-pha") != std::string::npos) {
      fDigitizerType = DigitizerType::PHA1;
    } else {
      // Fall back to model name logic for PHA - extract 4-digit number
      std::string modelNumber = "";
      for (char c : modelName) {
        if (std::isdigit(c)) {
          modelNumber += c;
        }
      }
      if (modelNumber.length() >= 4 && modelNumber[0] == '2') {
        fDigitizerType = DigitizerType::PHA2;
      } else {
        fDigitizerType = DigitizerType::PHA1;
      }
    }
  } else if (fwType.find("qdc") != std::string::npos) {
    fDigitizerType = DigitizerType::QDC1;
  } else if (fwType.find("scope") != std::string::npos ||
             fwType.find("oscilloscope") != std::string::npos) {
    // Similar logic for SCOPE
    if (fwType.find("dpp_scope") != std::string::npos ||
        fwType.find("scope_dpp") != std::string::npos) {
      fDigitizerType = DigitizerType::SCOPE2;
    } else if (fwType.find("dpp-scope") != std::string::npos ||
               fwType.find("scope-dpp") != std::string::npos) {
      fDigitizerType = DigitizerType::SCOPE1;
    } else {
      // Fall back to model name logic for SCOPE - extract 4-digit number
      std::string modelNumber = "";
      for (char c : modelName) {
        if (std::isdigit(c)) {
          modelNumber += c;
        }
      }
      if (modelNumber.length() >= 4 && modelNumber[0] == '2') {
        fDigitizerType = DigitizerType::SCOPE2;
      } else {
        fDigitizerType = DigitizerType::SCOPE1;
      }
    }
  }

  // Final fallback: use model name if firmware type didn't match any pattern
  if (fDigitizerType == DigitizerType::UNKNOWN) {
    // Extract just the numeric part from model name like "DT5230" or "VX2730"
    std::string modelNumber = "";
    for (char c : modelName) {
      if (std::isdigit(c)) {
        modelNumber += c;
      }
    }

    if (modelNumber.length() >= 4) {
      char firstDigit = modelNumber[0];
      if (firstDigit == '2') {
        // Need to guess type based on model number ranges
        if (modelNumber.substr(0, 2) == "27") {  // 27xx series typically PSD
          fDigitizerType = DigitizerType::PSD2;
        } else if (modelNumber.substr(0, 3) ==
                   "274") {  // 274x series typically SCOPE
          fDigitizerType = DigitizerType::SCOPE2;
        }
      }
    }
  }
}

bool Digitizer1::ValidateParameters()
{
  if (!fParameterValidator) {
    std::cerr
        << "Parameter validator not initialized. Device tree may be missing."
        << std::endl;
    return false;
  }

  auto validationSummary = fParameterValidator->ValidateParameters(fConfig);
  return validationSummary.invalidParameters == 0;
}

bool Digitizer1::ResetDigitizer() { return SendCommand("/cmd/Reset"); }

bool Digitizer1::ApplyConfiguration()
{
  bool status = true;
  for (const auto &config : fConfig) {
    // Only use parameters that start with '/' (CAEN digitizer paths)
    if (!config[0].empty() && config[0][0] == '/') {
      status &= SetParameter(config[0], config[1]);
    }
  }
  return status;
}

bool Digitizer1::ConfigureRecordLength()
{
  std::string buf;
  if (!GetParameter("/par/reclen", buf)) {
    std::cerr << "Failed to get record length parameter" << std::endl;
    return false;
  }

  auto rl = std::stoi(buf);
  if (rl < 0) {
    std::cerr << "Invalid record length: " << rl << std::endl;
    return false;
  }

  fRecordLength = rl;
  std::cout << "Record length: " << fRecordLength << std::endl;
  return true;
}

bool Digitizer1::ConfigureMaxRawDataSize()
{
  std::string buf;
  if (!GetParameter("/par/MaxRawDataSize", buf)) {
    std::cerr << "Failed to get max raw data size" << std::endl;
    return false;
  }

  fMaxRawDataSize = std::stoi(buf);
  std::cout << "Max raw data size: " << fMaxRawDataSize << std::endl;
  return true;
}

bool Digitizer1::ConfigureSampleRate()
{
  std::string buf;
  if (!GetParameter("/par/ADC_SamplRate", buf) || buf.empty()) {
    std::cerr << "Failed to get ADC sample rate" << std::endl;
    return false;
  }

  auto adcSamplRateMHz = std::stoi(buf);  // Sample rate in MHz
  if (adcSamplRateMHz <= 0) {
    std::cerr << "Invalid ADC sample rate: " << adcSamplRateMHz << " MHz"
              << std::endl;
    return false;
  }

  // Calculate time per sample in nanoseconds: (1000 ns) / (rate in MHz)
  uint32_t timeStepNs = 1000 / adcSamplRateMHz;

  // Create Dig1Decoder if not already created
  if (!fDig1Decoder) {
    fDig1Decoder = std::make_unique<Dig1Decoder>(fNThreads);
  }

  // Configure Dig1Decoder
  fDig1Decoder->SetTimeStep(timeStepNs);
  fDig1Decoder->SetDumpFlag(fDebugFlag);
  fDig1Decoder->SetModuleNumber(fModuleNumber);

  std::cout << "ADC Sample Rate: " << adcSamplRateMHz << " MHz" << std::endl;
  std::cout << "Time step: " << timeStepNs << " ns per sample" << std::endl;

  return true;
}

bool Digitizer1::EndpointConfigure()
{
  // Configure endpoint
  uint64_t epHandle;
  uint64_t epFolderHandle;
  bool status = true;
  auto err = CAEN_FELib_GetHandle(fHandle, "/endpoint/RAW", &epHandle);
  status &= CheckError(err);
  err = CAEN_FELib_GetParentHandle(epHandle, nullptr, &epFolderHandle);
  status &= CheckError(err);
  err = CAEN_FELib_SetValue(epFolderHandle, "/par/activeendpoint", "RAW");
  status &= CheckError(err);

  // Set data format
  nlohmann::json readDataJSON = GetReadDataFormatRAW();
  std::string readData = readDataJSON.dump();
  err = CAEN_FELib_GetHandle(fHandle, "/endpoint/RAW", &fReadDataHandle);
  status &= CheckError(err);
  err = CAEN_FELib_SetReadDataFormat(fReadDataHandle, readData.c_str());
  status &= CheckError(err);

  return status;
}

nlohmann::json Digitizer1::GetReadDataFormatRAW()
{
  nlohmann::json readDataJSON;
  nlohmann::json dataJSON;
  dataJSON["name"] = "DATA";
  dataJSON["type"] = "U8";
  dataJSON["dim"] = 1;
  readDataJSON.push_back(dataJSON);
  nlohmann::json sizeJSON;
  sizeJSON["name"] = "SIZE";
  sizeJSON["type"] = "SIZE_T";
  sizeJSON["dim"] = 0;
  readDataJSON.push_back(sizeJSON);

  return readDataJSON;
}

void Digitizer1::ReadDataThread()
{
  auto rawData = std::make_unique<RawData_t>(fMaxRawDataSize);
  while (fDataTakingFlag) {
    constexpr auto timeOut = 10;
    auto err = ReadDataWithLock(rawData, timeOut);

    if (err == CAEN_FELib_Success) {
      // Add data through Dig1Decoder converter ONLY
      if (fDig1Decoder) {
        auto dataType = fDig1Decoder->AddData(std::move(rawData));
        if (fDebugFlag) {
          std::cout << "Added data to Dig1Decoder, type: "
                    << static_cast<int>(dataType) << std::endl;
        }
      } else {
        std::cerr << "Error: Dig1Decoder not available in ReadDataThread"
                  << std::endl;
      }
      rawData = std::make_unique<RawData_t>(fMaxRawDataSize);
    } else if (err == CAEN_FELib_Timeout) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
}

int Digitizer1::ReadDataWithLock(std::unique_ptr<RawData_t> &rawData,
                                 int timeOut)
{
  int retCode = CAEN_FELib_Timeout;

  if (fReadDataMutex.try_lock()) {
    if (CAEN_FELib_HasData(fReadDataHandle, timeOut) == CAEN_FELib_Success) {
      retCode =
          CAEN_FELib_ReadData(fReadDataHandle, timeOut, rawData->data.data(),
                              &(rawData->size), &(rawData->nEvents));
    }
    fReadDataMutex.unlock();
  }
  return retCode;
}

// EventConversionThread removed - Dig1Decoder handles conversion internally

// ConvertRawToEventData removed - Dig1Decoder handles conversion internally

// ProcessDig1RawData removed - Dig1Decoder handles raw data processing internally

// DecodeDig1Event removed - Dig1Decoder handles event decoding internally

}  // namespace Digitizer
}  // namespace DELILA
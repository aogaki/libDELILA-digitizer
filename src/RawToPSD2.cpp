#include "RawToPSD2.hpp"

#include <algorithm>
#include <bitset>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <thread>

namespace DELILA
{
namespace Digitizer
{

// ============================================================================
// Constants
// ============================================================================

constexpr size_t kWordSize = 8;  // 64-bit word size in bytes

// Header bit masks and positions
constexpr uint64_t kHeaderTypeMask = 0xF;
constexpr int kHeaderTypeShift = 60;
constexpr uint64_t kHeaderTypeData = 0x2;

constexpr int kFailCheckShift = 56;
constexpr uint64_t kFailCheckMask = 0x1;

constexpr int kAggregateCounterShift = 32;
constexpr uint64_t kAggregateCounterMask = 0xFFFF;

constexpr uint64_t kTotalSizeMask = 0xFFFFFFFF;

// Event bit masks and positions
constexpr int kChannelShift = 56;
constexpr uint64_t kChannelMask = 0x7F;

constexpr uint64_t kTimeStampMask = 0xFFFFFFFFFFFF;

constexpr int kLastWordShift = 63;
constexpr int kWaveformFlagShift = 62;

constexpr int kFlagsLowPriorityShift = 50;
constexpr uint64_t kFlagsLowPriorityMask = 0x7FF;

constexpr int kFlagsHighPriorityShift = 42;
constexpr uint64_t kFlagsHighPriorityMask = 0xFF;

constexpr int kEnergyShortShift = 26;
constexpr uint64_t kEnergyShortMask = 0xFFFF;

constexpr int kFineTimeShift = 16;
constexpr uint64_t kFineTimeMask = 0x3FF;
constexpr double kFineTimeScale = 1024.0;

constexpr uint64_t kEnergyMask = 0xFFFF;

// Waveform header bit masks
constexpr int kWaveformCheck1Shift = 63;
constexpr int kWaveformCheck2Shift = 60;
constexpr uint64_t kWaveformCheck2Mask = 0x7;

constexpr int kTimeResolutionShift = 44;
constexpr uint64_t kTimeResolutionMask = 0x3;

constexpr int kTriggerThresholdShift = 28;
constexpr uint64_t kTriggerThresholdMask = 0xFFFF;

constexpr uint64_t kWaveformWordsMask = 0xFFF;

// Probe configuration bit positions
constexpr uint64_t kAnalogProbeMask = 0x3FFF;
constexpr uint64_t kDigitalProbeMask = 0x1;

// ============================================================================
// Constructor/Destructor
// ============================================================================

RawToPSD2::RawToPSD2(uint32_t nThreads)
{
  if (nThreads < 1) {
    nThreads = 1;
  }

  // Initialize data storage
  fPSD2DataVec = std::make_unique<std::vector<std::unique_ptr<PSD2Data_t>>>();
  fEventDataVec = std::make_unique<std::vector<std::unique_ptr<EventData>>>();

  // Start decode threads
  fDecodeFlag = true;
  fDecodeThreads.reserve(nThreads);
  for (uint32_t i = 0; i < nThreads; ++i) {
    fDecodeThreads.emplace_back(&RawToPSD2::DecodeThread, this);
  }
}

RawToPSD2::~RawToPSD2()
{
  // Signal threads to stop
  fDecodeFlag = false;

  // Wait for all threads to finish
  for (auto &thread : fDecodeThreads) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}

// ============================================================================
// Data Access
// ============================================================================

std::unique_ptr<std::vector<std::unique_ptr<PSD2Data_t>>> RawToPSD2::GetData()
{
  auto data = std::make_unique<std::vector<std::unique_ptr<PSD2Data_t>>>();
  {
    std::lock_guard<std::mutex> lock(fPSD2DataMutex);
    data->swap(*fPSD2DataVec);
    fPSD2DataVec->clear();
  }
  return data;
}

std::unique_ptr<std::vector<std::unique_ptr<EventData>>>
RawToPSD2::GetEventData()
{
  auto data = std::make_unique<std::vector<std::unique_ptr<EventData>>>();
  {
    std::lock_guard<std::mutex> lock(fEventDataMutex);
    data->swap(*fEventDataVec);
    fEventDataVec->clear();
  }
  return data;
}

// ============================================================================
// Threading and Data Processing
// ============================================================================

void RawToPSD2::DecodeThread()
{
  while (fDecodeFlag) {
    std::unique_ptr<RawData_t> rawData = nullptr;

    // Get data from queue
    {
      std::lock_guard<std::mutex> lock(fRawDataMutex);
      if (fRawDataQueue.empty()) {
        continue;
      }
      rawData = std::move(fRawDataQueue.front());
      fRawDataQueue.pop_front();
    }

    // Process data if available
    if (rawData) {
      DecodeData(std::move(rawData));
    }
  }
}

void RawToPSD2::DecodeData(std::unique_ptr<RawData_t> rawData)
{
  if (fDumpFlag) {
    DumpRawData(*rawData);
  }

  // Read and validate header
  uint64_t headerWord = 0;
  std::memcpy(&headerWord, rawData->data.data(), sizeof(uint64_t));

  if (!ValidateDataHeader(headerWord, rawData->size)) {
    return;
  }

  // Extract total size from header
  auto totalSize = static_cast<uint32_t>(headerWord & kTotalSizeMask);

  // Process all events in the data
  ProcessEventData(rawData->data.begin(), totalSize);
}

void RawToPSD2::DumpRawData(const RawData_t &rawData) const
{
  std::cout << "Data size: " << rawData.size << std::endl;
  for (size_t i = 0; i < rawData.size; i += kWordSize) {
    uint64_t buf = 0;
    std::memcpy(&buf, &rawData.data[i], kWordSize);
    std::cout << std::bitset<64>(buf) << std::endl;
  }
}

bool RawToPSD2::ValidateDataHeader(uint64_t headerWord, size_t dataSize)
{
  // Check header type
  auto headerType = (headerWord >> kHeaderTypeShift) & kHeaderTypeMask;
  if (headerType != kHeaderTypeData) {
    std::cerr << "Invalid header type: 0x" << std::hex << headerType << std::dec
              << std::endl;
    return false;
  }

  // Check fail bit
  auto failCheck = (headerWord >> kFailCheckShift) & kFailCheckMask;
  if (failCheck) {
    std::cerr << "Board fail bit set" << std::endl;
  }

  // Validate aggregate counter for single-threaded mode
  auto aggregateCounter =
      (headerWord >> kAggregateCounterShift) & kAggregateCounterMask;
  if (fDecodeThreads.size() == 1) {
    if (aggregateCounter != 0 && aggregateCounter != fLastCounter + 1) {
      std::cerr << "Aggregate counter discontinuity: " << fLastCounter << " -> "
                << aggregateCounter << std::endl;
    }
    fLastCounter = aggregateCounter;
  }

  // Validate total size
  auto totalSize = static_cast<uint32_t>(headerWord & kTotalSizeMask);
  if (totalSize * kWordSize != dataSize) {
    std::cerr << "Size mismatch: header=" << totalSize * kWordSize
              << " actual=" << dataSize << std::endl;
  }

  return true;
}

void RawToPSD2::ProcessEventData(
    const std::vector<uint8_t>::iterator &dataStart, uint32_t totalSize)
{
  if (fOutputFormat == OutputFormat::PSD2Data) {
    // Original PSD2Data output
    std::vector<std::unique_ptr<PSD2Data_t>> psd2DataVec;
    psd2DataVec.reserve(totalSize / 2);

    PSD2Data_t psd2Data;
    for (size_t wordIndex = 1; wordIndex < totalSize;) {
      DecodeEventPair(dataStart, wordIndex, psd2Data);
      psd2DataVec.emplace_back(std::make_unique<PSD2Data_t>(psd2Data));
    }

    // Store decoded data
    {
      std::lock_guard<std::mutex> lock(fPSD2DataMutex);
      fPSD2DataVec->insert(fPSD2DataVec->end(),
                           std::make_move_iterator(psd2DataVec.begin()),
                           std::make_move_iterator(psd2DataVec.end()));
    }
  } else {
    // EventData output
    std::vector<std::unique_ptr<EventData>> eventDataVec;
    eventDataVec.reserve(totalSize / 2);

    PSD2Data_t psd2Data;
    for (size_t wordIndex = 1; wordIndex < totalSize;) {
      DecodeEventPair(dataStart, wordIndex, psd2Data);
      auto eventData = ConvertPSD2ToEventData(psd2Data);
      if (eventData) {
        eventDataVec.push_back(std::move(eventData));
      }
    }

    // Sort EventData by timeStampNs in ascending order
    if (!eventDataVec.empty()) {
      std::sort(eventDataVec.begin(), eventDataVec.end(),
                [](const std::unique_ptr<EventData> &a,
                   const std::unique_ptr<EventData> &b) {
                  return a->GetTimeStampNs() < b->GetTimeStampNs();
                });
    }

    // Store converted data
    {
      std::lock_guard<std::mutex> lock(fEventDataMutex);
      fEventDataVec->insert(fEventDataVec->end(),
                            std::make_move_iterator(eventDataVec.begin()),
                            std::make_move_iterator(eventDataVec.end()));
    }
  }
}

void RawToPSD2::DecodeEventPair(const std::vector<uint8_t>::iterator &dataStart,
                                size_t &wordIndex, PSD2Data_t &psd2Data)
{
  // Read first word (channel and timestamp)
  uint64_t firstWord = 0;
  std::memcpy(&firstWord, &(*(dataStart + wordIndex * kWordSize)),
              sizeof(uint64_t));
  wordIndex++;

  // Read second word (flags and energy)
  uint64_t secondWord = 0;
  std::memcpy(&secondWord, &(*(dataStart + wordIndex * kWordSize)),
              sizeof(uint64_t));
  wordIndex++;

  // Decode first word
  DecodeFirstWord(firstWord, psd2Data);

  // Decode second word
  DecodeSecondWord(secondWord, psd2Data);

  // Check for waveform data
  bool hasWaveform = (secondWord >> kWaveformFlagShift) & 0x1;
  if (hasWaveform) {
    DecodeWaveformData(dataStart, wordIndex, psd2Data);
  } else {
    psd2Data.Resize(0);
  }

  psd2Data.timeResolution = fTimeStep;
}

void RawToPSD2::DecodeFirstWord(uint64_t word, PSD2Data_t &psd2Data) const
{
  // Extract channel
  psd2Data.channel = (word >> kChannelShift) & kChannelMask;

  // Extract raw timestamp
  uint64_t rawTimeStamp = word & kTimeStampMask;
  psd2Data.timeStamp = rawTimeStamp;

  if (fDumpFlag) {
    std::cout << "Channel: " << static_cast<int>(psd2Data.channel) << std::endl;
    std::cout << "Time stamp (raw): " << rawTimeStamp << std::endl;
  }
}

void RawToPSD2::DecodeSecondWord(uint64_t word, PSD2Data_t &psd2Data) const
{
  // Extract flags
  psd2Data.flagsLowPriority =
      (word >> kFlagsLowPriorityShift) & kFlagsLowPriorityMask;
  psd2Data.flagsHighPriority =
      (word >> kFlagsHighPriorityShift) & kFlagsHighPriorityMask;

  // Extract energies
  psd2Data.energyShort = (word >> kEnergyShortShift) & kEnergyShortMask;
  psd2Data.energy = word & kEnergyMask;

  // Calculate precise timestamp with fine time correction
  uint64_t fineTime = (word >> kFineTimeShift) & kFineTimeMask;
  double coarseTimeNs = static_cast<double>(psd2Data.timeStamp) * fTimeStep;
  double fineTimeNs =
      (static_cast<double>(fineTime) / kFineTimeScale) * fTimeStep;
  psd2Data.timeStampNs = coarseTimeNs + fineTimeNs;

  if (fDumpFlag) {
    std::cout << "Low priority flags: " << psd2Data.flagsLowPriority
              << std::endl;
    std::cout << "High priority flags: " << psd2Data.flagsHighPriority
              << std::endl;
    std::cout << "Short gate: " << psd2Data.energyShort << std::endl;
    std::cout << "Energy: " << psd2Data.energy << std::endl;
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Final timestamp: " << psd2Data.timeStampNs << " ns"
              << std::endl;
    std::cout << std::defaultfloat;
  }
}

void RawToPSD2::DecodeWaveformData(
    const std::vector<uint8_t>::iterator &dataStart, size_t &wordIndex,
    PSD2Data_t &psd2Data)
{
  // Read waveform header
  uint64_t waveformHeader = 0;
  std::memcpy(&waveformHeader, &(*(dataStart + wordIndex * kWordSize)),
              sizeof(uint64_t));
  wordIndex++;

  // Validate waveform header
  bool headerValid =
      ((waveformHeader >> kWaveformCheck1Shift) & 0x1) == 0x1 &&
      ((waveformHeader >> kWaveformCheck2Shift) & kWaveformCheck2Mask) == 0x0;
  if (!headerValid) {
    std::cerr << "Invalid waveform header" << std::endl;
  }

  // Decode waveform header
  DecodeWaveformHeader(waveformHeader, psd2Data);

  // Get waveform configuration
  WaveformConfig config = ExtractWaveformConfig(waveformHeader);

  // Read number of waveform words
  uint64_t nWordsWaveform = 0;
  std::memcpy(&nWordsWaveform, &(*(dataStart + wordIndex * kWordSize)),
              sizeof(uint64_t));
  wordIndex++;
  nWordsWaveform &= kWaveformWordsMask;

  // Resize data arrays (2 points per word)
  psd2Data.Resize(nWordsWaveform * 2);

  // Decode waveform data
  for (size_t j = 0; j < nWordsWaveform; j++) {
    uint64_t waveformWord = 0;
    std::memcpy(&waveformWord, &(*(dataStart + wordIndex * kWordSize)),
                sizeof(uint64_t));
    wordIndex++;

    // Each word contains two 32-bit samples
    uint32_t sample1 = static_cast<uint32_t>(waveformWord & 0xFFFFFFFF);
    uint32_t sample2 = static_cast<uint32_t>((waveformWord >> 32) & 0xFFFFFFFF);

    DecodeWaveformPoint(sample1, j * 2, config, psd2Data);
    DecodeWaveformPoint(sample2, j * 2 + 1, config, psd2Data);
  }
}

void RawToPSD2::DecodeWaveformHeader(uint64_t header,
                                     PSD2Data_t &psd2Data) const
{
  // Extract time resolution
  auto timeResolution = (header >> kTimeResolutionShift) & kTimeResolutionMask;
  psd2Data.downSampleFactor = 1 << timeResolution;  // 1, 2, 4, or 8

  // Extract trigger threshold
  psd2Data.triggerThr =
      (header >> kTriggerThresholdShift) & kTriggerThresholdMask;

  // Extract probe types
  psd2Data.digitalProbe4Type = static_cast<uint8_t>((header >> 24) & 0xF);
  psd2Data.digitalProbe3Type = static_cast<uint8_t>((header >> 20) & 0xF);
  psd2Data.digitalProbe2Type = static_cast<uint8_t>((header >> 16) & 0xF);
  psd2Data.digitalProbe1Type = static_cast<uint8_t>((header >> 12) & 0xF);
  psd2Data.analogProbe2Type = static_cast<uint8_t>((header >> 6) & 0x7);
  psd2Data.analogProbe1Type = static_cast<uint8_t>(header & 0x7);
}

RawToPSD2::WaveformConfig RawToPSD2::ExtractWaveformConfig(
    uint64_t header) const
{
  WaveformConfig config;

  // Analog probe 1 configuration
  config.ap1IsSigned = (header >> 3) & 0x1;
  config.ap1MulFactor = GetMultiplicationFactor((header >> 4) & 0x3);

  // Analog probe 2 configuration
  config.ap2IsSigned = (header >> 9) & 0x1;
  config.ap2MulFactor = GetMultiplicationFactor((header >> 10) & 0x3);

  return config;
}

uint32_t RawToPSD2::GetMultiplicationFactor(uint32_t encodedValue) const
{
  switch (encodedValue) {
    case 0:
      return 1;
    case 1:
      return 4;
    case 2:
      return 8;
    case 3:
      return 16;
    default:
      return 1;
  }
}

void RawToPSD2::DecodeWaveformPoint(uint32_t point, size_t dataIndex,
                                    const WaveformConfig &config,
                                    PSD2Data_t &psd2Data) const
{
  // Decode analog probes
  uint32_t analog1Raw = point & kAnalogProbeMask;
  uint32_t analog2Raw = (point >> 16) & kAnalogProbeMask;

  if (config.ap1IsSigned) {
    // Sign extend from 14-bit to 32-bit
    if (analog1Raw & 0x2000) analog1Raw |= 0xFFFFC000;
    psd2Data.analogProbe1[dataIndex] =
        static_cast<int32_t>(analog1Raw) * config.ap1MulFactor;
  } else {
    psd2Data.analogProbe1[dataIndex] = analog1Raw * config.ap1MulFactor;
  }

  if (config.ap2IsSigned) {
    // Sign extend from 14-bit to 32-bit
    if (analog2Raw & 0x2000) analog2Raw |= 0xFFFFC000;
    psd2Data.analogProbe2[dataIndex] =
        static_cast<int32_t>(analog2Raw) * config.ap2MulFactor;
  } else {
    psd2Data.analogProbe2[dataIndex] = analog2Raw * config.ap2MulFactor;
  }

  // Decode digital probes
  psd2Data.digitalProbe1[dataIndex] = (point >> 14) & kDigitalProbeMask;
  psd2Data.digitalProbe2[dataIndex] = (point >> 15) & kDigitalProbeMask;
  psd2Data.digitalProbe3[dataIndex] = (point >> 30) & kDigitalProbeMask;
  psd2Data.digitalProbe4[dataIndex] = (point >> 31) & kDigitalProbeMask;
}

// ============================================================================
// Data Input and Classification
// ============================================================================

DataType RawToPSD2::AddData(std::unique_ptr<RawData_t> rawData)
{
  constexpr uint32_t oneWordSize = kWordSize;
  if (rawData->size % oneWordSize != 0) {
    std::cerr << "Data size is not a multiple of " << oneWordSize << " Bytes"
              << std::endl;
    return DataType::Unknown;
  }

  // change big endian to little endian
  for (size_t i = 0; i < rawData->size; i += oneWordSize) {
    std::reverse(rawData->data.begin() + i,
                 rawData->data.begin() + i + oneWordSize);
  }

  auto dataType = CheckDataType(rawData);
  if (dataType == DataType::Event) {
    if (fIsRunning) {
      std::lock_guard<std::mutex> lock(fRawDataMutex);
      fRawDataQueue.push_back(std::move(rawData));
    }
  } else if (dataType == DataType::Start) {
    fIsRunning = true;
  } else if (dataType == DataType::Stop) {
    fIsRunning = false;
  } else if (dataType == DataType::Unknown) {
    std::cout << "Unknown data type" << std::endl;
    exit(1);
  }

  return dataType;
}

// ============================================================================
// Data Type Detection
// ============================================================================

DataType RawToPSD2::CheckDataType(std::unique_ptr<RawData_t> &rawData)
{
  if (rawData->size < 3 * kWordSize) {
    return DataType::Unknown;
  } else if (rawData->size == 3 * kWordSize) {
    if (CheckStop(rawData)) {
      return DataType::Stop;
    }
  } else if (rawData->size == 4 * kWordSize) {
    if (CheckStart(rawData)) {
      return DataType::Start;
    }
  }

  return DataType::Event;
}

// ============================================================================
// Specific Data Type Checkers
// ============================================================================

bool RawToPSD2::CheckStop(std::unique_ptr<RawData_t> &rawData)
{
  uint64_t buf = 0;
  // The first word bit[60:63] = 0x3
  // The first word bit[56:59] = 0x2
  std::memcpy(&buf, &(*(rawData->data.begin())), sizeof(uint64_t));
  auto firstCondition =
      ((buf >> 60) & 0xF) == 0x3 && ((buf >> 56) & 0xF) == 0x2;

  // The second word bit[56:63] = 0x0
  std::memcpy(&buf, &(*(rawData->data.begin() + 8)), sizeof(uint64_t));
  auto secondCondition = ((buf >> 56) & 0xF) == 0x0;

  // The third word bit[56:63] = 0x1
  std::memcpy(&buf, &(*(rawData->data.begin() + 16)), sizeof(uint64_t));
  auto thirdCondition = ((buf >> 56) & 0xF) == 0x1;

  if (firstCondition && secondCondition && thirdCondition) {
    // The third word bit[0:31] = dead time
    // auto deadTime = static_cast<uint32_t>(buf & 0xFFFFFFFF);
    // std::cout << "Dead time: " << deadTime * 8 << " ns" << std::endl;
    return true;
  }

  return false;
}

bool RawToPSD2::CheckStart(std::unique_ptr<RawData_t> &rawData)
{
  uint64_t buf = 0;
  // The first word bit[60:63] = 0x3
  // The first word bit[56:59] = 0x0
  std::memcpy(&buf, &(*(rawData->data.begin())), sizeof(uint64_t));
  auto firstCondition =
      ((buf >> 60) & 0xF) == 0x3 && ((buf >> 56) & 0xF) == 0x0;

  // The second word bit[56:63] = 0x2
  std::memcpy(&buf, &(*(rawData->data.begin() + 8)), sizeof(uint64_t));
  auto secondCondition = ((buf >> 56) & 0xF) == 0x2;

  // The third word bit[56:63] = 0x1
  std::memcpy(&buf, &(*(rawData->data.begin() + 16)), sizeof(uint64_t));
  auto thirdCondition = ((buf >> 56) & 0xF) == 0x1;

  // The fourth word bit[56:63] = 0x1
  std::memcpy(&buf, &(*(rawData->data.begin() + 24)), sizeof(uint64_t));
  auto fourthCondition = ((buf >> 56) & 0xF) == 0x1;

  if (firstCondition && secondCondition && thirdCondition && fourthCondition) {
    return true;
  }

  return false;
}

// ============================================================================
// EventData Conversion
// ============================================================================

std::unique_ptr<EventData> RawToPSD2::ConvertPSD2ToEventData(
    const PSD2Data_t &psd2Data) const
{
  auto eventData = std::make_unique<EventData>(psd2Data.waveformSize);

  // Copy basic timing and energy information
  eventData->SetTimeStampNs(psd2Data.timeStampNs);
  eventData->SetEnergy(psd2Data.energy);
  eventData->SetEnergyShort(psd2Data.energyShort);

  // Copy channel and module information
  eventData->SetChannel(psd2Data.channel);
  eventData->SetModule(fModuleNumber);

  // Copy time resolution and sampling information
  eventData->SetTimeResolution(psd2Data.timeResolution);
  eventData->SetDownSampleFactor(psd2Data.downSampleFactor);

  // Copy probe type information
  eventData->SetAnalogProbe1Type(psd2Data.analogProbe1Type);
  eventData->SetAnalogProbe2Type(psd2Data.analogProbe2Type);
  eventData->SetDigitalProbe1Type(psd2Data.digitalProbe1Type);
  eventData->SetDigitalProbe2Type(psd2Data.digitalProbe2Type);
  eventData->SetDigitalProbe3Type(psd2Data.digitalProbe3Type);
  eventData->SetDigitalProbe4Type(psd2Data.digitalProbe4Type);

  // Move waveform data efficiently (avoid copies)
  if (psd2Data.waveformSize > 0) {
    eventData->SetAnalogProbe1(psd2Data.analogProbe1);
    eventData->SetAnalogProbe2(psd2Data.analogProbe2);
    eventData->SetDigitalProbe1(psd2Data.digitalProbe1);
    eventData->SetDigitalProbe2(psd2Data.digitalProbe2);
    eventData->SetDigitalProbe3(psd2Data.digitalProbe3);
    eventData->SetDigitalProbe4(psd2Data.digitalProbe4);
  }

  return eventData;
}

}  // namespace Digitizer
}  // namespace DELILA

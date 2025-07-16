#include "Dig1Decoder.hpp"

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
// Constants for PSD1 Format
// ============================================================================

constexpr size_t kWordSize =
    4;  // 32-bit word size in bytes (PSD1 uses 32-bit words)

// Board header bit masks and positions
constexpr uint32_t kBoardHeaderTypeMask = 0xF;
constexpr int kBoardHeaderTypeShift = 28;
constexpr uint32_t kBoardHeaderTypeData = 0xA;  // 0b1010

constexpr uint32_t kBoardAggregateSizeMask = 0x0FFFFFFF;  // [0:27]

constexpr int kDualChannelMaskShift = 0;
constexpr uint32_t kDualChannelMaskMask = 0xFF;  // [0:7]

constexpr int kLVDSPatternShift = 8;
constexpr uint32_t kLVDSPatternMask = 0x7FFF;  // [8:22]

constexpr int kBoardFailShift = 26;
constexpr uint32_t kBoardFailMask = 0x1;

constexpr int kBoardIdShift = 27;
constexpr uint32_t kBoardIdMask = 0x1F;  // [27:31]

constexpr uint32_t kBoardCounterMask = 0x7FFFFF;  // [0:22]

// Dual channel header bit masks
constexpr uint32_t kDualChannelSizeMask = 0x3FFFFF;  // [0:21]
constexpr int kDualChannelHeaderShift = 31;

constexpr uint32_t kNumSamplesWaveMask = 0xFFFF;  // [0:15]

constexpr int kDigitalProbe1Shift = 16;
constexpr uint32_t kDigitalProbe1Mask = 0x7;  // [16:18]

constexpr int kDigitalProbe2Shift = 19;
constexpr uint32_t kDigitalProbe2Mask = 0x7;  // [19:21]

constexpr int kAnalogProbeShift = 22;
constexpr uint32_t kAnalogProbeMask = 0x3;  // [22:23]

constexpr int kExtraOptionShift = 24;
constexpr uint32_t kExtraOptionMask = 0x7;  // [24:26]

constexpr int kSamplesEnabledShift = 27;  // ES
constexpr int kExtrasEnabledShift = 28;   // EE
constexpr int kTimeEnabledShift = 29;     // ET
constexpr int kChargeEnabledShift = 30;   // EQ
constexpr int kDualTraceShift = 31;       // DT

// Event data bit masks
constexpr uint32_t kTriggerTimeTagMask = 0x7FFFFFFF;  // [0:30]
constexpr int kChannelFlagShift = 31;                 // [31] 0=even, 1=odd

// Waveform data bit masks (16-bit samples, 2 per 32-bit word)
constexpr uint32_t kAnalogSampleMask = 0x3FFF;  // [0:13] or [16:29]
constexpr int kDigitalProbe1WaveShift = 14;     // [14] or [30]
constexpr int kDigitalProbe2WaveShift = 15;     // [15] or [31]
constexpr int kSecondSampleShift = 16;  // Second sample starts at bit 16

// Extras word bit masks (for extra option 0b010)
constexpr uint32_t kFineTimeStampMask = 0x3FF;  // [0:9]
constexpr int kFlagsShift = 10;
constexpr uint32_t kFlagsMask = 0x3F;  // [10:15]
constexpr int kExtendedTimeShift = 16;
constexpr uint32_t kExtendedTimeMask = 0xFFFF;  // [16:31]

// Charge word bit masks
constexpr uint32_t kChargeShortMask = 0x7FFF;  // [0:14]
constexpr int kPileupFlagShift = 15;
constexpr int kChargeLongShift = 16;
constexpr uint32_t kChargeLongMask = 0xFFFF;  // [16:31]

// ============================================================================
// Constructor/Destructor
// ============================================================================

Dig1Decoder::Dig1Decoder(uint32_t nThreads)
{
  if (nThreads < 1) {
    nThreads = 1;
  }

  // Initialize data storage
  fEventDataVec = std::make_unique<std::vector<std::unique_ptr<EventData>>>();

  // Start in running state (dig1 doesn't have explicit start/stop like dig2)
  fIsRunning = true;

  // Start decode threads
  fDecodeFlag = true;
  fDecodeThreads.reserve(nThreads);
  for (uint32_t i = 0; i < nThreads; ++i) {
    fDecodeThreads.emplace_back(&Dig1Decoder::DecodeThread, this);
  }
}

Dig1Decoder::~Dig1Decoder()
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

std::unique_ptr<std::vector<std::unique_ptr<EventData>>>
Dig1Decoder::GetEventData()
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

void Dig1Decoder::DecodeThread()
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

void Dig1Decoder::DecodeData(std::unique_ptr<RawData_t> rawData)
{
  if (fDumpFlag) {
    DumpRawData(*rawData);
  }

  // Read and validate board header (first word)
  if (rawData->size <
      4 * kWordSize) {  // Need at least 4 words for board header
    std::cerr << "PSD1 data too small for board header" << std::endl;
    return;
  }

  uint32_t firstWord = 0;
  std::memcpy(&firstWord, rawData->data.data(), sizeof(uint32_t));

  if (!ValidateDataHeader(firstWord, rawData->size)) {
    return;
  }

  // Process all events in the data (multiple board aggregate blocks)
  // Pass the total data size in 32-bit words
  uint32_t totalDataSizeWords = rawData->size / kWordSize;
  ProcessEventData(rawData->data.begin(), totalDataSizeWords);
}

void Dig1Decoder::DumpRawData(const RawData_t &rawData) const
{
  std::cout << "PSD1 Data size: " << rawData.size << std::endl;
  for (size_t i = 0; i < rawData.size; i += kWordSize) {
    uint32_t buf = 0;
    std::memcpy(&buf, &rawData.data[i], kWordSize);
    std::cout << std::bitset<32>(buf) << std::endl;
  }
}

bool Dig1Decoder::ValidateDataHeader(uint32_t headerWord, size_t dataSize)
{
  // Check header type (should be 0b1010 for PSD1)
  auto headerType =
      (headerWord >> kBoardHeaderTypeShift) & kBoardHeaderTypeMask;
  if (headerType != kBoardHeaderTypeData) {
    std::cerr << "Invalid PSD1 header type: 0x" << std::hex << headerType
              << std::dec << std::endl;
    return false;
  }

  // Note: totalSize is the size of this board aggregate block, not the entire data
  // The entire data may contain multiple board aggregate blocks
  auto totalSize = headerWord & kBoardAggregateSizeMask;
  if (fDumpFlag) {
    std::cout << "Board aggregate size: " << totalSize * kWordSize << " bytes"
              << std::endl;
  }

  return true;
}

void Dig1Decoder::ProcessEventData(
    const std::vector<uint8_t>::iterator &dataStart, uint32_t totalDataSize)
{
  // Direct EventData output
  std::vector<std::unique_ptr<EventData>> eventDataVec;
  eventDataVec.reserve(totalDataSize / 8);  // Rough estimate

  size_t wordIndex = 0;

  // Process multiple Board Aggregate Blocks in the data
  while (wordIndex < totalDataSize) {
    // Decode board header (4 words)
    BoardHeaderInfo boardInfo;
    if (!DecodeBoardHeader(dataStart, wordIndex, boardInfo)) {
      std::cerr << "Failed to decode board header at word " << wordIndex
                << std::endl;
      break;
    }

    if (fDumpFlag) {
      std::cout << "Processing Board Aggregate Block: size="
                << boardInfo.aggregateSize << " words, mask=0x" << std::hex
                << static_cast<int>(boardInfo.dualChannelMask) << std::dec
                << std::endl;
    }

    // Calculate the end of this board aggregate block
    size_t boardEndIndex =
        wordIndex - 4 +
        boardInfo.aggregateSize;  // -4 because we already advanced past header

    // Safety check: ensure board end doesn't exceed total data
    if (boardEndIndex > totalDataSize) {
      std::cerr << "Board aggregate block extends beyond data: "
                << boardEndIndex << " > " << totalDataSize << std::endl;
      boardEndIndex = totalDataSize;
    }

    // Process each dual channel block in this board aggregate
    uint8_t activeMask = boardInfo.dualChannelMask;
    for (int pair = 0; pair < 8; ++pair) {
      if (!(activeMask & (1 << pair))) {
        continue;  // This channel pair is not active
      }

      if (wordIndex >= boardEndIndex) {
        std::cerr << "Unexpected end of board aggregate block" << std::endl;
        break;
      }

      // Decode dual channel header (2 words)
      DualChannelInfo dualChInfo;
      if (!DecodeDualChannelHeader(dataStart, wordIndex, dualChInfo)) {
        std::cerr << "Failed to decode dual channel header for pair " << pair
                  << std::endl;
        break;
      }

      // Decode events in this dual channel block
      size_t channelEndIndex =
          wordIndex - 2 +
          dualChInfo
              .aggregateSize;  // -2 because we already advanced past header

      // Safety check: ensure channel end doesn't exceed board end
      if (channelEndIndex > boardEndIndex) {
        std::cerr << "Channel aggregate block extends beyond board: "
                  << channelEndIndex << " > " << boardEndIndex << std::endl;
        channelEndIndex = boardEndIndex;
      }

      if (fDumpFlag) {
        std::cout << "Processing Channel Pair " << pair
                  << ": size=" << dualChInfo.aggregateSize
                  << " words, samples=" << dualChInfo.numSamplesWave
                  << std::endl;
      }

      while (wordIndex < channelEndIndex && wordIndex < boardEndIndex) {
        auto eventData = DecodeEventDirect(dataStart, wordIndex, dualChInfo);
        if (eventData) {
          // Set the channel pair offset
          eventData->channel += pair * 2;  // Each pair handles 2 channels
          eventDataVec.push_back(std::move(eventData));
        }
      }
    }

    // Move to the next board aggregate block
    wordIndex = boardEndIndex;
  }

  // Sort EventData by timeStampNs in ascending order
  if (!eventDataVec.empty()) {
    std::sort(eventDataVec.begin(), eventDataVec.end(),
              [](const std::unique_ptr<EventData> &a,
                 const std::unique_ptr<EventData> &b) {
                return a->timeStampNs < b->timeStampNs;
              });
  }

  if (fDumpFlag) {
    std::cout << "PSD1 ProcessEventData: decoded " << eventDataVec.size()
              << " events from " << totalDataSize << " words" << std::endl;
  }

  // Store converted data
  {
    std::lock_guard<std::mutex> lock(fEventDataMutex);
    fEventDataVec->insert(fEventDataVec->end(),
                          std::make_move_iterator(eventDataVec.begin()),
                          std::make_move_iterator(eventDataVec.end()));
  }
}

// ============================================================================
// PSD1 Format Decoding Methods
// ============================================================================

bool Dig1Decoder::DecodeBoardHeader(
    const std::vector<uint8_t>::iterator &dataStart, size_t &wordIndex,
    BoardHeaderInfo &boardInfo)
{
  // Note: Size validation should be done by caller
  // This method assumes sufficient data is available

  // First word: aggregate size and header type
  uint32_t word1 = 0;
  std::memcpy(&word1, &(*(dataStart + wordIndex * kWordSize)),
              sizeof(uint32_t));
  wordIndex++;

  boardInfo.aggregateSize = word1 & kBoardAggregateSizeMask;

  // Second word: dual channel mask, LVDS pattern, board fail, board ID
  uint32_t word2 = 0;
  std::memcpy(&word2, &(*(dataStart + wordIndex * kWordSize)),
              sizeof(uint32_t));
  wordIndex++;

  boardInfo.dualChannelMask =
      (word2 >> kDualChannelMaskShift) & kDualChannelMaskMask;
  boardInfo.lvdsPattern = (word2 >> kLVDSPatternShift) & kLVDSPatternMask;
  boardInfo.boardFailFlag = (word2 >> kBoardFailShift) & kBoardFailMask;
  boardInfo.boardId = (word2 >> kBoardIdShift) & kBoardIdMask;

  // Third word: board aggregate counter
  uint32_t word3 = 0;
  std::memcpy(&word3, &(*(dataStart + wordIndex * kWordSize)),
              sizeof(uint32_t));
  wordIndex++;

  boardInfo.aggregateCounter = word3 & kBoardCounterMask;

  // Fourth word: board time tag
  uint32_t word4 = 0;
  std::memcpy(&word4, &(*(dataStart + wordIndex * kWordSize)),
              sizeof(uint32_t));
  wordIndex++;

  boardInfo.boardTimeTag = word4;

  if (fDumpFlag) {
    std::cout << "Board Header:" << std::endl;
    std::cout << "  Aggregate Size: " << boardInfo.aggregateSize << std::endl;
    std::cout << "  Dual Channel Mask: 0x" << std::hex
              << static_cast<int>(boardInfo.dualChannelMask) << std::dec
              << std::endl;
    std::cout << "  Board ID: " << static_cast<int>(boardInfo.boardId)
              << std::endl;
  }

  return true;
}

bool Dig1Decoder::DecodeDualChannelHeader(
    const std::vector<uint8_t>::iterator &dataStart, size_t &wordIndex,
    DualChannelInfo &dualChInfo)
{
  // First word: dual channel aggregate size
  uint32_t word1 = 0;
  std::memcpy(&word1, &(*(dataStart + wordIndex * kWordSize)),
              sizeof(uint32_t));
  wordIndex++;

  dualChInfo.aggregateSize = word1 & kDualChannelSizeMask;

  // Validate dual channel header bit
  bool isValidHeader = (word1 >> kDualChannelHeaderShift) & 0x1;
  if (!isValidHeader) {
    std::cerr << "Invalid dual channel header" << std::endl;
    return false;
  }

  // Second word: configuration flags and probe settings
  uint32_t word2 = 0;
  std::memcpy(&word2, &(*(dataStart + wordIndex * kWordSize)),
              sizeof(uint32_t));
  wordIndex++;

  dualChInfo.numSamplesWave = word2 & kNumSamplesWaveMask;
  dualChInfo.digitalProbe1 =
      (word2 >> kDigitalProbe1Shift) & kDigitalProbe1Mask;
  dualChInfo.digitalProbe2 =
      (word2 >> kDigitalProbe2Shift) & kDigitalProbe2Mask;
  dualChInfo.analogProbe = (word2 >> kAnalogProbeShift) & kAnalogProbeMask;
  dualChInfo.extraOption = (word2 >> kExtraOptionShift) & kExtraOptionMask;
  dualChInfo.samplesEnabled = (word2 >> kSamplesEnabledShift) & 0x1;
  dualChInfo.extrasEnabled = (word2 >> kExtrasEnabledShift) & 0x1;
  dualChInfo.timeEnabled = (word2 >> kTimeEnabledShift) & 0x1;
  dualChInfo.chargeEnabled = (word2 >> kChargeEnabledShift) & 0x1;
  dualChInfo.dualTraceEnabled = (word2 >> kDualTraceShift) & 0x1;

  if (fDumpFlag) {
    std::cout << "Dual Channel Header:" << std::endl;
    std::cout << "  Aggregate Size: " << dualChInfo.aggregateSize << std::endl;
    std::cout << "  Samples/8: " << dualChInfo.numSamplesWave << std::endl;
    std::cout << "  Samples Enabled: " << dualChInfo.samplesEnabled
              << std::endl;
  }

  return true;
}

std::unique_ptr<EventData> Dig1Decoder::DecodeEventDirect(
    const std::vector<uint8_t>::iterator &dataStart, size_t &wordIndex,
    const DualChannelInfo &dualChInfo)
{
  // Read trigger time tag (first word of event)
  uint32_t timeTagWord = 0;
  std::memcpy(&timeTagWord, &(*(dataStart + wordIndex * kWordSize)),
              sizeof(uint32_t));
  wordIndex++;

  uint32_t triggerTimeTag = timeTagWord & kTriggerTimeTagMask;
  bool isOddChannel = (timeTagWord >> kChannelFlagShift) & 0x1;

  // Calculate waveform size (numSamplesWave is samples/8, we need actual samples)
  size_t waveformSize = dualChInfo.numSamplesWave * 8;

  // Create EventData with proper size
  auto eventData = std::make_unique<EventData>(waveformSize);

  // Set basic event information
  eventData->channel =
      isOddChannel ? 1 : 0;  // Will be adjusted by caller for channel pair
  eventData->module = fModuleNumber;
  eventData->timeResolution = fTimeStep;

  // Store probe type information
  eventData->digitalProbe1Type = dualChInfo.digitalProbe1;
  eventData->digitalProbe2Type = dualChInfo.digitalProbe2;
  eventData->analogProbe1Type = dualChInfo.analogProbe;
  eventData->analogProbe2Type =
      dualChInfo.dualTraceEnabled ? dualChInfo.analogProbe : 0;

  // Decode waveform data if present
  if (dualChInfo.samplesEnabled && waveformSize > 0) {
    DecodeWaveform(dataStart, wordIndex, dualChInfo, *eventData);
  }

  // Decode extras word if present and calculate final timestamp
  if (dualChInfo.extrasEnabled) {
    uint32_t extrasWord = 0;
    std::memcpy(&extrasWord, &(*(dataStart + wordIndex * kWordSize)),
                sizeof(uint32_t));
    wordIndex++;

    // Extract extended timestamp from extras word
    uint16_t extendedTime = DecodeExtrasWord(extrasWord, *eventData);

    // Calculate final timestamp using extended timestamp (47-bit total)
    // Extended timestamp goes in upper 16 bits, trigger time tag in lower 31 bits
    uint64_t extendedTimestamp = static_cast<uint64_t>(extendedTime) << 31;
    uint64_t combinedTimeTag =
        static_cast<uint64_t>(triggerTimeTag) + extendedTimestamp;

    // Apply sample time multiplication
    uint64_t finalTimestamp = combinedTimeTag * fTimeStep;

    // Add fine time correction (10 bits from extras word)
    uint16_t fineTimeStamp = extrasWord & kFineTimeStampMask;
    double fineTimeNs =
        (static_cast<double>(fineTimeStamp) / 1024.0) * fTimeStep;

    eventData->timeStampNs = static_cast<double>(finalTimestamp) + fineTimeNs;

    if (fDumpFlag) {
      std::cout << "Timestamp calc: trigger=" << triggerTimeTag
                << ", extended=" << extendedTime
                << ", combined=" << combinedTimeTag
                << ", final=" << eventData->timeStampNs << " ns" << std::endl;
    }
  } else {
    // No extended timestamp available - use only trigger time tag
    eventData->timeStampNs = static_cast<double>(triggerTimeTag) * fTimeStep;
  }

  // Decode charge word if present
  if (dualChInfo.chargeEnabled) {
    uint32_t chargeWord = 0;
    std::memcpy(&chargeWord, &(*(dataStart + wordIndex * kWordSize)),
                sizeof(uint32_t));
    wordIndex++;
    DecodeChargeWord(chargeWord, *eventData);
  }

  return eventData;
}

void Dig1Decoder::DecodeWaveform(
    const std::vector<uint8_t>::iterator &dataStart, size_t &wordIndex,
    const DualChannelInfo &dualChInfo, EventData &eventData)
{
  size_t numWords =
      dualChInfo.numSamplesWave *
      2;  // numSamplesWave is samples/8, 2 samples per word, so *2 words total

  for (size_t i = 0; i < numWords; ++i) {
    uint32_t waveformWord = 0;
    std::memcpy(&waveformWord, &(*(dataStart + wordIndex * kWordSize)),
                sizeof(uint32_t));
    wordIndex++;

    // Each word contains two 16-bit samples
    uint16_t sample1 = waveformWord & 0xFFFF;
    uint16_t sample2 = (waveformWord >> 16) & 0xFFFF;

    size_t sampleIndex1 = i * 2;
    size_t sampleIndex2 = i * 2 + 1;

    if (sampleIndex1 < eventData.waveformSize) {
      // Extract analog data (14 bits)
      eventData.analogProbe1[sampleIndex1] = sample1 & kAnalogSampleMask;

      // Extract digital probes
      eventData.digitalProbe1[sampleIndex1] =
          (sample1 >> kDigitalProbe1WaveShift) & 0x1;
      eventData.digitalProbe2[sampleIndex1] =
          (sample1 >> kDigitalProbe2WaveShift) & 0x1;

      // For dual trace mode, second analog probe data comes from odd samples
      if (dualChInfo.dualTraceEnabled && sampleIndex1 > 0) {
        // Previous odd sample contains second analog probe data
        eventData.analogProbe2[sampleIndex1] =
            eventData.analogProbe1[sampleIndex1 - 1];
      }
    }

    if (sampleIndex2 < eventData.waveformSize) {
      // Extract analog data (14 bits)
      eventData.analogProbe1[sampleIndex2] = sample2 & kAnalogSampleMask;

      // Extract digital probes
      eventData.digitalProbe1[sampleIndex2] =
          (sample2 >> kDigitalProbe1WaveShift) & 0x1;
      eventData.digitalProbe2[sampleIndex2] =
          (sample2 >> kDigitalProbe2WaveShift) & 0x1;

      // For dual trace mode, odd samples contain second analog probe data
      if (dualChInfo.dualTraceEnabled) {
        eventData.analogProbe2[sampleIndex2] =
            eventData.analogProbe1[sampleIndex2];
        // Restore the correct first analog probe data (from even sample)
        eventData.analogProbe1[sampleIndex2] =
            eventData.analogProbe1[sampleIndex2 - 1];
      }
    }
  }
}

uint16_t Dig1Decoder::DecodeExtrasWord(uint32_t extrasWord,
                                       EventData &eventData)
{
  // For extra option 0b010 (most common case)
  uint16_t fineTimeStamp = extrasWord & kFineTimeStampMask;
  uint8_t flags = (extrasWord >> kFlagsShift) & kFlagsMask;
  uint16_t extendedTime =
      (extrasWord >> kExtendedTimeShift) & kExtendedTimeMask;

  // Store flags in EventData.flags field
  eventData.flags = 0;
  if (flags & 0x20)
    eventData.flags |=
        EventData::FLAG_TRIGGER_LOST;  // bit[15] = bit[5] in 6-bit flags
  if (flags & 0x10)
    eventData.flags |=
        EventData::FLAG_OVER_RANGE;  // bit[14] = bit[4] in 6-bit flags
  if (flags & 0x08)
    eventData.flags |=
        EventData::FLAG_1024_TRIGGER;  // bit[13] = bit[3] in 6-bit flags
  if (flags & 0x04)
    eventData.flags |=
        EventData::FLAG_N_LOST_TRIGGER;  // bit[12] = bit[2] in 6-bit flags

  // Store fine time correction for later use (don't apply to timestamp here)
  // The final timestamp calculation will be done in DecodeEventDirect

  if (fDumpFlag) {
    std::cout << "Extras - Fine Time: " << fineTimeStamp << ", Flags: 0x"
              << std::hex << static_cast<int>(flags) << std::dec
              << ", Extended Time: " << extendedTime << std::endl;
  }

  // Return extended timestamp for use in final timestamp calculation
  return extendedTime;
}

void Dig1Decoder::DecodeChargeWord(uint32_t chargeWord, EventData &eventData)
{
  eventData.energyShort = chargeWord & kChargeShortMask;
  bool pileupFlag = (chargeWord >> kPileupFlagShift) & 0x1;
  eventData.energy = (chargeWord >> kChargeLongShift) & kChargeLongMask;

  // Store pileup flag
  if (pileupFlag) {
    eventData.flags |= EventData::FLAG_PILEUP;
  }

  if (fDumpFlag) {
    std::cout << "Charge - Short: " << eventData.energyShort
              << ", Long: " << eventData.energy << ", Pileup: " << pileupFlag
              << std::endl;
  }
}

// ============================================================================
// Data Input and Classification
// ============================================================================

DataType Dig1Decoder::AddData(std::unique_ptr<RawData_t> rawData)
{
  constexpr uint32_t oneWordSize = kWordSize;
  if (rawData->size % oneWordSize != 0) {
    std::cerr << "PSD1 data size is not a multiple of " << oneWordSize
              << " Bytes" << std::endl;
    return DataType::Unknown;
  }

  // PSD1 uses little endian, no byte swapping needed for most systems
  // (Unlike PSD2 which needs big endian to little endian conversion)

  auto dataType = CheckDataType(rawData);

  if (fDumpFlag) {
    std::cout << "PSD1 AddData: size=" << rawData->size
              << ", type=" << static_cast<int>(dataType) << std::endl;
  }
  if (dataType == DataType::Event) {
    if (fIsRunning) {
      std::lock_guard<std::mutex> lock(fRawDataMutex);
      fRawDataQueue.push_back(std::move(rawData));
      if (fDumpFlag) {
        std::cout << "Added PSD1 event data to queue, queue size: "
                  << fRawDataQueue.size() << std::endl;
      }
    } else {
      if (fDumpFlag) {
        std::cout << "PSD1 decoder not running, discarding event data"
                  << std::endl;
      }
    }
  } else if (dataType == DataType::Start) {
    fIsRunning = true;
    if (fDumpFlag) {
      std::cout << "PSD1 decoder started" << std::endl;
    }
  } else if (dataType == DataType::Stop) {
    fIsRunning = false;
    if (fDumpFlag) {
      std::cout << "PSD1 decoder stopped" << std::endl;
    }
  } else if (dataType == DataType::Unknown) {
    if (fDumpFlag) {
      std::cout << "Unknown PSD1 data type, discarding" << std::endl;
    }
    return DataType::Unknown;
  }

  return dataType;
}

// ============================================================================
// Data Type Detection (Simplified for PSD1)
// ============================================================================

DataType Dig1Decoder::CheckDataType(std::unique_ptr<RawData_t> &rawData)
{
  if (rawData->size < 4 * kWordSize) {
    if (fDumpFlag) {
      std::cout << "PSD1 data too small: " << rawData->size << " bytes"
                << std::endl;
    }
    return DataType::Unknown;
  }

  // For PSD1, we primarily check the first word header type
  uint32_t firstWord = 0;
  std::memcpy(&firstWord, rawData->data.data(), sizeof(uint32_t));

  auto headerType = (firstWord >> kBoardHeaderTypeShift) & kBoardHeaderTypeMask;

  if (fDumpFlag) {
    std::cout << "PSD1 first word: 0x" << std::hex << firstWord << std::dec
              << ", header type: 0x" << std::hex << headerType << std::dec
              << std::endl;
  }

  if (headerType == kBoardHeaderTypeData) {
    return DataType::Event;
  }

  // For now, treat any data as potentially valid events
  // This is more permissive than the strict header check
  if (rawData->size >= 16 * kWordSize) {  // Minimum size for a meaningful event
    if (fDumpFlag) {
      std::cout << "Treating as Event despite header type mismatch"
                << std::endl;
    }
    return DataType::Event;
  }

  if (fDumpFlag) {
    std::cout << "Unknown data type for PSD1" << std::endl;
  }
  return DataType::Unknown;
}

bool Dig1Decoder::CheckStop(std::unique_ptr<RawData_t> &rawData)
{
  // TODO: Implement PSD1-specific stop detection if needed
  return false;
}

bool Dig1Decoder::CheckStart(std::unique_ptr<RawData_t> &rawData)
{
  // TODO: Implement PSD1-specific start detection if needed
  return false;
}

}  // namespace Digitizer
}  // namespace DELILA
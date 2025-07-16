#ifndef DIG1DECODER_HPP
#define DIG1DECODER_HPP

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "RawData.hpp"
#include "EventData.hpp"
#include "DataType.hpp"

namespace DELILA {
namespace Digitizer {


class Dig1Decoder {
 public:
  // Constructor/Destructor
  explicit Dig1Decoder(uint32_t nThreads = 1);
  ~Dig1Decoder();

  // Configuration
  void SetTimeStep(uint32_t timeStep) { fTimeStep = timeStep; }
  void SetDumpFlag(bool dumpFlag) { fDumpFlag = dumpFlag; }
  void SetModuleNumber(uint8_t moduleNumber) { fModuleNumber = moduleNumber; }

  // Data Processing
  DataType AddData(std::unique_ptr<RawData_t> rawData);
  std::unique_ptr<std::vector<std::unique_ptr<EventData>>> GetEventData();

 private:
  // === Configuration ===
  uint32_t fTimeStep = 1;
  bool fDumpFlag = false;
  uint8_t fModuleNumber = 0;

  // === Threading Control ===
  bool fDecodeFlag = false;
  std::vector<std::thread> fDecodeThreads;

  // === Start/Stop State ===
  bool fIsRunning = false;

  // === Raw Data Queue ===
  std::deque<std::unique_ptr<RawData_t>> fRawDataQueue;
  std::mutex fRawDataMutex;

  // === Processed Data Storage ===
  std::unique_ptr<std::vector<std::unique_ptr<EventData>>> fEventDataVec;
  std::mutex fEventDataMutex;

  // === Data Processing State ===
  uint64_t fLastCounter = 0;

  // === Data Type Detection ===
  DataType CheckDataType(std::unique_ptr<RawData_t>& rawData);
  bool CheckStart(std::unique_ptr<RawData_t>& rawData);
  bool CheckStop(std::unique_ptr<RawData_t>& rawData);

  // === Data Processing ===
  void DecodeThread();
  void DecodeData(std::unique_ptr<RawData_t> rawData);
  
  // === Data Decoding Helpers ===
  void DumpRawData(const RawData_t& rawData) const;
  bool ValidateDataHeader(uint32_t headerWord, size_t dataSize);
  void ProcessEventData(const std::vector<uint8_t>::iterator& dataStart, 
                       uint32_t totalSize);

  // === PSD1 Format Structures ===
  
  // Board level (4 words)
  struct BoardHeaderInfo {
    uint32_t aggregateSize;      // [0:27] size in 32-bit words
    uint8_t dualChannelMask;     // [0:7] which channel pairs active
    uint16_t lvdsPattern;        // [8:22] LVDS pattern
    bool boardFailFlag;          // [26] board failure
    uint8_t boardId;             // [27:31] board identifier
    uint32_t aggregateCounter;   // [0:22] aggregate counter
    uint32_t boardTimeTag;       // [0:31] board time tag
  };
  
  // Dual channel level (2 words)
  struct DualChannelInfo {
    uint32_t aggregateSize;      // [0:21] dual channel size
    uint16_t numSamplesWave;     // [0:15] waveform samples/8
    uint8_t digitalProbe1;       // [16:18] DP1 selection
    uint8_t digitalProbe2;       // [19:21] DP2 selection  
    uint8_t analogProbe;         // [22:23] AP selection
    uint8_t extraOption;         // [24:26] extras format (usually 0b010)
    bool samplesEnabled;         // [27] ES: waveform enabled
    bool extrasEnabled;          // [28] EE: extras enabled
    bool timeEnabled;            // [29] ET: time tag enabled
    bool chargeEnabled;          // [30] EQ: charge enabled
    bool dualTraceEnabled;       // [31] DT: dual trace enabled
  };
  
  // PSD1 decoding methods
  bool DecodeBoardHeader(const std::vector<uint8_t>::iterator& dataStart, 
                        size_t& wordIndex, BoardHeaderInfo& boardInfo);
  bool DecodeDualChannelHeader(const std::vector<uint8_t>::iterator& dataStart,
                              size_t& wordIndex, DualChannelInfo& dualChInfo);
  std::unique_ptr<EventData> DecodeEventDirect(
      const std::vector<uint8_t>::iterator& dataStart, 
      size_t& wordIndex, const DualChannelInfo& dualChInfo);
  void DecodeWaveform(const std::vector<uint8_t>::iterator& dataStart,
                     size_t& wordIndex, const DualChannelInfo& dualChInfo,
                     EventData& eventData);
  uint16_t DecodeExtrasWord(uint32_t extrasWord, EventData& eventData);
  void DecodeChargeWord(uint32_t chargeWord, EventData& eventData);
};

}  // namespace Digitizer
}  // namespace DELILA

#endif  // DIG1DECODER_HPP
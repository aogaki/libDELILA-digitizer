#ifndef RAWTOPSD2_HPP
#define RAWTOPSD2_HPP

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "PSD2Data.hpp"
#include "RawData.hpp"

namespace DELILA {
namespace Digitizer {

enum class DataType {
  Start,
  Stop,
  Event,
  Unknown
};

class RawToPSD2 {
 public:
  // Constructor/Destructor
  explicit RawToPSD2(uint32_t nThreads = 1);
  ~RawToPSD2();

  // Configuration
  void SetTimeStep(uint32_t timeStep) { fTimeStep = timeStep; }
  void SetDumpFlag(bool dumpFlag) { fDumpFlag = dumpFlag; }

  // Data Processing
  DataType AddData(std::unique_ptr<RawData_t> rawData);
  std::unique_ptr<std::vector<std::unique_ptr<PSD2Data_t>>> GetData();

 private:
  // === Configuration ===
  uint32_t fTimeStep = 1;
  bool fDumpFlag = false;

  // === Threading Control ===
  bool fDecodeFlag = false;
  std::vector<std::thread> fDecodeThreads;

  // === Start/Stop State ===
  bool fIsRunning = false;

  // === Raw Data Queue ===
  std::deque<std::unique_ptr<RawData_t>> fRawDataQueue;
  std::mutex fRawDataMutex;

  // === Processed Data Storage ===
  std::unique_ptr<std::vector<std::unique_ptr<PSD2Data_t>>> fPSD2DataVec;
  std::mutex fPSD2DataMutex;

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
  bool ValidateDataHeader(uint64_t headerWord, size_t dataSize);
  void ProcessEventData(const std::vector<uint8_t>::iterator& dataStart, 
                       uint32_t totalSize);
  void DecodeEventPair(const std::vector<uint8_t>::iterator& dataStart,
                      size_t& wordIndex, PSD2Data_t& psd2Data);
  void DecodeFirstWord(uint64_t word, PSD2Data_t& psd2Data) const;
  void DecodeSecondWord(uint64_t word, PSD2Data_t& psd2Data) const;
  void DecodeWaveformData(const std::vector<uint8_t>::iterator& dataStart,
                         size_t& wordIndex, PSD2Data_t& psd2Data);
  void DecodeWaveformHeader(uint64_t header, PSD2Data_t& psd2Data) const;
  
  // Helper struct for waveform configuration
  struct WaveformConfig {
    bool ap1IsSigned;
    bool ap2IsSigned;
    uint32_t ap1MulFactor;
    uint32_t ap2MulFactor;
  };
  
  WaveformConfig ExtractWaveformConfig(uint64_t header) const;
  uint32_t GetMultiplicationFactor(uint32_t encodedValue) const;
  void DecodeWaveformPoint(uint32_t point, size_t dataIndex, 
                          const WaveformConfig& config, PSD2Data_t& psd2Data) const;
};

}  // namespace Digitizer
}  // namespace DELILA

#endif  // RAWTOPSD2_HPP
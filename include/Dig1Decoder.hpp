#ifndef DIG1DECODER_HPP
#define DIG1DECODER_HPP

#include <atomic>
#include <ctpl_stl.h>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

#include "DataType.hpp"
#include "DigitizerType.hpp"
#include "EventData.hpp"
#include "OutputFormat.hpp"
#include "RawData.hpp"

namespace DELILA
{
namespace Digitizer
{

class Dig1Decoder
{
 public:
  explicit Dig1Decoder(uint32_t nThreads = 1);
  ~Dig1Decoder();

  // Configuration
  void SetTimeStep(uint32_t timeStep) { fTimeStep = timeStep; }
  void SetDumpFlag(bool dumpFlag) { fDumpFlag = dumpFlag; }
  void SetOutputFormat(OutputFormat format) { fOutputFormat = format; }
  void SetModuleNumber(uint8_t moduleNumber) { fModuleNumber = moduleNumber; }
  void SetFirmwareType(DigitizerType type) { fFirmwareType = type; }

  // Data processing - Direct to EventData
  DataType AddData(std::unique_ptr<RawData_t> rawData);
  std::unique_ptr<std::vector<std::unique_ptr<EventData>>> GetEventData();

  // Control
  void ClearData();
  bool IsDataAvailable() const;

 private:
  // Dig1-specific decoding methods
  void DecodeData(std::unique_ptr<RawData_t> rawData);
  void ProcessRawData(const std::vector<uint8_t> &data, 
                      std::vector<std::unique_ptr<EventData>> &eventDataVec);
  std::unique_ptr<EventData> DecodeEvent(const std::vector<uint8_t>::const_iterator &dataStart, 
                                          size_t &wordIndex);
  
  // Helper methods for different firmware types
  void DecodePSDEvent(const std::vector<uint8_t>::const_iterator &dataStart, 
                      size_t &wordIndex, EventData &eventData);
  void DecodePHAEvent(const std::vector<uint8_t>::const_iterator &dataStart, 
                      size_t &wordIndex, EventData &eventData);

  // Member variables
  uint32_t fTimeStep = 8;         // Time resolution in ns (dig1 default)
  bool fDumpFlag = false;
  OutputFormat fOutputFormat = OutputFormat::ROOT;
  uint8_t fModuleNumber = 0;
  DigitizerType fFirmwareType = DigitizerType::UNKNOWN;
  
  // Thread pool for parallel processing
  std::unique_ptr<ctpl::thread_pool> fThreadPool;
  uint32_t fNThreads = 1;
  
  // Data storage
  std::queue<std::unique_ptr<std::vector<std::unique_ptr<EventData>>>> fDataQueue;
  mutable std::mutex fDataMutex;
  
  // Statistics
  std::atomic<uint64_t> fTotalEventsProcessed{0};
  std::atomic<uint64_t> fTotalBytesProcessed{0};
};

}  // namespace Digitizer
}  // namespace DELILA

#endif  // DIG1DECODER_HPP
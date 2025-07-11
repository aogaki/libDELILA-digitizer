#ifndef PSD2DATA_HPP
#define PSD2DATA_HPP

#include <cstddef>
#include <cstdint>
#include <vector>
#include <utility>

namespace DELILA {
namespace Digitizer {

/**
 * @brief PSD2 (Pulse Shape Discrimination 2) data structure
 * 
 * This class represents processed event data from a PSD2 digitizer,
 * containing timing information, energy measurements, flags, and 
 * optional waveform data from various probes.
 */
class PSD2Data {
 public:
  // Constructors and Destructor
  explicit PSD2Data(size_t waveformSize = 0);
  ~PSD2Data() = default;

  // Copy semantics
  PSD2Data(const PSD2Data& other);
  PSD2Data& operator=(const PSD2Data& other);

  // Move semantics
  PSD2Data(PSD2Data&& other) noexcept;
  PSD2Data& operator=(PSD2Data&& other) noexcept;

  // Waveform management
  void Resize(size_t size);
  void ClearWaveform();
  void ReserveWaveform(size_t capacity);
  
  // Getters - Basic properties
  uint64_t GetTimeStamp() const { return timeStamp_; }
  double GetTimeStampNs() const { return timeStampNs_; }
  size_t GetWaveformSize() const { return waveformSize_; }
  size_t GetEventSize() const { return eventSize_; }
  uint32_t GetAggregateCounter() const { return aggregateCounter_; }
  uint16_t GetFineTimeStamp() const { return fineTimeStamp_; }
  
  // Getters - Energy and channel info
  uint16_t GetEnergy() const { return energy_; }
  uint16_t GetEnergyShort() const { return energyShort_; }
  uint8_t GetChannel() const { return channel_; }
  uint8_t GetTimeResolution() const { return timeResolution_; }
  
  // Getters - Flags and trigger
  uint16_t GetFlagsLowPriority() const { return flagsLowPriority_; }
  uint16_t GetFlagsHighPriority() const { return flagsHighPriority_; }
  uint16_t GetTriggerThr() const { return triggerThr_; }
  bool GetBoardFail() const { return boardFail_; }
  bool GetFlush() const { return flush_; }
  
  // Getters - Probe types
  uint8_t GetAnalogProbe1Type() const { return analogProbe1Type_; }
  uint8_t GetAnalogProbe2Type() const { return analogProbe2Type_; }
  uint8_t GetDigitalProbe1Type() const { return digitalProbe1Type_; }
  uint8_t GetDigitalProbe2Type() const { return digitalProbe2Type_; }
  uint8_t GetDigitalProbe3Type() const { return digitalProbe3Type_; }
  uint8_t GetDigitalProbe4Type() const { return digitalProbe4Type_; }
  uint8_t GetDownSampleFactor() const { return downSampleFactor_; }
  
  // Getters - Waveform data (const)
  const std::vector<int32_t>& GetAnalogProbe1() const { return analogProbe1_; }
  const std::vector<int32_t>& GetAnalogProbe2() const { return analogProbe2_; }
  const std::vector<uint8_t>& GetDigitalProbe1() const { return digitalProbe1_; }
  const std::vector<uint8_t>& GetDigitalProbe2() const { return digitalProbe2_; }
  const std::vector<uint8_t>& GetDigitalProbe3() const { return digitalProbe3_; }
  const std::vector<uint8_t>& GetDigitalProbe4() const { return digitalProbe4_; }
  
  // Setters - Basic properties
  void SetTimeStamp(uint64_t timeStamp) { timeStamp_ = timeStamp; }
  void SetTimeStampNs(double timeStampNs) { timeStampNs_ = timeStampNs; }
  void SetEventSize(size_t eventSize) { eventSize_ = eventSize; }
  void SetAggregateCounter(uint32_t counter) { aggregateCounter_ = counter; }
  void SetFineTimeStamp(uint16_t fineTime) { fineTimeStamp_ = fineTime; }
  
  // Setters - Energy and channel info
  void SetEnergy(uint16_t energy) { energy_ = energy; }
  void SetEnergyShort(uint16_t energyShort) { energyShort_ = energyShort; }
  void SetChannel(uint8_t channel) { channel_ = channel; }
  void SetTimeResolution(uint8_t resolution) { timeResolution_ = resolution; }
  
  // Setters - Flags and trigger
  void SetFlagsLowPriority(uint16_t flags) { flagsLowPriority_ = flags; }
  void SetFlagsHighPriority(uint16_t flags) { flagsHighPriority_ = flags; }
  void SetTriggerThr(uint16_t threshold) { triggerThr_ = threshold; }
  void SetBoardFail(bool fail) { boardFail_ = fail; }
  void SetFlush(bool flush) { flush_ = flush; }
  
  // Setters - Probe types
  void SetAnalogProbe1Type(uint8_t type) { analogProbe1Type_ = type; }
  void SetAnalogProbe2Type(uint8_t type) { analogProbe2Type_ = type; }
  void SetDigitalProbe1Type(uint8_t type) { digitalProbe1Type_ = type; }
  void SetDigitalProbe2Type(uint8_t type) { digitalProbe2Type_ = type; }
  void SetDigitalProbe3Type(uint8_t type) { digitalProbe3Type_ = type; }
  void SetDigitalProbe4Type(uint8_t type) { digitalProbe4Type_ = type; }
  void SetDownSampleFactor(uint8_t factor) { downSampleFactor_ = factor; }
  
  // Setters - Waveform data (move-enabled)
  void SetAnalogProbe1(std::vector<int32_t> data) { analogProbe1_ = std::move(data); }
  void SetAnalogProbe2(std::vector<int32_t> data) { analogProbe2_ = std::move(data); }
  void SetDigitalProbe1(std::vector<uint8_t> data) { digitalProbe1_ = std::move(data); }
  void SetDigitalProbe2(std::vector<uint8_t> data) { digitalProbe2_ = std::move(data); }
  void SetDigitalProbe3(std::vector<uint8_t> data) { digitalProbe3_ = std::move(data); }
  void SetDigitalProbe4(std::vector<uint8_t> data) { digitalProbe4_ = std::move(data); }

  // Legacy public access (for backward compatibility)
  // TODO: Consider deprecating these in favor of getter/setter methods

  uint64_t timeStamp;
  double timeStampNs;
  size_t waveformSize;
  size_t eventSize;
  std::vector<int32_t> analogProbe1;
  std::vector<int32_t> analogProbe2;
  std::vector<uint8_t> digitalProbe1;
  std::vector<uint8_t> digitalProbe2;
  std::vector<uint8_t> digitalProbe3;
  std::vector<uint8_t> digitalProbe4;
  uint32_t aggregateCounter;
  uint16_t fineTimeStamp;
  uint16_t energy;
  uint16_t energyShort;
  uint16_t flagsLowPriority;
  uint16_t flagsHighPriority;
  uint16_t triggerThr;
  uint8_t channel;
  uint8_t timeResolution;
  uint8_t analogProbe1Type;
  uint8_t analogProbe2Type;
  uint8_t digitalProbe1Type;
  uint8_t digitalProbe2Type;
  uint8_t digitalProbe3Type;
  uint8_t digitalProbe4Type;
  uint8_t downSampleFactor;
  bool boardFail;
  bool flush;

 private:
  // Private members (future migration target)
  uint64_t timeStamp_ = 0;
  double timeStampNs_ = 0.0;
  size_t waveformSize_ = 0;
  size_t eventSize_ = 0;
  uint32_t aggregateCounter_ = 0;
  uint16_t fineTimeStamp_ = 0;
  uint16_t energy_ = 0;
  uint16_t energyShort_ = 0;
  uint16_t flagsLowPriority_ = 0;
  uint16_t flagsHighPriority_ = 0;
  uint16_t triggerThr_ = 0;
  uint8_t channel_ = 0;
  uint8_t timeResolution_ = 0;
  uint8_t analogProbe1Type_ = 0;
  uint8_t analogProbe2Type_ = 0;
  uint8_t digitalProbe1Type_ = 0;
  uint8_t digitalProbe2Type_ = 0;
  uint8_t digitalProbe3Type_ = 0;
  uint8_t digitalProbe4Type_ = 0;
  uint8_t downSampleFactor_ = 0;
  bool boardFail_ = false;
  bool flush_ = false;
  
  std::vector<int32_t> analogProbe1_;
  std::vector<int32_t> analogProbe2_;
  std::vector<uint8_t> digitalProbe1_;
  std::vector<uint8_t> digitalProbe2_;
  std::vector<uint8_t> digitalProbe3_;
  std::vector<uint8_t> digitalProbe4_;
  
  // Helper method for copying data
  void CopyFrom(const PSD2Data& other);
};

// Modern type alias
using PSD2Data_t = PSD2Data;

// Legacy typedef for backward compatibility
typedef PSD2Data PSD2DATA_t;

}  // namespace Digitizer
}  // namespace DELILA

#endif  // PSD2DATA_HPP
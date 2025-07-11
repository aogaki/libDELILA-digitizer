#ifndef EVENTDATA_HPP
#define EVENTDATA_HPP

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace DELILA
{
namespace Digitizer
{

/**
 * @brief Event data structure for digitizer events
 *
 * This class represents a single event from a digitizer, containing
 * timing information, energy measurements, and optional waveform data.
 */
class EventData
{
 public:
  // Constructors and Destructor
  explicit EventData(size_t waveformSize = 0);
  ~EventData() = default;

  // Copy semantics
  EventData(const EventData &other);
  EventData &operator=(const EventData &other);

  // Move semantics
  EventData(EventData &&other) noexcept;
  EventData &operator=(EventData &&other) noexcept;

  // Waveform management
  void ResizeWaveform(size_t size);
  void ClearWaveform();

  // Display methods
  void Print() const;
  void PrintSummary() const;
  void PrintWaveform(size_t maxSamples = 10) const;

  // Getters
  double GetTimeStampNs() const { return timeStampNs_; }
  size_t GetWaveformSize() const { return waveformSize_; }
  uint16_t GetEnergy() const { return energy_; }
  uint16_t GetEnergyShort() const { return energyShort_; }
  uint8_t GetModule() const { return module_; }
  uint8_t GetChannel() const { return channel_; }
  uint8_t GetTimeResolution() const { return timeResolution_; }
  uint8_t GetDownSampleFactor() const { return downSampleFactor_; }

  // Probe type getters
  uint8_t GetAnalogProbe1Type() const { return analogProbe1Type_; }
  uint8_t GetAnalogProbe2Type() const { return analogProbe2Type_; }
  uint8_t GetDigitalProbe1Type() const { return digitalProbe1Type_; }
  uint8_t GetDigitalProbe2Type() const { return digitalProbe2Type_; }
  uint8_t GetDigitalProbe3Type() const { return digitalProbe3Type_; }
  uint8_t GetDigitalProbe4Type() const { return digitalProbe4Type_; }

  // Waveform data getters (const)
  const std::vector<int32_t> &GetAnalogProbe1() const { return analogProbe1_; }
  const std::vector<int32_t> &GetAnalogProbe2() const { return analogProbe2_; }
  const std::vector<uint8_t> &GetDigitalProbe1() const
  {
    return digitalProbe1_;
  }
  const std::vector<uint8_t> &GetDigitalProbe2() const
  {
    return digitalProbe2_;
  }
  const std::vector<uint8_t> &GetDigitalProbe3() const
  {
    return digitalProbe3_;
  }
  const std::vector<uint8_t> &GetDigitalProbe4() const
  {
    return digitalProbe4_;
  }

  // Setters
  void SetTimeStampNs(double timeStamp) { timeStampNs_ = timeStamp; }
  void SetEnergy(uint16_t energy) { energy_ = energy; }
  void SetEnergyShort(uint16_t energyShort) { energyShort_ = energyShort; }
  void SetModule(uint8_t module) { module_ = module; }
  void SetChannel(uint8_t channel) { channel_ = channel; }
  void SetTimeResolution(uint8_t timeResolution)
  {
    timeResolution_ = timeResolution;
  }
  void SetDownSampleFactor(uint8_t factor) { downSampleFactor_ = factor; }

  // Probe type setters
  void SetAnalogProbe1Type(uint8_t type) { analogProbe1Type_ = type; }
  void SetAnalogProbe2Type(uint8_t type) { analogProbe2Type_ = type; }
  void SetDigitalProbe1Type(uint8_t type) { digitalProbe1Type_ = type; }
  void SetDigitalProbe2Type(uint8_t type) { digitalProbe2Type_ = type; }
  void SetDigitalProbe3Type(uint8_t type) { digitalProbe3Type_ = type; }
  void SetDigitalProbe4Type(uint8_t type) { digitalProbe4Type_ = type; }

  // Waveform data setters (move-enabled)
  void SetAnalogProbe1(std::vector<int32_t> data)
  {
    analogProbe1_ = std::move(data);
  }
  void SetAnalogProbe2(std::vector<int32_t> data)
  {
    analogProbe2_ = std::move(data);
  }
  void SetDigitalProbe1(std::vector<uint8_t> data)
  {
    digitalProbe1_ = std::move(data);
  }
  void SetDigitalProbe2(std::vector<uint8_t> data)
  {
    digitalProbe2_ = std::move(data);
  }
  void SetDigitalProbe3(std::vector<uint8_t> data)
  {
    digitalProbe3_ = std::move(data);
  }
  void SetDigitalProbe4(std::vector<uint8_t> data)
  {
    digitalProbe4_ = std::move(data);
  }

  // Legacy public access (for backward compatibility)
  // TODO: Consider deprecating these in favor of getter/setter methods
  double timeStampNs;
  size_t waveformSize;
  std::vector<int32_t> analogProbe1;
  std::vector<int32_t> analogProbe2;
  std::vector<uint8_t> digitalProbe1;
  std::vector<uint8_t> digitalProbe2;
  std::vector<uint8_t> digitalProbe3;
  std::vector<uint8_t> digitalProbe4;
  uint16_t energy;
  uint16_t energyShort;
  uint8_t module;
  uint8_t channel;
  uint8_t timeResolution;
  uint8_t analogProbe1Type;
  uint8_t analogProbe2Type;
  uint8_t digitalProbe1Type;
  uint8_t digitalProbe2Type;
  uint8_t digitalProbe3Type;
  uint8_t digitalProbe4Type;
  uint8_t downSampleFactor;

 private:
  // Private members (future migration target)
  double timeStampNs_ = 0.0;
  size_t waveformSize_ = 0;
  uint16_t energy_ = 0;
  uint16_t energyShort_ = 0;
  uint8_t module_ = 0;
  uint8_t channel_ = 0;
  uint8_t timeResolution_ = 0;
  uint8_t analogProbe1Type_ = 0;
  uint8_t analogProbe2Type_ = 0;
  uint8_t digitalProbe1Type_ = 0;
  uint8_t digitalProbe2Type_ = 0;
  uint8_t digitalProbe3Type_ = 0;
  uint8_t digitalProbe4Type_ = 0;
  uint8_t downSampleFactor_ = 0;

  std::vector<int32_t> analogProbe1_;
  std::vector<int32_t> analogProbe2_;
  std::vector<uint8_t> digitalProbe1_;
  std::vector<uint8_t> digitalProbe2_;
  std::vector<uint8_t> digitalProbe3_;
  std::vector<uint8_t> digitalProbe4_;

  // Helper method for copying data
  void CopyFrom(const EventData &other);
};

// Modern type alias
using EventData_t = EventData;

// Legacy typedef for backward compatibility
using EVENTDATA_t = EventData;

}  // namespace Digitizer
}  // namespace DELILA

#endif  // EVENTDATA_HPP
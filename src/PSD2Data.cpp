#include "PSD2Data.hpp"

#include <algorithm>

namespace DELILA {
namespace Digitizer {

// Constructor
PSD2Data::PSD2Data(size_t waveformSize) 
    : timeStamp(0),
      timeStampNs(0.0),
      waveformSize(0),
      eventSize(0),
      aggregateCounter(0),
      fineTimeStamp(0),
      energy(0),
      energyShort(0),
      flagsLowPriority(0),
      flagsHighPriority(0),
      triggerThr(0),
      channel(0),
      timeResolution(0),
      analogProbe1Type(0),
      analogProbe2Type(0),
      digitalProbe1Type(0),
      digitalProbe2Type(0),
      digitalProbe3Type(0),
      digitalProbe4Type(0),
      downSampleFactor(0),
      boardFail(false),
      flush(false) {
  if (waveformSize > 0) {
    Resize(waveformSize);
  }
}

// Copy constructor
PSD2Data::PSD2Data(const PSD2Data& other) {
  CopyFrom(other);
}

// Copy assignment operator
PSD2Data& PSD2Data::operator=(const PSD2Data& other) {
  if (this != &other) {
    CopyFrom(other);
  }
  return *this;
}

// Move constructor
PSD2Data::PSD2Data(PSD2Data&& other) noexcept
    : timeStamp(other.timeStamp),
      timeStampNs(other.timeStampNs),
      waveformSize(other.waveformSize),
      eventSize(other.eventSize),
      analogProbe1(std::move(other.analogProbe1)),
      analogProbe2(std::move(other.analogProbe2)),
      digitalProbe1(std::move(other.digitalProbe1)),
      digitalProbe2(std::move(other.digitalProbe2)),
      digitalProbe3(std::move(other.digitalProbe3)),
      digitalProbe4(std::move(other.digitalProbe4)),
      aggregateCounter(other.aggregateCounter),
      fineTimeStamp(other.fineTimeStamp),
      energy(other.energy),
      energyShort(other.energyShort),
      flagsLowPriority(other.flagsLowPriority),
      flagsHighPriority(other.flagsHighPriority),
      triggerThr(other.triggerThr),
      channel(other.channel),
      timeResolution(other.timeResolution),
      analogProbe1Type(other.analogProbe1Type),
      analogProbe2Type(other.analogProbe2Type),
      digitalProbe1Type(other.digitalProbe1Type),
      digitalProbe2Type(other.digitalProbe2Type),
      digitalProbe3Type(other.digitalProbe3Type),
      digitalProbe4Type(other.digitalProbe4Type),
      downSampleFactor(other.downSampleFactor),
      boardFail(other.boardFail),
      flush(other.flush),
      // Private members
      timeStamp_(other.timeStamp_),
      timeStampNs_(other.timeStampNs_),
      waveformSize_(other.waveformSize_),
      eventSize_(other.eventSize_),
      aggregateCounter_(other.aggregateCounter_),
      fineTimeStamp_(other.fineTimeStamp_),
      energy_(other.energy_),
      energyShort_(other.energyShort_),
      flagsLowPriority_(other.flagsLowPriority_),
      flagsHighPriority_(other.flagsHighPriority_),
      triggerThr_(other.triggerThr_),
      channel_(other.channel_),
      timeResolution_(other.timeResolution_),
      analogProbe1Type_(other.analogProbe1Type_),
      analogProbe2Type_(other.analogProbe2Type_),
      digitalProbe1Type_(other.digitalProbe1Type_),
      digitalProbe2Type_(other.digitalProbe2Type_),
      digitalProbe3Type_(other.digitalProbe3Type_),
      digitalProbe4Type_(other.digitalProbe4Type_),
      downSampleFactor_(other.downSampleFactor_),
      boardFail_(other.boardFail_),
      flush_(other.flush_),
      analogProbe1_(std::move(other.analogProbe1_)),
      analogProbe2_(std::move(other.analogProbe2_)),
      digitalProbe1_(std::move(other.digitalProbe1_)),
      digitalProbe2_(std::move(other.digitalProbe2_)),
      digitalProbe3_(std::move(other.digitalProbe3_)),
      digitalProbe4_(std::move(other.digitalProbe4_)) {
  
  // Reset other object
  other.timeStamp = 0;
  other.timeStampNs = 0.0;
  other.waveformSize = 0;
  other.eventSize = 0;
  other.aggregateCounter = 0;
  other.fineTimeStamp = 0;
  other.energy = 0;
  other.energyShort = 0;
  other.flagsLowPriority = 0;
  other.flagsHighPriority = 0;
  other.triggerThr = 0;
  other.channel = 0;
  other.timeResolution = 0;
  other.analogProbe1Type = 0;
  other.analogProbe2Type = 0;
  other.digitalProbe1Type = 0;
  other.digitalProbe2Type = 0;
  other.digitalProbe3Type = 0;
  other.digitalProbe4Type = 0;
  other.downSampleFactor = 0;
  other.boardFail = false;
  other.flush = false;
}

// Move assignment operator
PSD2Data& PSD2Data::operator=(PSD2Data&& other) noexcept {
  if (this != &other) {
    // Move public members
    timeStamp = other.timeStamp;
    timeStampNs = other.timeStampNs;
    waveformSize = other.waveformSize;
    eventSize = other.eventSize;
    analogProbe1 = std::move(other.analogProbe1);
    analogProbe2 = std::move(other.analogProbe2);
    digitalProbe1 = std::move(other.digitalProbe1);
    digitalProbe2 = std::move(other.digitalProbe2);
    digitalProbe3 = std::move(other.digitalProbe3);
    digitalProbe4 = std::move(other.digitalProbe4);
    aggregateCounter = other.aggregateCounter;
    fineTimeStamp = other.fineTimeStamp;
    energy = other.energy;
    energyShort = other.energyShort;
    flagsLowPriority = other.flagsLowPriority;
    flagsHighPriority = other.flagsHighPriority;
    triggerThr = other.triggerThr;
    channel = other.channel;
    timeResolution = other.timeResolution;
    analogProbe1Type = other.analogProbe1Type;
    analogProbe2Type = other.analogProbe2Type;
    digitalProbe1Type = other.digitalProbe1Type;
    digitalProbe2Type = other.digitalProbe2Type;
    digitalProbe3Type = other.digitalProbe3Type;
    digitalProbe4Type = other.digitalProbe4Type;
    downSampleFactor = other.downSampleFactor;
    boardFail = other.boardFail;
    flush = other.flush;
    
    // Move private members
    timeStamp_ = other.timeStamp_;
    timeStampNs_ = other.timeStampNs_;
    waveformSize_ = other.waveformSize_;
    eventSize_ = other.eventSize_;
    aggregateCounter_ = other.aggregateCounter_;
    fineTimeStamp_ = other.fineTimeStamp_;
    energy_ = other.energy_;
    energyShort_ = other.energyShort_;
    flagsLowPriority_ = other.flagsLowPriority_;
    flagsHighPriority_ = other.flagsHighPriority_;
    triggerThr_ = other.triggerThr_;
    channel_ = other.channel_;
    timeResolution_ = other.timeResolution_;
    analogProbe1Type_ = other.analogProbe1Type_;
    analogProbe2Type_ = other.analogProbe2Type_;
    digitalProbe1Type_ = other.digitalProbe1Type_;
    digitalProbe2Type_ = other.digitalProbe2Type_;
    digitalProbe3Type_ = other.digitalProbe3Type_;
    digitalProbe4Type_ = other.digitalProbe4Type_;
    downSampleFactor_ = other.downSampleFactor_;
    boardFail_ = other.boardFail_;
    flush_ = other.flush_;
    analogProbe1_ = std::move(other.analogProbe1_);
    analogProbe2_ = std::move(other.analogProbe2_);
    digitalProbe1_ = std::move(other.digitalProbe1_);
    digitalProbe2_ = std::move(other.digitalProbe2_);
    digitalProbe3_ = std::move(other.digitalProbe3_);
    digitalProbe4_ = std::move(other.digitalProbe4_);
    
    // Reset other object
    other.timeStamp = 0;
    other.timeStampNs = 0.0;
    other.waveformSize = 0;
    other.eventSize = 0;
    other.aggregateCounter = 0;
    other.fineTimeStamp = 0;
    other.energy = 0;
    other.energyShort = 0;
    other.flagsLowPriority = 0;
    other.flagsHighPriority = 0;
    other.triggerThr = 0;
    other.channel = 0;
    other.timeResolution = 0;
    other.analogProbe1Type = 0;
    other.analogProbe2Type = 0;
    other.digitalProbe1Type = 0;
    other.digitalProbe2Type = 0;
    other.digitalProbe3Type = 0;
    other.digitalProbe4Type = 0;
    other.downSampleFactor = 0;
    other.boardFail = false;
    other.flush = false;
  }
  return *this;
}

void PSD2Data::Resize(size_t size) {
  waveformSize = size;
  waveformSize_ = size;
  
  // Resize public vectors
  analogProbe1.resize(size);
  analogProbe2.resize(size);
  digitalProbe1.resize(size);
  digitalProbe2.resize(size);
  digitalProbe3.resize(size);
  digitalProbe4.resize(size);
  
  // Resize private vectors
  analogProbe1_.resize(size);
  analogProbe2_.resize(size);
  digitalProbe1_.resize(size);
  digitalProbe2_.resize(size);
  digitalProbe3_.resize(size);
  digitalProbe4_.resize(size);
}

void PSD2Data::ClearWaveform() {
  Resize(0);
}

void PSD2Data::ReserveWaveform(size_t capacity) {
  // Reserve space for public vectors
  analogProbe1.reserve(capacity);
  analogProbe2.reserve(capacity);
  digitalProbe1.reserve(capacity);
  digitalProbe2.reserve(capacity);
  digitalProbe3.reserve(capacity);
  digitalProbe4.reserve(capacity);
  
  // Reserve space for private vectors
  analogProbe1_.reserve(capacity);
  analogProbe2_.reserve(capacity);
  digitalProbe1_.reserve(capacity);
  digitalProbe2_.reserve(capacity);
  digitalProbe3_.reserve(capacity);
  digitalProbe4_.reserve(capacity);
}

void PSD2Data::CopyFrom(const PSD2Data& other) {
  // Copy public members
  timeStamp = other.timeStamp;
  timeStampNs = other.timeStampNs;
  waveformSize = other.waveformSize;
  eventSize = other.eventSize;
  analogProbe1 = other.analogProbe1;
  analogProbe2 = other.analogProbe2;
  digitalProbe1 = other.digitalProbe1;
  digitalProbe2 = other.digitalProbe2;
  digitalProbe3 = other.digitalProbe3;
  digitalProbe4 = other.digitalProbe4;
  aggregateCounter = other.aggregateCounter;
  fineTimeStamp = other.fineTimeStamp;
  energy = other.energy;
  energyShort = other.energyShort;
  flagsLowPriority = other.flagsLowPriority;
  flagsHighPriority = other.flagsHighPriority;
  triggerThr = other.triggerThr;
  channel = other.channel;
  timeResolution = other.timeResolution;
  analogProbe1Type = other.analogProbe1Type;
  analogProbe2Type = other.analogProbe2Type;
  digitalProbe1Type = other.digitalProbe1Type;
  digitalProbe2Type = other.digitalProbe2Type;
  digitalProbe3Type = other.digitalProbe3Type;
  digitalProbe4Type = other.digitalProbe4Type;
  downSampleFactor = other.downSampleFactor;
  boardFail = other.boardFail;
  flush = other.flush;
  
  // Copy private members
  timeStamp_ = other.timeStamp_;
  timeStampNs_ = other.timeStampNs_;
  waveformSize_ = other.waveformSize_;
  eventSize_ = other.eventSize_;
  aggregateCounter_ = other.aggregateCounter_;
  fineTimeStamp_ = other.fineTimeStamp_;
  energy_ = other.energy_;
  energyShort_ = other.energyShort_;
  flagsLowPriority_ = other.flagsLowPriority_;
  flagsHighPriority_ = other.flagsHighPriority_;
  triggerThr_ = other.triggerThr_;
  channel_ = other.channel_;
  timeResolution_ = other.timeResolution_;
  analogProbe1Type_ = other.analogProbe1Type_;
  analogProbe2Type_ = other.analogProbe2Type_;
  digitalProbe1Type_ = other.digitalProbe1Type_;
  digitalProbe2Type_ = other.digitalProbe2Type_;
  digitalProbe3Type_ = other.digitalProbe3Type_;
  digitalProbe4Type_ = other.digitalProbe4Type_;
  downSampleFactor_ = other.downSampleFactor_;
  boardFail_ = other.boardFail_;
  flush_ = other.flush_;
  analogProbe1_ = other.analogProbe1_;
  analogProbe2_ = other.analogProbe2_;
  digitalProbe1_ = other.digitalProbe1_;
  digitalProbe2_ = other.digitalProbe2_;
  digitalProbe3_ = other.digitalProbe3_;
  digitalProbe4_ = other.digitalProbe4_;
}

}  // namespace Digitizer
}  // namespace DELILA
#include "RawData.hpp"

#include <algorithm>

namespace DELILA {
namespace Digitizer {

// Constructor
RawData::RawData(size_t dataSize) 
    : size(0),
      nEvents(0),
      size_(0),
      nEvents_(0) {
  if (dataSize > 0) {
    Resize(dataSize);
  }
}

// Copy constructor
RawData::RawData(const RawData& other) {
  CopyFrom(other);
}

// Copy assignment operator
RawData& RawData::operator=(const RawData& other) {
  if (this != &other) {
    CopyFrom(other);
  }
  return *this;
}

// Move constructor
RawData::RawData(RawData&& other) noexcept
    : data(std::move(other.data)),
      size(other.size),
      nEvents(other.nEvents),
      data_(std::move(other.data_)),
      size_(other.size_),
      nEvents_(other.nEvents_) {
  
  // Reset other object
  other.size = 0;
  other.nEvents = 0;
  other.size_ = 0;
  other.nEvents_ = 0;
}

// Move assignment operator
RawData& RawData::operator=(RawData&& other) noexcept {
  if (this != &other) {
    // Move public members
    data = std::move(other.data);
    size = other.size;
    nEvents = other.nEvents;
    
    // Move private members
    data_ = std::move(other.data_);
    size_ = other.size_;
    nEvents_ = other.nEvents_;
    
    // Reset other object
    other.size = 0;
    other.nEvents = 0;
    other.size_ = 0;
    other.nEvents_ = 0;
  }
  return *this;
}

void RawData::Resize(size_t newSize) {
  size = newSize;
  size_ = newSize;
  
  // Resize both public and private vectors
  data.resize(newSize);
  data_.resize(newSize);
}

void RawData::Clear() {
  Resize(0);
  nEvents = 0;
  nEvents_ = 0;
}

void RawData::Reserve(size_t capacity) {
  data.reserve(capacity);
  data_.reserve(capacity);
}

void RawData::SetData(std::vector<uint8_t> newData) {
  data_ = std::move(newData);
  data = data_;  // Copy to public member for backward compatibility
  size_ = data_.size();
  size = size_;
}

void RawData::CopyFrom(const RawData& other) {
  // Copy public members
  data = other.data;
  size = other.size;
  nEvents = other.nEvents;
  
  // Copy private members
  data_ = other.data_;
  size_ = other.size_;
  nEvents_ = other.nEvents_;
}

}  // namespace Digitizer
}  // namespace DELILA
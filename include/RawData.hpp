#ifndef RAWDATA_HPP
#define RAWDATA_HPP

#include <cstddef>
#include <cstdint>
#include <vector>
#include <utility>

namespace DELILA {
namespace Digitizer {

/**
 * @brief Raw data container for digitizer data
 * 
 * This class represents raw data buffer from a digitizer, containing
 * binary data along with size and event count information.
 */
class RawData {
 public:
  // Constructors and Destructor
  explicit RawData(size_t dataSize = 0);
  ~RawData() = default;

  // Copy semantics
  RawData(const RawData& other);
  RawData& operator=(const RawData& other);

  // Move semantics
  RawData(RawData&& other) noexcept;
  RawData& operator=(RawData&& other) noexcept;

  // Data management
  void Resize(size_t newSize);
  void Clear();
  void Reserve(size_t capacity);
  
  // Getters
  const std::vector<uint8_t>& GetData() const { return data_; }
  std::vector<uint8_t>& GetData() { return data_; }
  size_t GetSize() const { return size_; }
  uint32_t GetNEvents() const { return nEvents_; }
  size_t GetCapacity() const { return data_.capacity(); }
  bool IsEmpty() const { return data_.empty(); }
  
  // Setters
  void SetData(std::vector<uint8_t> newData);
  void SetSize(size_t newSize) { size_ = newSize; }
  void SetNEvents(uint32_t events) { nEvents_ = events; }

  // Legacy public access (for backward compatibility)
  // TODO: Consider deprecating these in favor of getter/setter methods
  std::vector<uint8_t> data;
  size_t size;
  uint32_t nEvents;

 private:
  // Private members (future migration target)
  std::vector<uint8_t> data_;
  size_t size_ = 0;
  uint32_t nEvents_ = 0;
  
  // Helper method for copying data
  void CopyFrom(const RawData& other);
};

// Modern type alias
using RawData_t = RawData;

// Legacy typedef for backward compatibility
typedef RawData RAWDATA_t;

}  // namespace Digitizer
}  // namespace DELILA

#endif  // RAWDATA_HPP
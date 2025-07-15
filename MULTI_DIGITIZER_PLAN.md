# Multi-Digitizer Architecture Implementation Plan

## Overview
This document outlines the plan to extend the current dig2 (PSD2) digitizer implementation to support dig1 and create a unified interface for multiple digitizer types.

## Current State Analysis

### Existing Architecture
- **Current Class**: `Digitizer` - hardcoded for dig2 (PSD2) format
- **Data Flow**: Raw Data → `RawToPSD2` → `EventData`
- **Supported Types**: Only dig2 (PSD2, PHA2, SCOPE2, etc.)
- **Configuration**: Single configuration system
- **URL Format**: Various formats (USB, Ethernet, etc.)

### Limitations
- Hardcoded for dig2 format only
- No abstraction for different digitizer types
- Cannot support dig1 without major refactoring
- Tight coupling between hardware interface and data processing

## Target Architecture

### 1. Interface-Based Design

```cpp
// Base interface for all digitizers
class IDigitizer {
public:
    virtual ~IDigitizer() = default;
    
    // Main lifecycle methods
    virtual bool Initialize(const ConfigurationManager& config) = 0;
    virtual bool Configure() = 0;
    virtual bool StartAcquisition() = 0;
    virtual bool StopAcquisition() = 0;
    
    // Control methods
    virtual bool SendSWTrigger() = 0;
    virtual bool CheckStatus() = 0;
    
    // Data access
    virtual std::unique_ptr<std::vector<std::unique_ptr<EventData>>> GetEventData() = 0;
    
    // Device information
    virtual void PrintDeviceInfo() = 0;
    virtual std::string GetDeviceTree() const = 0;
    virtual DigitizerType GetType() const = 0;
};
```

### 2. Specific Implementations

```cpp
// For dig1 types (PSD1, PHA1, QDC1, SCOPE1)
class Digitizer1 : public IDigitizer {
private:
    // Hardware interface (same as dig2)
    uint64_t fHandle = 0;
    uint64_t fReadDataHandle = 0;
    
    // Device information
    nlohmann::json fDeviceTree;
    DigitizerType fDigitizerType = DigitizerType::UNKNOWN;
    
    // Data processing
    std::unique_ptr<RawToDig1> fRawToDig1;
    
    // Parameter validation (same system as dig2)
    std::unique_ptr<ParameterValidator> fParameterValidator;
    
    // Configuration and data handling
    std::vector<std::array<std::string, 2>> fConfig;
    uint8_t fModuleNumber = 0;
    // Additional dig1-specific members
};

// For dig2 types (PSD2, PHA2, SCOPE2) - current Digitizer renamed
class Digitizer2 : public IDigitizer {
private:
    std::unique_ptr<RawToPSD2> fRawToPSD2;
    // dig2-specific members (existing code)
};
```

### 3. Factory Pattern

```cpp
class DigitizerFactory {
public:
    static std::unique_ptr<IDigitizer> CreateDigitizer(const ConfigurationManager& config);
    static DigitizerType DetectDigitizerType(const std::string& url);
    static DigitizerType DetectFromDeviceTree(const nlohmann::json& deviceTree);
    
private:
    static DigitizerType ParseURL(const std::string& url);
    static DigitizerType AnalyzeFirmware(const std::string& fwType, const std::string& modelName);
};
```

### 4. Data Structures

```cpp
// Dig1-specific intermediate data structure
struct Dig1Data_t {
    // Basic event information
    uint8_t channel = 0;
    uint64_t timeStamp = 0;
    double timeStampNs = 0.0;
    uint16_t energy = 0;
    int16_t energyShort = 0;
    uint32_t flags = 0;
    
    // Waveform data
    size_t waveformSize = 0;
    std::vector<int16_t> analogProbe1;
    std::vector<int16_t> analogProbe2;
    std::vector<uint8_t> digitalProbe1;
    std::vector<uint8_t> digitalProbe2;
    std::vector<uint8_t> digitalProbe3;
    std::vector<uint8_t> digitalProbe4;
    
    // Dig1-specific fields
    uint32_t timeResolution = 8;    // Time resolution in ns
    uint32_t downSampleFactor = 1;  // Down sample factor
    
    // Probe type information
    int32_t analogProbe1Type = 0;
    int32_t analogProbe2Type = 0;
    int32_t digitalProbe1Type = 0;
    int32_t digitalProbe2Type = 0;
    int32_t digitalProbe3Type = 0;
    int32_t digitalProbe4Type = 0;
};

// Raw to Dig1 converter
class RawToDig1 {
public:
    explicit RawToDig1(uint32_t nThreads = 1);
    ~RawToDig1();
    
    // Configuration
    void SetTimeStep(uint32_t timeStep);
    void SetDumpFlag(bool dumpFlag);
    void SetOutputFormat(OutputFormat format);
    void SetModuleNumber(uint8_t moduleNumber);
    
    // Data processing
    DataType AddData(std::unique_ptr<RawData_t> rawData);
    std::unique_ptr<std::vector<std::unique_ptr<Dig1Data_t>>> GetData();
    std::unique_ptr<std::vector<std::unique_ptr<EventData>>> GetEventData();
    
private:
    // Dig1-specific decoding methods
    void DecodeData(std::unique_ptr<RawData_t> rawData);
    void ProcessEventData(const std::vector<uint8_t>::iterator& dataStart, uint32_t totalSize);
    void DecodeEvent(const std::vector<uint8_t>::iterator& dataStart, size_t& wordIndex, Dig1Data_t& dig1Data);
    std::unique_ptr<EventData> ConvertDig1ToEventData(const Dig1Data_t& dig1Data) const;
};
```

## Implementation Phases

### Phase 1: Create Interface & Factory ✅ COMPLETED

#### Tasks: ✅ ALL COMPLETED
1. **✅ Create IDigitizer Interface**
   - ✅ Define pure virtual methods for all digitizer operations
   - ✅ Ensure all existing Digitizer methods are covered
   - ✅ Add GetType() method for runtime type identification

2. **✅ Create DigitizerFactory Class**
   - ✅ Implement URL parsing for dig1:// and dig2:// schemes
   - ✅ Add device tree analysis for automatic type detection
   - ✅ Create factory method for digitizer instantiation

3. **✅ Refactor Current Implementation**
   - ✅ Rename `Digitizer` class to `Digitizer2`
   - ✅ Make `Digitizer2` inherit from `IDigitizer`
   - ✅ Ensure all existing functionality is preserved

4. **✅ Backward Compatibility Maintained**
   - ✅ Original `Digitizer` class converted to factory-based wrapper
   - ✅ main.cpp requires no changes
   - ✅ All existing API calls continue to work

#### Files Created/Modified: ✅ COMPLETED
- ✅ `include/IDigitizer.hpp` (created)
- ✅ `include/DigitizerFactory.hpp` (created)
- ✅ `src/DigitizerFactory.cpp` (created)
- ✅ `include/Digitizer2.hpp` (created from Digitizer.hpp)
- ✅ `src/Digitizer2.cpp` (created from Digitizer.cpp)
- ✅ `include/Digitizer.hpp` (converted to wrapper class)
- ✅ `src/Digitizer.cpp` (converted to factory-based wrapper)
- ✅ CMakeLists.txt automatically includes new files via glob patterns
- ✅ main.cpp requires no changes (backward compatibility preserved)

### Phase 2: Implement Dig1 Support (High Priority)

#### Tasks:
1. **Create Dig1 Data Structures**
   - Define `Dig1Data_t` structure
   - Implement constructors, copy/move semantics
   - Add validation methods

2. **Implement RawToDig1 Converter**
   - Create class with similar interface to RawToPSD2
   - Implement dig1-specific raw data parsing
   - Add direct EventData conversion with sorting
   - Support both Dig1Data and EventData output formats

3. **Implement Digitizer1 Class**
   - Create class inheriting from IDigitizer
   - Implement all interface methods
   - Integrate with RawToDig1 converter
   - Handle dig1-specific configuration
   - **Implement Parameter Validation System (same as dig2)**:
     - Fetch and parse device tree from dig1 hardware
     - Create ParameterValidator instance with dig1 device tree
     - Validate configuration parameters against dig1 device tree structure
     - Handle dig1-specific parameter mappings and validation rules

4. **Add Type Detection Logic**
   - Implement URL-based detection (dig1://)
   - Add device tree analysis for dig1 types
   - Update factory to create appropriate digitizer

5. **Parameter Validation Integration**
   - **Device Tree Fetching**: Implement device tree retrieval for dig1 hardware
     - Use same CAEN_FELib APIs as dig2 for device tree access
     - Parse JSON device tree structure specific to dig1 firmware
     - Handle dig1-specific parameter paths and hierarchies
   - **Validation System**: Integrate existing ParameterValidator class
     - Create ParameterValidator instance with dig1 device tree
     - Validate all configuration parameters against dig1 device tree
     - Use same validation methods as dig2 (ValidateParameters, ValidateParameter)
     - Handle dig1-specific parameter types and constraints
   - **Configuration Mapping**: Map configuration file parameters to dig1 device tree paths
     - Handle channel-specific parameters (e.g., "/ch/0/par/trigger_threshold")
     - Support global parameters (e.g., "/par/sample_rate")
     - Implement dig1-specific parameter name mappings
   - **Error Handling**: Provide detailed validation error reporting
     - Report invalid parameter values with dig1-specific constraints
     - Warn about unsupported parameters for dig1 hardware
     - Validate parameter ranges against dig1 device tree definitions

#### Files to Create:
- `include/Dig1Data.hpp`
- `src/Dig1Data.cpp`
- `include/RawToDig1.hpp`
- `src/RawToDig1.cpp`
- `include/Digitizer1.hpp`
- `src/Digitizer1.cpp`

### Phase 3: Integration & Testing (Medium Priority)

#### Tasks:
1. **Configuration System Updates**
   - Ensure ConfigurationManager works with both types
   - Add dig1-specific parameter validation
   - Update parameter mapping for dig1 device trees

2. **Testing & Validation**
   - Create test configurations for both dig1 and dig2
   - Test automatic type detection
   - Verify EventData output consistency
   - Performance testing with both types

3. **Documentation Updates**
   - Update README with multi-digitizer support
   - Add configuration examples for both types
   - Document URL schemes and type detection

#### Files to Update:
- `README.md`
- Configuration files (examples)
- Test configurations

### Phase 4: Advanced Features (Low Priority)

#### Tasks:
1. **Performance Optimizations**
   - Optimize memory usage for large data sets
   - Tune threading for different digitizer types
   - Add configurable buffer sizes

2. **Extended Type Support**
   - Add support for PHA1, QDC1, SCOPE1 variants
   - Implement type-specific optimizations
   - Add advanced configuration options

3. **Monitoring & Diagnostics**
   - Add performance metrics collection
   - Implement health checking for each type
   - Add debugging and diagnostic tools

## Type Detection Strategy

### URL-Based Detection (Primary)
```
dig1://caen.internal/usb?link_num=0    → Digitizer1
dig2://192.168.1.100:8080             → Digitizer2
usb://0                                → Auto-detect from device tree
eth://192.168.1.100                    → Auto-detect from device tree
```

### Device Tree Analysis (Fallback)
```cpp
DigitizerType DetectFromDeviceTree(const nlohmann::json& deviceTree) {
    std::string fwType = deviceTree["par"]["fwtype"]["value"];
    std::string modelName = deviceTree["par"]["modelname"]["value"];
    
    if (fwType.find("dpp-psd") != std::string::npos) return DigitizerType::PSD1;
    if (fwType.find("dpp_psd") != std::string::npos) return DigitizerType::PSD2;
    // ... more detection logic
}
```

### Configuration File Support
```ini
# dig1.conf
URL dig1://caen.internal/usb?link_num=0
Type PSD1  # Optional explicit type override

# dig2.conf  
URL dig2://192.168.1.100:8080
Type PSD2  # Optional explicit type override
```

## Parameter Validation Strategy

### Unified Validation Approach
Both dig1 and dig2 will use the same parameter validation system:

```cpp
// Common validation workflow for both digitizer types
1. Hardware Connection → Device Tree Retrieval
2. Device Tree → ParameterValidator Creation  
3. Configuration File → Parameter Validation
4. Validation Results → Error/Warning Reporting
```

### Dig1 Parameter Validation Implementation
```cpp
class Digitizer1 : public IDigitizer {
    bool ValidateParameters() {
        if (!fParameterValidator) {
            std::cerr << "Parameter validator not initialized for dig1" << std::endl;
            return false;
        }
        
        // Use same validation system as dig2
        auto validationSummary = fParameterValidator->ValidateParameters(fConfig);
        return validationSummary.invalidParameters == 0;
    }
    
private:
    // Device tree management (same as dig2)
    std::string GetDeviceTree();                    // Fetch from dig1 hardware
    void DetermineDigitizerType();                  // Analyze dig1 firmware type
    bool InitializeParameterValidator();            // Create validator with dig1 tree
};
```

### Device Tree Structure Comparison
```
Dig2 Device Tree: /par/fwtype → "dpp_psd", /par/modelname → "V2740"
Dig1 Device Tree: /par/fwtype → "dpp-psd", /par/modelname → "V2750"

Both support similar parameter paths:
- Channel parameters: /ch/{n}/par/{parameter_name}
- Global parameters: /par/{parameter_name}
- Board parameters: /board/{parameter_name}
```

## Data Flow Comparison

### Current (Dig2 Only):
```
Raw Data → RawToPSD2 → PSD2Data_t → EventData → Application
```

### New (Multi-Digitizer):
```
Raw Data → DigitizerFactory → IDigitizer
                             ├── Digitizer1 → RawToDig1 → EventData
                             └── Digitizer2 → RawToPSD2 → EventData
```

### Parameter Validation Flow:
```
Configuration File → ConfigurationManager → Digitizer1/2 → ParameterValidator
                                                          ↓
Device Tree (dig1/dig2) ← Hardware Connection ← Validation Results
```

## Benefits

### Technical Benefits
- **Extensibility**: Easy to add new digitizer types
- **Maintainability**: Clear separation of concerns
- **Type Safety**: Compile-time interface enforcement
- **Performance**: Type-specific optimizations possible
- **Testing**: Each type can be tested independently

### User Benefits
- **Unified API**: Same interface for all digitizer types
- **Automatic Detection**: No need to specify digitizer type manually
- **Backward Compatibility**: Existing configurations continue to work
- **Flexibility**: Support for mixed digitizer setups

## Risk Mitigation

### Compatibility Risks
- **Mitigation**: Extensive testing with existing configurations
- **Rollback Plan**: Keep original Digitizer class as Digitizer2

### Performance Risks
- **Mitigation**: Benchmark before/after implementation
- **Optimization**: Profile and optimize critical paths

### Complexity Risks
- **Mitigation**: Phased implementation with testing at each stage
- **Documentation**: Comprehensive documentation and examples

## Success Criteria

### Phase 1 Success ✅ COMPLETED
- [x] Factory pattern implemented and working
- [x] Existing dig2 functionality preserved  
- [x] No performance regression
- [x] Build system updated and working
- [x] Backward compatibility maintained

### Phase 2 Success
- [ ] Dig1 digitizer fully functional
- [ ] Type detection working correctly  
- [ ] EventData output consistent between dig1/dig2
- [ ] **Parameter validation system integrated for dig1**
  - [ ] Device tree fetching from dig1 hardware working
  - [ ] ParameterValidator creation with dig1 device tree successful
  - [ ] Configuration parameter validation against dig1 constraints
  - [ ] Error reporting for invalid dig1 parameters
- [ ] New test suite for dig1 passing

### Phase 3 Success
- [ ] Complete integration testing passed
- [ ] Documentation updated and comprehensive
- [ ] Performance meets or exceeds current implementation
- [ ] Ready for production deployment

## Timeline Estimate

- **Phase 1**: ✅ COMPLETED (Interface & Factory)
- **Phase 2**: 2-3 weeks (Dig1 Implementation)  
- **Phase 3**: 1 week (Integration & Testing)
- **Phase 4**: 2-3 weeks (Advanced Features - optional)

**Remaining Estimated Time**: 3-4 weeks for core functionality (Phases 2-3)

## Phase 1 Implementation Summary ✅

### What Was Accomplished
- **Interface-Based Architecture**: Created `IDigitizer` interface defining all digitizer operations
- **Factory Pattern**: Implemented `DigitizerFactory` with URL parsing and device tree analysis
- **Backward Compatibility**: Original `Digitizer` class now acts as a wrapper using the factory internally
- **Type Detection**: Support for `dig1://` and `dig2://` URL schemes plus firmware analysis
- **Clean Separation**: `Digitizer2` contains the original dig2 implementation inheriting from `IDigitizer`

### Key Benefits Achieved
- **Zero Breaking Changes**: All existing code continues to work unchanged
- **Extensible Design**: Ready to add dig1 support without affecting dig2 functionality  
- **Type Safety**: Compile-time interface enforcement through pure virtual methods
- **Automatic Detection**: Smart digitizer type detection from URLs and device trees
- **Build System**: Seamless integration with existing CMake configuration

### Current State
- ✅ **Build System**: All files compile successfully with no errors
- ✅ **API Compatibility**: Existing main.cpp works without modifications
- ✅ **Type Safety**: Interface contract enforced at compile time
- ✅ **Factory Ready**: Infrastructure in place for dig1 implementation

### Next Steps (Phase 2)
The foundation is now complete for implementing dig1 support:
1. Create `Dig1Data_t` structure for dig1-specific data
2. Implement `RawToDig1` converter class
3. Create `Digitizer1` class inheriting from `IDigitizer`
4. Update factory to instantiate `Digitizer1` for dig1 types

## Conclusion

This plan provides a solid foundation for extending the digitizer library to support multiple types while maintaining backward compatibility and providing a clean, extensible architecture for future development.
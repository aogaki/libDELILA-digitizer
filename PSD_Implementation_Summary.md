# PSD-Specific Implementation Summary

## Overview
This document summarizes the PSD (Pulse Shape Discrimination) implementation for endpoint setting and data fetching, comparing the old DigiCon code with the current Digitizer1.cpp implementation.

## Endpoint Setting (PSD)

### Old Code Implementation
**Location**: `DigiCon/src/TDigitizer.cpp:188-196`

```cpp
// Get endpoint handle based on firmware type
if (fFW == "DPP-PSD")
    err = CAEN_FELib_GetHandle(fHandle, "/endpoint/DPPPSD", &fReadDataHandle);

// Set data format using JSON configuration
nlohmann::json readDataJSON = fParameters["readout_data_format"];
std::string readData = readDataJSON.dump();
err = CAEN_FELib_SetReadDataFormat(fReadDataHandle, readData.c_str());
```

### Current Code Implementation
**Location**: `src/Digitizer1.cpp:387-471`

```cpp
// Determine endpoint name from firmware type
std::string fwType = fDeviceTree["par"]["fwtype"]["value"];
fEndpointName = DeriveEndpointName(fwType);  // "dpp-psd" → "DPPPSD"

// Initialize endpoint
std::string endpointPath = "/endpoint/" + fEndpointName;
CAEN_FELib_GetHandle(fHandle, endpointPath.c_str(), &fEndpointHandle);

// Set active endpoint
CAEN_FELib_SetValue(epFolderHandle, "/par/activeendpoint", fEndpointName.c_str());

// Configure data format
auto readDataFormat = GetReadDataFormat();
std::string formatStr = readDataFormat.dump();
CAEN_FELib_SetReadDataFormat(fEndpointHandle, formatStr.c_str());
```

**Key Improvements**:
- Automatic endpoint name derivation from firmware type
- Active endpoint configuration
- Structured JSON data format generation

## Data Fetching (PSD)

### Old Code Implementation
**Location**: `DigiCon/src/TDigitizer.cpp:199-237`

```cpp
void TDigitizer::FetchEventsPSD() {
    // Single-threaded continuous loop
    while (fRunning) {
        auto err = CAEN_FELib_ReadData(
            fReadDataHandle, fTimeOut, 
            &eventData.channel, &eventData.timeStamp, &eventData.timeStampNs, 
            &eventData.energy, &eventData.energyShort, &eventData.flags,
            eventData.analogProbe1.data(), &eventData.analogProbe1Type,
            eventData.analogProbe2.data(), &eventData.analogProbe2Type,
            eventData.digitalProbe1.data(), &eventData.digitalProbe1Type,
            eventData.digitalProbe2.data(), &eventData.digitalProbe2Type,
            &eventData.waveformSize, &eventData.eventSize);
        
        // Buffer management with threshold (1023 events)
        if (eventBuffer.size() > fEventThreshold || err != CAEN_FELib_Success) {
            std::lock_guard<std::mutex> lock(fEventsDataMutex);
            fEventsVec->insert(fEventsVec->end(),
                             std::make_move_iterator(eventBuffer.begin()),
                             std::make_move_iterator(eventBuffer.end()));
            eventBuffer.clear();
        }
    }
}
```

### Current Code Implementation
**Location**: `src/Digitizer1.cpp:509-603`

```cpp
void Digitizer1::ReadDataThread() {
    // Multi-threaded approach
    while (fDataTakingFlag) {
        // Check data availability
        if (CAEN_FELib_HasData(fEndpointHandle, 100) == CAEN_FELib_Success) {
            auto eventData = CreateEventDataFromReadData();
            if (eventData) {
                std::lock_guard<std::mutex> lock(fEventDataMutex);
                if (fEventDataVec) {
                    fEventDataVec->push_back(std::move(eventData));
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

std::unique_ptr<EventData> Digitizer1::CreateEventDataFromReadData() {
    // Same CAEN_FELib_ReadData signature for PSD
    auto err = CAEN_FELib_ReadData(
        fReadDataHandle, fTimeOut, 
        &channel, &timestamp, &timestampNs, &energy, &energyShort, &flags,
        analogProbe1Data.data(), &analogProbe1Type,
        analogProbe2Data.data(), &analogProbe2Type,
        digitalProbe1Data.data(), &digitalProbe1Type,
        digitalProbe2Data.data(), &digitalProbe2Type,
        &waveformSize, &eventSize);
    
    // Create and populate EventData object
    eventData->SetChannel(channel);
    eventData->SetTimeStampNs(timestampNs);
    eventData->SetEnergy(energy);
    eventData->SetEnergyShort(energyShort);
    // ... additional data setting
}
```

**Key Improvements**:
- Multi-threaded data acquisition (configurable thread count)
- Data availability checking before reading
- Automatic EventData object creation
- Better memory management with unique_ptr

## PSD-Specific Data Fields

### Core PSD Data Structure
- **channel**: Channel number (uint8_t)
- **timestamp**: Event timestamp (uint64_t)
- **timestampNs**: Nanosecond timestamp (double)
- **energy**: Long gate energy (uint16_t)
- **energyShort**: Short gate energy (int16_t) - PSD-specific dual energy
- **flags**: Event flags (uint32_t)
- **analogProbe1/2**: Analog waveform data (int16_t arrays)
- **digitalProbe1/2**: Digital waveform data (uint8_t arrays)
- **waveformSize**: Size of waveform data (size_t)
- **eventSize**: Total event size (uint32_t)

### PSD Data Format JSON
```json
[
  {"name": "CHANNEL", "type": "U8", "dim": 0},
  {"name": "TIMESTAMP", "type": "U64", "dim": 0},
  {"name": "TIMESTAMP_NS", "type": "DOUBLE", "dim": 0},
  {"name": "ENERGY", "type": "U16", "dim": 0},
  {"name": "ENERGY_SHORT", "type": "I16", "dim": 0},
  {"name": "FLAGS", "type": "U32", "dim": 0},
  {"name": "ANALOG_PROBE_1", "type": "I16", "dim": 1},
  {"name": "ANALOG_PROBE_1_TYPE", "type": "I32", "dim": 0},
  {"name": "ANALOG_PROBE_2", "type": "I16", "dim": 1},
  {"name": "ANALOG_PROBE_2_TYPE", "type": "I32", "dim": 0},
  {"name": "DIGITAL_PROBE_1", "type": "U8", "dim": 1},
  {"name": "DIGITAL_PROBE_1_TYPE", "type": "I32", "dim": 0},
  {"name": "DIGITAL_PROBE_2", "type": "U8", "dim": 1},
  {"name": "DIGITAL_PROBE_2_TYPE", "type": "I32", "dim": 0},
  {"name": "WAVEFORM_SIZE", "type": "SIZE_T", "dim": 0},
  {"name": "EVENT_SIZE", "type": "U32", "dim": 0}
]
```

## Configuration Examples

### Connection URL
```
"URL": "dig1://caen.internal/usb?link_num=0"
```

### Firmware Type Detection
```cpp
if (fwType.find("dpp-psd") != std::string::npos) {
    fDigitizerType = DigitizerType::PSD1;
}
```

### Endpoint Path
```
"/endpoint/DPPPSD"
```

## Key Differences: Old vs Current

| Aspect | Old Code | Current Code |
|--------|----------|-------------|
| Threading | Single thread | Multi-threaded (configurable) |
| Data Format | JSON from parameters | Programmatic JSON generation |
| Endpoint Setup | Direct handle get | Active endpoint configuration |
| Data Buffering | Threshold-based (1023) | Immediate processing |
| Error Handling | Basic error checking | Comprehensive error checking |
| Memory Management | Raw pointers | Smart pointers (unique_ptr) |
| Type Safety | Basic type handling | Strong type safety with EventData |

## Usage Summary

The current implementation maintains the same core PSD functionality as the old code but with significant improvements in:
- **Performance**: Multi-threaded data acquisition
- **Reliability**: Better error handling and memory management
- **Maintainability**: Cleaner code structure and type safety
- **Scalability**: Configurable thread count and better resource management

The PSD-specific dual energy measurement (energy and energyShort) is preserved and properly handled in both implementations.
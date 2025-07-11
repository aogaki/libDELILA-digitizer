#include <CAEN_FELib.h>
#include <TApplication.h>
#include <TCanvas.h>
#include <TGraph.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

#include "ConfigurationManager.hpp"
#include "Digitizer.hpp"

// Using namespace declaration for DELILA::Digitizer
using namespace DELILA::Digitizer;

enum class AppState { Quit, Reload, Continue };

// ============================================================================
// Application Functions
// ============================================================================

bool InitializeApplication(int argc, char** argv, ConfigurationManager& config, 
                          Digitizer& digitizer);
void SaveDeviceTree(const Digitizer& digitizer, const std::string& configPath);
std::string DetermineDeviceTreeFilename(const std::string& configPath);
void RunAcquisitionLoop(Digitizer& digitizer);
void PrintFinalStatistics(double eventCounter, 
                         std::chrono::system_clock::time_point startTime);
bool ParseCommandLine(int argc, char** argv, std::string& configFile);
bool LoadConfiguration(const std::string& configFile, ConfigurationManager& config);
bool SetupDigitizer(const ConfigurationManager& config, Digitizer& digitizer);

AppState InputCheck()
{
  struct termios oldt, newt;
  char ch = -1;
  int oldf;

  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

  ch = getchar();

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);

  if (ch == 'q' || ch == 'Q') {
    return AppState::Quit;
  } else if (ch == 'r' || ch == 'R') {
    return AppState::Reload;
  }

  return AppState::Continue;
}

int main(int argc, char **argv)
{
  TApplication app("app", 0, nullptr);

  ConfigurationManager config;
  Digitizer digitizer;
  
  // Initialize application
  if (!InitializeApplication(argc, argv, config, digitizer)) {
    return 1;
  }
  
  // Save device tree
  SaveDeviceTree(digitizer, argv[1]);
  
  // Run the main acquisition loop
  RunAcquisitionLoop(digitizer);
  
  // Stop acquisition
  std::cout << "\nStopping acquisition..." << std::endl;
  if (!digitizer.StopAcquisition()) {
    std::cerr << "Failed to stop acquisition" << std::endl;
  } else {
    std::cout << "Acquisition stopped successfully!" << std::endl;
  }

  std::cout << "Exiting..." << std::endl;
  return 0;
}

// ============================================================================
// Implementation of Application Functions
// ============================================================================

bool ParseCommandLine(int argc, char** argv, std::string& configFile)
{
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
    std::cerr << "Example: " << argv[0] << " dig1.conf" << std::endl;
    return false;
  }
  
  configFile = argv[1];
  return true;
}

bool LoadConfiguration(const std::string& configFile, ConfigurationManager& config)
{
  auto loadResult = config.LoadFromFile(configFile);
  if (loadResult != ConfigurationManager::LoadResult::Success) {
    std::cerr << "Failed to load configuration file: " << configFile << std::endl;
    std::cerr << "Error: " << config.GetLastError() << std::endl;
    return false;
  }
  return true;
}

bool SetupDigitizer(const ConfigurationManager& config, Digitizer& digitizer)
{
  // Initialize digitizer
  if (!digitizer.Initialize(config)) {
    std::cerr << "Failed to initialize digitizer" << std::endl;
    return false;
  }
  std::cout << "Digitizer initialized successfully!" << std::endl;
  
  // Print device information
  digitizer.PrintDeviceInfo();
  
  // Configure the digitizer
  std::cout << "\nConfiguring digitizer..." << std::endl;
  if (!digitizer.Configure()) {
    std::cerr << "Failed to configure digitizer" << std::endl;
    return false;
  }
  std::cout << "Digitizer configured successfully!" << std::endl;
  
  // Start acquisition
  std::cout << "\nStarting acquisition..." << std::endl;
  if (!digitizer.StartAcquisition()) {
    std::cerr << "Failed to start acquisition" << std::endl;
    return false;
  }
  std::cout << "Acquisition started successfully!" << std::endl;
  
  return true;
}

bool InitializeApplication(int argc, char** argv, ConfigurationManager& config, 
                          Digitizer& digitizer)
{
  std::string configFile;
  
  // Parse command line arguments
  if (!ParseCommandLine(argc, argv, configFile)) {
    return false;
  }
  
  // Load configuration
  if (!LoadConfiguration(configFile, config)) {
    return false;
  }
  
  // Setup digitizer
  return SetupDigitizer(config, digitizer);
}

std::string DetermineDeviceTreeFilename(const std::string& configPath)
{
  if (configPath.find("dig1") != std::string::npos) {
    return "devTree1.json";
  } else if (configPath.find("dig2") != std::string::npos) {
    return "devTree2.json";
  } else {
    return "devTree.json";  // Default name
  }
}

void SaveDeviceTree(const Digitizer& digitizer, const std::string& configPath)
{
  const auto& deviceTree = digitizer.GetDeviceTreeJSON();
  if (deviceTree.empty()) {
    return;
  }
  
  std::string jsonFilename = DetermineDeviceTreeFilename(configPath);
  
  std::ofstream jsonFile(jsonFilename);
  if (jsonFile.is_open()) {
    jsonFile << deviceTree.dump(2);  // 2 spaces indentation
    jsonFile.close();
    std::cout << "Device tree saved to " << jsonFilename << std::endl;
  } else {
    std::cerr << "Failed to save device tree to " << jsonFilename << std::endl;
  }
}

void RunAcquisitionLoop(Digitizer& digitizer)
{
  std::cout << "\nPress 'q' to quit, 'r' to reload" << std::endl;
  
  double eventCounter = 0;
  auto startTime = std::chrono::system_clock::now();
  
  while (true) {
    auto state = InputCheck();
    if (state == AppState::Quit) {
      break;
    }
    
    auto eventData = digitizer.GetEventData();
    if (eventData->size() > 0) {
      eventCounter += eventData->size();
      std::cout << eventData->back()->GetTimeStampNs() << " Received "
                << eventData->size()
                << " events (Total: " << static_cast<size_t>(eventCounter) << ")"
                << std::endl;
    } else {
      // Sleep when no data is available to reduce CPU usage
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
  
  PrintFinalStatistics(eventCounter, startTime);
}

void PrintFinalStatistics(double eventCounter, 
                         std::chrono::system_clock::time_point startTime)
{
  auto endTime = std::chrono::system_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
  auto eventRate = static_cast<double>(eventCounter) / duration * 1000;
  
  std::cout << "\n=== FINAL STATISTICS ===" << std::endl;
  std::cout << "Total duration: " << std::fixed << std::setprecision(3)
            << duration / 1000.0 << " seconds" << std::endl;
  std::cout << "Total events: " << static_cast<size_t>(eventCounter) << std::endl;
  std::cout << "Average event rate: " << std::fixed << std::setprecision(1)
            << eventRate << " Hz" << std::endl;
  
  if (eventCounter > 0) {
    std::cout << "Average time per event: " << std::fixed
              << std::setprecision(3) << (duration / eventCounter) << " ms"
              << std::endl;
  }
  
  std::cout << "=======================" << std::endl;
}

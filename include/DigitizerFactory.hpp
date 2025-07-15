#ifndef DIGITIZERFACTORY_HPP
#define DIGITIZERFACTORY_HPP

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "ConfigurationManager.hpp"
#include "IDigitizer.hpp"

namespace DELILA
{
namespace Digitizer
{

class DigitizerFactory {
public:
    static std::unique_ptr<IDigitizer> CreateDigitizer(const ConfigurationManager& config);
    static DigitizerType DetectDigitizerType(const std::string& url);
    static DigitizerType DetectFromDeviceTree(const nlohmann::json& deviceTree);
    
private:
    static DigitizerType ParseURL(const std::string& url);
    static DigitizerType AnalyzeFirmware(const std::string& fwType, const std::string& modelName);
};

}  // namespace Digitizer
}  // namespace DELILA

#endif  // DIGITIZERFACTORY_HPP
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace sdbusplus
{
namespace bus
{
class bus;
} // namespace bus
} // namespace sdbusplus

namespace sdeventplus
{
class Event;
} // namespace sdeventplus

namespace functions
{
namespace process_hostfirmware
{
using ErrorCallbackType =
    std::function<void(const std::filesystem::path&, std::error_code&)>;
using LinkCallbackType =
    std::function<void(const std::filesystem::path&,
                       const std::filesystem::path&, const ErrorCallbackType&)>;
using MaybeCallCallbackType =
    std::function<void(const std::vector<std::string>&)>;
bool getExtensionsForIbmCompatibleSystem(
    const std::map<std::string, std::vector<std::string>>&,
    const std::vector<std::string>&, std::vector<std::string>&);
void writeLink(const std::filesystem::path&, const std::filesystem::path&,
               const ErrorCallbackType&);
void findLinks(const std::filesystem::path&, const std::vector<std::string>&,
               const ErrorCallbackType&, const LinkCallbackType&);
bool maybeCall(
    const std::map<
        std::string,
        std::map<std::string, std::variant<std::vector<std::string>>>>&,
    const MaybeCallCallbackType&);
std::shared_ptr<void> processHostFirmware(
    sdbusplus::bus::bus&, std::map<std::string, std::vector<std::string>>,
    std::filesystem::path, ErrorCallbackType, sdeventplus::Event&);
} // namespace process_hostfirmware
} // namespace functions

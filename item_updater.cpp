#include "config.h"
#include "item_updater.hpp"

namespace openpower
{
namespace software
{
namespace manager
{

int ItemUpdater::createActivation(sd_bus_message* msg,
                                  void* userData,
                                  sd_bus_error* retErr)
{
    auto versionId = 1;
    auto* updater = static_cast<ItemUpdater*>(userData);
    updater->activations.insert(std::make_pair(
            versionId,
            std::make_unique<Activation>(
                    updater->busItem,
                    OBJ_ACTIVATION)));
    return 0;
}

} // namespace manager
} // namespace software
} // namespace openpower


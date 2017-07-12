#include "item_updater.hpp"

namespace openpower
{
namespace software
{
namespace updater
{

void Version::delete_()
{
    parent->erase(version());
}

} // namespace updater
} // namespace software
} // namespace openpower

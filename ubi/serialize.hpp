#pragma once

#include <string>

namespace openpower
{
namespace software
{
namespace updater
{

/** @brief Serialization function - stores activation information to file
 *  @param[in] versionId - The version for which to store information.
 *  @param[in] priority - RedundancyPriority value for that version.
 */
void storeToFile(const std::string& versionId, uint8_t priority);

/** @brief Serialization function - restores activation information from file
 *  @param[in] versionId - The version for which to retrieve information.
 *  @param[in] priority - RedundancyPriority pointer for that version.
 *  @return true if restore was successful, false if not
 */
bool restoreFromFile(const std::string& versionId, uint8_t& priority);

/** @brief Removes the serial file for a given version.
 *  @param[in] versionId - The version for which to remove a file, if it exists.
 */
void removeFile(const std::string& versionId);

} // namespace updater
} // namespace software
} // namespace openpower

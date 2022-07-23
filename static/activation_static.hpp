#pragma once

#include "activation.hpp"

#include <filesystem>
#include <string>

namespace openpower
{
namespace software
{
namespace updater
{

namespace fs = std::filesystem;

/** @class ActivationStatic
 *  @brief Implementation for static PNOR layout
 */
class ActivationStatic : public Activation
{
  public:
    using Activation::Activation;
    ~ActivationStatic() = default;
    Activations activation(Activations value) override;

  private:
    void unitStateChange(sdbusplus::message_t& msg) override;
    void startActivation() override;
    void finishActivation() override;

    std::string pnorUpdateUnit;

    fs::path pnorFilePath;
};

} // namespace updater
} // namespace software
} // namespace openpower

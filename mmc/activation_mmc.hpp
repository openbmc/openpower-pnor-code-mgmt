#pragma once

#include "activation.hpp"

namespace openpower
{
namespace software
{
namespace updater
{

namespace fs = std::filesystem;

/** @class ActivationMMC
 *  @brief Implementation for eMMC PNOR layout
 */
class ActivationMMC : public Activation
{
  public:
    using Activation::Activation;
    ~ActivationMMC() = default;
    Activations activation(Activations value) override;

  private:
    void unitStateChange(sdbusplus::message_t& msg) override;
    void startActivation() override;
    void finishActivation() override;
};

} // namespace updater
} // namespace software
} // namespace openpower

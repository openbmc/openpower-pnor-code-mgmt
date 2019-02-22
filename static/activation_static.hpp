#pragma once

#include "activation.hpp"

namespace openpower
{
namespace software
{
namespace updater
{

/** @class ActivationStatic
 *  @brief Implementation for static PNOR layout
 */
class ActivationStatic : public Activation
{
  public:
    using Activation::Activation;
    ~ActivationStatic() = default;

  private:
    void unitStateChange(sdbusplus::message::message& msg) override;
    void startActivation() override;
    void finishActivation() override;
};

} // namespace updater
} // namespace software
} // namespace openpower

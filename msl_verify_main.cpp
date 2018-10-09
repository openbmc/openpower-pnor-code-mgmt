#include "config.h"

#include "msl_verify.hpp"

int main(int argc, char* argv[])
{
    using MinimumShipLevel = openpower::software::image::MinimumShipLevel;
    MinimumShipLevel minimumShipLevel(PNOR_MSL);

    if (!minimumShipLevel.verify())
    {
        // TODO Create error log
    }

    return 0;
}

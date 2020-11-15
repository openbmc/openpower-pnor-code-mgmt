# openpower-pnor-code-mgmt
OpenPower PNOR (Processor NOR) Code Management provides a set of host software
management applications for OpenPower systems. The host firmware is stored on
the PNOR chip.
More information can be found at
[Software Architecture](https://github.com/openbmc/phosphor-dbus-interfaces/blob/master/xyz/openbmc_project/Software/README.md)
or
[Host Code Update](https://github.com/openbmc/docs/blob/master/code-update/host-code-update.md)

## To Build

openpower-pnor-code-mgmt is built using meson.

```
meson build
cd build
ninja
ninja test
ninja install

To clean the repository run `rm -rf build`.
```

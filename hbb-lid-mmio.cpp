
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

#include <exception>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>

constexpr auto DEVICE_PATH = "/dev/bmc-device0";
constexpr auto DEVICE_OFFSET = 0x3D00000;
// constexpr auto DEVICE_SIZE = 64 * 1024 * 1024;
constexpr auto LID_PATH = "/var/lib/phosphor-software-manager/pnor/prsv/HBB";

/**
 * @brief Safe wrapper around mmap syscall
 *
 * MMIO provide RAII managed safe wrapper to fixed side memory
 * regions to perform read and write operating on that region.
 *
 *
 * @details
 * MMIO take **ownership** of provided region and responsible for unmapping the
 * region.
 * MMIO can't be copied or assigned.
 *
 */
class MMIO
{
  public:
    MMIO(uintptr_t address, size_t size) : address{address}, size{size} {}

    MMIO(const MMIO&) = delete;

    MMIO& operator=(const MMIO&) = delete;

    ~MMIO()
    {
        if (address != 0)
        {
            munmap(reinterpret_cast<void*>(address), size);
        }
    }

    /**
     * @brief write the data of file to MMIO region.
     *
     * @details
     * Size of the file should be exactly same with the MMIO region size.
     * file is expected the be binay and have read permission.
     *
     * @param path path to input file
     */
    auto write_file(const std::filesystem::path& path) -> void
    {
        const auto file_size = std::filesystem::file_size(path);
        if (file_size != size)
        {
            throw std::runtime_error(
                std::format("MMIO size({}) != file_size({})", size, file_size));
        }

        std::ifstream reader(path, std::ios_base::binary);
        if (!reader.good())
        {
            throw std::system_error(errno, std::system_category());
        }

        reader.read(reinterpret_cast<char*>(address), file_size * sizeof(char));
    }

    auto dump(const std::filesystem::path& path) -> void
    {
        std::ofstream writer(path, std::ios_base::binary);
        if (!writer.is_open())
        {
            throw std::system_error(errno, std::system_category());
        }
        writer.write(reinterpret_cast<const char*>(address), size);
    }

  private:
    uintptr_t address{0};
    size_t size{0};
};

/**
 * @brief Safe wrapper to manage MMIO devices
 *
 * Device provide RAII managed wrapper to device file descriptor to manage and
 * access MMIO regions for the file descriptor
 *
 * @details
 * Device optionally take the ownership of file descriptor but can be managed
 * manually. Device can't be copied or assigned.
 *
 */
class Device
{
  public:
    Device(int fd, bool managed = false) : fd{fd}, managed{managed} {}

    Device(const Device&) = delete;

    Device& operator=(const Device&) = delete;

    ~Device()
    {
        if (fd != -1 && managed)
        {
            close(fd);
        }
    }

    /**
     * @brief Get the MMIO region of device node.
     *
     * @param offset offset of MMIO region
     * @param size size of MMIO region
     * @return MMIO managed MMIO object
     */
    auto get_region(off_t offset, size_t size) -> MMIO
    {
        auto mem =
            mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
        if (mem == nullptr)
        {
            throw std::system_error(errno, std::system_category());
        }
        return MMIO(reinterpret_cast<uintptr_t>(mem), size);
    }

    /**
     * @brief open the managed device node in read+write permission
     *
     * @param p path to device file descriptor
     * @return Device managed Device object
     */
    static auto open_path(const std::filesystem::path& p) -> Device
    {
        auto fd = open(p.c_str(), O_RDWR | O_CLOEXEC);
        if (fd == -1)
        {
            throw std::system_error(errno, std::system_category());
        }
        return Device(fd, true);
    }

  private:
    int fd{-1};
    bool managed{false};
};

int main(int, char**)
{
    auto bmc_device = Device::open_path(DEVICE_PATH);

    const auto lid_size = std::filesystem::file_size(LID_PATH);

    auto memory = bmc_device.get_region(DEVICE_OFFSET, lid_size);
    memory.write_file(LID_PATH);

    return 0;
}

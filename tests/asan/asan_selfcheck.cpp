// Deliberate heap overflow: CTest passes only if AddressSanitizer reports it.
#include <cstddef>
#include <memory>

int main(int argc, char *[])
{
    const std::size_t size = static_cast<std::size_t>(argc) + 3;
    const auto buffer = std::make_unique<char[]>(size);
    return buffer[size + 1];
}

#include <cstdint>
#include <iostream>
#include <string>

namespace {

std::string yes_no(bool value) { return value ? "true" : "false"; }

std::uint64_t tiny_vector_checksum() {
    std::uint64_t checksum = 0;
    for (std::uint64_t i = 0; i < 128; ++i) {
        checksum += (i * 1315423911ULL) ^ (i << (i % 13));
    }
    return checksum;
}

}  // namespace

int main() {
#if defined(__ANDROID__)
    const bool android = true;
#else
    const bool android = false;
#endif

#if defined(__aarch64__)
    const char* arch = "aarch64";
#elif defined(__arm__)
    const char* arch = "arm";
#elif defined(__x86_64__)
    const char* arch = "x86_64";
#else
    const char* arch = "unknown";
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    const bool neon = true;
#else
    const bool neon = false;
#endif

    std::cout << "{\n";
    std::cout << "  \"program\": \"inferlab_android_hello\",\n";
    std::cout << "  \"android\": " << yes_no(android) << ",\n";
    std::cout << "  \"arch\": \"" << arch << "\",\n";
    std::cout << "  \"neon_macro\": " << yes_no(neon) << ",\n";
    std::cout << "  \"sizeof_void_ptr\": " << sizeof(void*) << ",\n";
    std::cout << "  \"tiny_vector_checksum\": " << tiny_vector_checksum() << "\n";
    std::cout << "}\n";
    return 0;
}

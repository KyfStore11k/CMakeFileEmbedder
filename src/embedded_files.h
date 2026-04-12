#pragma once

#include <cstddef>
#include <unordered_map>
#include <string>

namespace embedded {
    struct EmbeddedFile {
        const unsigned char* data;
        size_t size;
    };
    extern std::unordered_map<std::string, EmbeddedFile> registry;
}
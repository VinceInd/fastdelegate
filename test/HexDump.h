#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <ostream>
#include <iomanip>

void HexDump(std::ostream& os, const char* data, size_t size, size_t width = 16) {

    const char* start = data;
    const char* line = start;
    const char* end = data + size;

    width = (width < 1) ? 16 : width;
    while (line < end) {

        // Address header
        os << std::hex << std::setfill('0') << std::setw(4);
        os << uint16_t(std::distance(start, line));
        os << ":";

        auto remain = std::distance(line, end);
        ssize_t numChars = (remain < ssize_t(width)) ? remain : ssize_t(width);

        // Print hex chars
        for (const char* c = line; c < line + numChars; ++c) {
            os << " " << std::setw(2) << static_cast<unsigned>(uint8_t(*c));
        }

        if (numChars < ssize_t(width)) {
            std::fill_n(std::ostream_iterator<char>(os), 3*(width-numChars), ' ');
        }

        // Print ASCII chars
        os << " ";
        for (const char* c = line; c < line + numChars; ++c) {
            os << (isprint(*c) ? *c : '.');
        }

        os << "\n";
        line += numChars;
    }
}

template <typename T>
void MakeHexDump(std::ostream& oss, const T& t) {
    HexDump(oss, reinterpret_cast<const char*>(&t), sizeof(T));
}

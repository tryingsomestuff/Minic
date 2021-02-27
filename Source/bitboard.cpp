#include "bitboard.hpp"

#include "logging.hpp"

namespace BB{

std::string showBitBoard(const BitBoard & b) {
    std::bitset<64> bs(b);
    std::stringstream ss;
    for (int j = 7; j >= 0; --j) {
        ss << "\n" << Logging::_protocolComment[Logging::ct] << "+-+-+-+-+-+-+-+-+" << std::endl << Logging::_protocolComment[Logging::ct] << "|";
        for (int i = 0; i < 8; ++i) ss << (bs[i + j * 8] ? "X" : " ") << '|';
    }
    ss << "\n" << Logging::_protocolComment[Logging::ct] << "+-+-+-+-+-+-+-+-+";
    return ss.str();
}

}
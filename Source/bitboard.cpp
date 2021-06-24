#include "bitboard.hpp"

#include "logging.hpp"

namespace BB {

std::string ToString(const BitBoard& b) {
   std::bitset<64> bs(b);
   std::stringstream ss;
   for (int j = 7; j >= 0; --j) {
      ss << "\n";
      ss << Logging::_protocolComment[Logging::ct] << "+-+-+-+-+-+-+-+-+"
         << "\n";
      ss << Logging::_protocolComment[Logging::ct] << "|";
      for (int i = 0; i < 8; ++i) ss << (bs[i + j * 8] ? "X" : " ") << '|';
   }
   ss << "\n";
   ss << Logging::_protocolComment[Logging::ct] << "+-+-+-+-+-+-+-+-+";
   return ss.str();
}

} // namespace BB
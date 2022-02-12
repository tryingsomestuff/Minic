#pragma once

#include <algorithm>
#include <chrono>
#include <iostream>
#include <list>
#include <optional>
#include <thread>
#include <unordered_set>
#include <vector>

#include "definition.hpp"
#include "hash.hpp"
#include "learn_tools.hpp"
#include "position.hpp"

#define LEARN_SFEN_READ_SIZE (1000 * 1000 * 10)

typedef std::vector<PackedSfenValue> PSVector;
struct BasicSfenInputStream {
   virtual std::optional<PackedSfenValue> next()      = 0;
   virtual bool                           eof() const = 0;
   virtual ~BasicSfenInputStream() {}
};

struct BinSfenInputStream : BasicSfenInputStream {
   static constexpr auto           openmode  = std::ios::in | std::ios::binary;
   static inline const std::string extension = "bin";

   BinSfenInputStream(std::string filename): m_stream(filename, openmode), m_eof(!m_stream) {}

   std::optional<PackedSfenValue> next() override {
      PackedSfenValue e;
      if (m_stream.read(reinterpret_cast<char*>(&e), sizeof(PackedSfenValue))) { return e; }
      else {
         m_eof = true;
         return std::nullopt;
      }
   }

   bool eof() const override { return m_eof; }

   ~BinSfenInputStream() override {}

  private:
   std::fstream m_stream;
   bool         m_eof;
};

inline std::unique_ptr<BasicSfenInputStream> open_sfen_input_file(const std::string& filename) {
   return std::make_unique<BinSfenInputStream>(filename);
}

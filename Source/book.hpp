#pragma once

#include "definition.hpp"

namespace Book {

// Read own format binary book and fill book cache with it
void initBook();
// Request a book move from a position hash (a random process is used to choose a move if many are available)
[[nodiscard]] MiniMove Get(const Hash h);

#ifdef IMPORTBOOK
// Convert own format ascii book to binary book
bool buildBook(const std::string & bookFileName);
#endif

} // Book

//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <fstream>

// ----------------------------------------------------------------------------- : File to UTF8 Encoding

inline std::string fileToUTF8(const std::string& filepath) {
  // File to char
  std::ifstream file(filepath, std::ios::binary);
  file.unsetf(std::ios::skipws);
  std::vector<unsigned char> buffer = std::vector<unsigned char>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  int size = buffer.size();
  if (size < 2) {
    queue_message(MESSAGE_WARNING, _("File too small to encode"));
    return "";
  }
  // All bytes that have a highest bit of 0 are valid UTF8 characters, so:
  // Reset the highest bit of each byte, store these bits in additional bytes at the end
  const unsigned char highest_bit = 1 << 7;
  unsigned char added_byte = 0;
  for (int i = 0, b = 0 ; i < size ; ++i, ++b) {
    if (b == 7) {                         // Never set the highest bit of the added byte
      buffer.push_back(added_byte);
      b = 0;
    }
    unsigned char bit = 1 << b;
    if ((buffer[i] & highest_bit) != 0) { // The highest bit of the buffer is set
      buffer[i] &= ~highest_bit;          // Reset the highest bit of the buffer
      added_byte |= bit;                  // Set the bit of the added byte
    } else {
      added_byte &= ~bit;                 // Reset the bit of the added byte
    }
  }
  buffer.push_back(added_byte);
  // Char to string
  return std::string(buffer.begin(), buffer.end());
}

inline bool UTF8ToFile(const std::string& filepath, std::string& string) {
  // String to char
  std::vector<unsigned char> buffer(string.begin(), string.end());
  int size = buffer.size();
  if (size < 2) {
    queue_message(MESSAGE_WARNING, _("File too small to decode"));
    return false;
  }
  // Restore the highest bit of each byte
  size = (size * 7) / 8;
  const unsigned char highest_bit = 1 << 7;
  unsigned char added_byte = buffer[size];
  for (int i = 0, j = size, b = 0 ; i < size ; ++i, ++b) {
    if (b == 7) {
      ++j;
      added_byte = buffer[j];
      b = 0;
    }
    unsigned char bit = 1 << b;
    if ((added_byte & bit) != 0) { // The bit of the added byte is set
      buffer[i] |= highest_bit;    // Set the highest bit of the buffer
    }
  }
  buffer.resize(size);
  // Char to file
  std::ofstream file(filepath, std::ios::out|std::ios::binary);
  std::copy(buffer.cbegin(), buffer.cend(), std::ostream_iterator<unsigned char>(file));
  return true;
}

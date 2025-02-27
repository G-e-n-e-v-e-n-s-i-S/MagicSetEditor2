//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <random>
#include <sstream>

// ----------------------------------------------------------------------------- : UID

static std::random_device              rd;                                              // Get true random number generator
static std::mt19937_64                 gen((static_cast<uint64_t>(rd()) << 32) ^ rd()); // Bitwise XOR two outputs to seed pseudo random number generator
static std::uniform_int_distribution<> dis(0, 9);

// Generate a string consisting of 32 uniformly random digits.
static String generate_uid() {
  std::stringstream ss;
  int i;
  ss << std::hex;
  for (i = 0; i < 32; i++) {
    ss << dis(gen);
  };
  //return ss.str();
  String wxString(ss.str().c_str(), wxConvUTF8);
  return wxString;
}

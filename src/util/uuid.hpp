//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <random>
#include <sstream>

// ----------------------------------------------------------------------------- : UUID

// This only has a 64bit space of randomness
// compared to the required 128bit for true UUID
// but it's more than enough for our purposes
namespace uuid {
   static std::random_device              rd;
   static std::mt19937_64                 gen((static_cast<uint64_t>(rd()) << 32) | rd());
   static std::uniform_int_distribution<> dis(0, 15);
   static std::uniform_int_distribution<> dis2(8, 11);

   std::string generate_uuid() {
      std::stringstream ss;
      int i;
      ss << std::hex;
      for (i = 0; i < 8; i++) {
         ss << dis(gen);
}
      ss << "-";
      for (i = 0; i < 4; i++) {
         ss << dis(gen);
      }
      ss << "-4";
      for (i = 0; i < 3; i++) {
         ss << dis(gen);
      }
      ss << "-";
      ss << dis2(gen);
      for (i = 0; i < 3; i++) {
         ss << dis(gen);
      }
      ss << "-";
      for (i = 0; i < 12; i++) {
         ss << dis(gen);
      };
      return ss.str();
   }
}

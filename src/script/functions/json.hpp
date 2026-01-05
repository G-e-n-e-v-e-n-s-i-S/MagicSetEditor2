//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <boost/json.hpp>
#include <data/set.hpp>
#include <data/pack.hpp>

// All this isn't great, but it will do for now. Idealy you would create JsonWriter and JsonReader classes
// that inherit from Writer and Reader, and just have a switch to go from normal mode to JSON mode...


// ----------------------------------------------------------------------------- : JSON to String

void pretty_print(std::ostream& os, const boost::json::value& jv, std::string* indent = nullptr);


String json_pretty_print(const boost::json::value& jv, std::string* indent = nullptr);

String json_ugly_print(const boost::json::value& jv);

// ----------------------------------------------------------------------------- : JSON to MSE

ScriptValueP json_to_mse(const boost::json::value& jv, Set* set);

template <typename T>
void read(T& out, boost::json::object& jv, const char value_name[]);

// templates don't work with enums? are you kidding me with this language?
void read(PackSelectType& out, boost::json::object& jv, const char value_name[]);

PackItemP json_to_mse_pack_item(boost::json::object& jv);

PackTypeP json_to_mse_pack_type(boost::json::object& jv);

KeywordP json_to_mse_keyword(boost::json::object& jv);

CardP json_to_mse_card(boost::json::object& jv, Set* set);

SetP json_to_mse_set(boost::json::object& jv);

ScriptValueP json_to_mse(const boost::json::value& jv, Set* set);
ScriptValueP json_to_mse(const String& string, Set* set);
ScriptValueP json_to_mse(const ScriptValueP& sv, Set* set);

// ----------------------------------------------------------------------------- : MSE to JSON

template <typename T>
void write(boost::json::object& out, const String& name, const T& value);

void write(boost::json::object& out, const String& name, IndexMap<FieldP, ValueP>& map);

void write(boost::json::object& out, const String& name, DelayedIndexMaps<FieldP,ValueP>& map);

boost::json::object mse_to_json(const PackItemP& item);

boost::json::object mse_to_json(const PackTypeP& pack);

boost::json::object mse_to_json(const KeywordP& keyword);

boost::json::object mse_to_json(const CardP& card, const Set* set);

boost::json::object mse_to_json(const Set* set);

boost::json::value mse_to_json(const ScriptValueP& sv, Set* set);

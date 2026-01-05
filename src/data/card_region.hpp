//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

#include <util/prec.hpp>
#include <util/reflect.hpp>
#include <script/scriptable.hpp>

DECLARE_POINTER_TYPE(CardRegion);

class CardRegion : public IntrusivePtrBase<CardRegion> {
public:
	CardRegion();
	virtual ~CardRegion();

	String name;
	double x, y;
	int width, height;

private:
	DECLARE_REFLECTION();
};

inline String type_name(const CardRegion&) {
	return _TYPE_("card region");
}

inline String type_name(const vector<CardRegionP>&) {
	return _TYPE_("card regions");
}

//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>

DECLARE_POINTER_TYPE(Set);

// ----------------------------------------------------------------------------- : AddCSVWindow

/// A window for selecting a subset of the cards from a set,
/** and selecting a link relation type.
/** this is used when linking cards
 */
class AddCSVWindow : public wxDialog {
public:
	AddCSVWindow(Window* parent, const SetP& set, bool sizer);

protected:
	DECLARE_EVENT_TABLE();

	wxChoice*       separator_type;
	wxTextCtrl*     file_path;
	wxButton*       file_browse;
	SetP            set;
	char            separator;

	std::vector<std::string> readCSVRow(const std::string& row);
	
	void setSeparatorType();

	void onSeparatorTypeChange(wxCommandEvent&);

	void onBrowseFiles(wxCommandEvent&);

	void onOk(wxCommandEvent&);

};

enum class CSVState {
	UnquotedField,
	QuotedField,
	QuotedQuote
};

//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/reflect.hpp>
#include <util/real_point.hpp>
#include <gui/card_select_window.hpp>
#include <data/set.hpp>
#include <data/stylesheet.hpp>

DECLARE_POINTER_TYPE(Set);
DECLARE_POINTER_TYPE(PrintJob);

// ----------------------------------------------------------------------------- : Job

class PrintJob : public IntrusivePtrBase<PrintJob> {
public:
  PrintJob(const SetP& set, const vector<CardP>& cards, double spacing) : set(set), cards(cards), spacing(spacing) {
    default_size.width  = set->stylesheet->card_width  * 25.4 / set->stylesheet->card_dpi;
    default_size.height = set->stylesheet->card_height * 25.4 / set->stylesheet->card_dpi;
  }
  
  SetP set;
  vector<CardP> cards;   ///< Cards selected by the user for print
  RealSize default_size; ///< Size of a card with the default stylesheet in millimetres
  double spacing;        ///< Spacing between cards in millimetres
  
  struct CardLayout {
    CardLayout(const CardP& card, const RealSize& size, Radians rotation)
    : card(card), size(size), rotation(rotation) {}

    bool operator<(const CardLayout& that) const {
      return size.width > that.size.width; // put the widest cards first
    }

    CardP card;
    RealSize size;
    Radians rotation;

    RealSize pos;
  };

  void init(const RealSize& page_size);

  RealSize page_size;                      ///< Size of a page in millimetres
  vector<CardLayout> card_layouts;         ///< Locations of the cards on the pages
  vector<vector<CardLayout>> page_layouts; ///< The CardLayout grouped by page

  /// Is this job uninitialized?
  inline bool empty() const { return page_layouts.empty(); }

private:
  // calculate the width and height of each card in millimeters
  void measure_cards();
  // calculate where the cards go on the pages
  void layout_cards();
  // if two cards are almost aligned, align them
  void align_cards();
  // center the cards on the middle of each page
  void center_cards();
};

// ----------------------------------------------------------------------------- : Printing

/// Make a print job, by asking the user for options, and card selection
PrintJobP make_print_job(Window* parent, const SetP& set, const ExportCardSelectionChoices& choices);

/// Show a print preview for the given set
void print_preview(Window* parent, const PrintJobP& job);
void print_preview(Window* parent, const SetP& set, const ExportCardSelectionChoices& choices);

/// Print the given set
void print_set(Window* parent, const PrintJobP& job);
void print_set(Window* parent, const SetP& set, const ExportCardSelectionChoices& choices);


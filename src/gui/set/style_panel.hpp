//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gui/set/panel.hpp>
#include <gui/control/filter_ctrl.hpp>

class CardViewer;
class PackageList;
class StylingEditor;

// ----------------------------------------------------------------------------- : StylePanel

/// A panel showing a list of stylesheets, and an editor for styling
class StylePanel : public SetWindowPanel {
public:
  StylePanel(Window* parent, int id);
  
  void onChangeSet() override;
  void onAction(const Action&, bool undone) override;
  
  // --------------------------------------------------- : UI
  
  void initUI(wxToolBar*, wxMenuBar*) override;
  
  // --------------------------------------------------- : Clipboard
  bool canCut() const override;
  bool canCopy() const override;
  bool canPaste() const override;
  bool canSelectAll() const override;
  void doCut() override;
  void doCopy() override;
  void doPaste() override;
  void doSelectAll() override;
  
  // --------------------------------------------------- : Selection
  void selectCard(const CardP& card) override;
  
private:
  DECLARE_EVENT_TABLE();
  
  CardViewer*    preview;    ///< Card preview
  PackageList*   list;    ///< List of stylesheets
  StylingEditor* editor;    ///< Editor for styling information
  wxButton*      use_for_all;
  wxCheckBox*    use_custom_options;
  FilterCtrl* stylesheet_filter;
  String stylesheet_filter_value;
  CardP          card;    ///< Card we are working on
  bool list_size_already_initialized = false;
  
  void onStyleSelect(wxCommandEvent&);
  void onUseForAll(wxCommandEvent&);
  void onUseCustom(wxCommandEvent&);

  void onStylesheetFilterUpdate(wxCommandEvent&);
  
  /// Determine the best size for the list of stylesheets based on available space
  void updateListSize();
  bool Layout() override;
  
  /// Actual intialization of the controls
  void initControls();
};


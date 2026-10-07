//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/symbol.hpp>
#include <gui/symbol/selection.hpp>
#include <render/symbol/viewer.hpp>

class SymbolWindow;
DECLARE_POINTER_TYPE(SymbolEditorBase);

// ----------------------------------------------------------------------------- : SymbolControl

/// Control for editing symbols
/** What kind of editing is done is determined by the contained SymbolEditorBase object
 *  That object handles all events and the drawing. This class is mostly just a proxy.
 */
class SymbolControl : public wxControl, public SymbolViewer {
public:
  SymbolControl(SymbolWindow* parent, int id, const SymbolP& symbol);
  
  void onChangeSymbol() override;
  
  void onAction(const Action&, bool undone) override;
  
  // Forward command to editor
  void onExtraTool(wxCommandEvent& ev);
  
  // Switch to some editing mode
  void onModeChange(wxCommandEvent& ev);
  
  /// Handle UpdateUIEvents propagated from the SymbolWindow
  /** Handles events for editing mode related stuff
   */
  void onUpdateUI(wxUpdateUIEvent& ev);
  
  /// The selection has changed, tell the part list
  void signalSelectionChange();
  
  /// Activate a part, open it in the point editor, if it is a shape
  void activatePart(const SymbolPartP& part);
  
  /// Select a specific part from the symbol
  /// The editor is switched to the select editor
  void selectPart(const SymbolPartP& part);
  
  /// Update the selection
  void onUpdateSelection();
  
  /// Are we editing?
  bool isEditing();
  
  /// Zoom in (steps > 0) or out (steps < 0), around mouse position
  bool zoomBy(int steps, const wxPoint& mouse);

  /// Current zoom level in percent, 100 means that the symbol area exactly fits the window
  inline int zoomPercent() const { return zoom_percent; }

  /// By what factor is the grid denser than settings.symbol_grid_size at the current zoom level?
  int gridDensity() const;

  enum {
    ZOOM_MIN        = 50,   ///< lowest zoom level in percent
    ZOOM_MAX        = 200,  ///< highest zoom level in percent
    ZOOM_STEP       = 10,   ///< size of one zoom step in percent
    ZOOM_DENSE_GRID = 140,  ///< from this zoom level on, the grid is twice as dense
  };

private:
  /// Switch the a different editor object
  void switchEditor(const SymbolEditorBaseP& e);
    
  /// Draw the editor
  void draw(DC& dc);
  
  /// Apply zoom_percent and view_center to the viewer
  void updateView();

  /// Size in pixels of the symbol area [0...1] at the current zoom level
  double symbolAreaSize() const;

  /// Move the view while panning
  void panTo(const wxPoint& mouse);

  /// Stop panning
  void endPan();

private:
  DECLARE_EVENT_TABLE();

  // --------------------------------------------------- : Data
  
public:
  /// What parts are selected?
  SymbolPartsSelection selected_parts;
  SymbolPartP          highlight_part;    ///< part the mouse cursor is over
  SymbolShapeP         selected_shape;    ///< if there is a single selection
  SymbolSymmetryP      selected_symmetry; ///< if there is a single selection
  
  /// Parent window 
  SymbolWindow* parent;
  
private:
  /// The current editor
  SymbolEditorBaseP editor;
  
  /// Last mouse position
  Vector2D last_pos;
  
  /// Zoom level in percent, 100 = the symbol area [0...1] exactly fits the smallest side of the window
  int zoom_percent;
  /// The point of the symbol (in symbol coordinates) that is shown in the center of the window
  Vector2D view_center;
  /// Collects mouse wheel movement until there is enough for a zoom step (for high resolution wheels)
  int wheel_accumulator;

  /// Are we panning the view (dragging with the middle mouse button)?
  bool panning;
  wxPoint  pan_start;        ///< window position of the mouse when panning started
  Vector2D pan_start_center; ///< view_center when panning started
  wxCursor pan_old_cursor;   ///< cursor to restore when panning stops

  // --------------------------------------------------- : Events

  void onLeftDown  (wxMouseEvent& ev);
  void onLeftUp    (wxMouseEvent& ev);
  void onLeftDClick(wxMouseEvent& ev);
  void onRightDown (wxMouseEvent& ev);
  void onMiddleDown(wxMouseEvent& ev);
  void onMiddleUp  (wxMouseEvent& ev);
  void onMotion    (wxMouseEvent& ev);
  void onMouseWheel(wxMouseEvent& ev);
  void onLoseCapture(wxMouseCaptureLostEvent& ev);

  void onPaint    (wxPaintEvent& ev);
  void onKeyChange(wxKeyEvent& ev);
  void onChar     (wxKeyEvent& ev);
  void onSize     (wxSizeEvent& ev);
};



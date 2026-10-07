//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gui/symbol/control.hpp>
#include <gui/symbol/window.hpp>
#include <gui/symbol/editor.hpp>
#include <gui/symbol/select_editor.hpp>
#include <gui/symbol/point_editor.hpp>
#include <gui/symbol/basic_shape_editor.hpp>
#include <gui/symbol/symmetry_editor.hpp>
#include <gui/util.hpp>
#include <data/action/symbol.hpp>
#include <data/settings.hpp>
#include <util/window_id.hpp>
#include <wx/dcbuffer.h>

// ----------------------------------------------------------------------------- : SymbolControl

SymbolControl::SymbolControl(SymbolWindow* parent, int id, const SymbolP& symbol)
  : wxControl(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_THEME)
  , SymbolViewer(symbol, true)
  , parent(parent)
  , zoom_percent(100)
  , view_center(0.5, 0.5)
  , wheel_accumulator(0)
  , panning(false)
{
  onChangeSymbol();
}

void SymbolControl::switchEditor(const SymbolEditorBaseP& e) {
  if (editor) editor->destroyUI(parent->GetToolBar(), parent->GetMenuBar());
  editor = e;
  if (editor) editor->initUI   (parent->GetToolBar(), parent->GetMenuBar());
  Refresh(false);
}

void SymbolControl::onChangeSymbol() {
  selected_parts.setSymbol(symbol);
  switchEditor(make_intrusive<SymbolSelectEditor>(this, false));
  zoom_percent      = 100;
  view_center       = Vector2D(0.5, 0.5);
  wheel_accumulator = 0;
  updateView();
  Refresh(false);
}

void SymbolControl::onModeChange(wxCommandEvent& ev) {
  switch (ev.GetId()) {
    case ID_MODE_SELECT:
      switchEditor(make_intrusive<SymbolSelectEditor>(this, false));
      break;
    case ID_MODE_ROTATE:
      switchEditor(make_intrusive<SymbolSelectEditor>(this, true));
      break;
    case ID_MODE_POINTS:
      if (selected_parts.size() == 1) {
        selected_shape = selected_parts.getAShape();
        if (selected_shape) {
          switchEditor(make_intrusive<SymbolPointEditor>(this, selected_shape));
        }
      }
      break;
    case ID_MODE_SHAPES:
      if (!selected_parts.empty()) {
        selected_parts.clear();
        signalSelectionChange();
      }
      switchEditor(make_intrusive<SymbolBasicShapeEditor>(this));
      break;
    case ID_MODE_SYMMETRY:
      switchEditor(make_intrusive<SymbolSymmetryEditor>(this, selected_parts.getASymmetry()));
      break;
  }
}

void SymbolControl::onExtraTool(wxCommandEvent& ev) {
  switch (ev.GetId()) {
    case ID_VIEW_GRID:
      settings.symbol_grid = !settings.symbol_grid;
      Refresh(false);
      break;
    case ID_VIEW_GRID_SNAP:
      settings.symbol_grid_snap = !settings.symbol_grid_snap;
      Refresh(false);
      break;
    default:
      if (editor) editor->onCommand(ev.GetId());
  }
}

void SymbolControl::onAction(const Action& action, bool undone) {
  TYPE_CASE_(action, SymbolPartAction) {
    Refresh(false);
  }
}

void SymbolControl::onUpdateSelection() {
  switch(editor->modeToolId()) {
    case ID_MODE_POINTS: {
      // can only select a single part!
      SymbolShapeP shape = selected_parts.getAShape();
      if (!shape) {
        if (selected_parts.select(selected_shape)) {
          signalSelectionChange();
        }
        break;
      }
      if (shape != selected_shape) {
        if (selected_parts.select(shape)) {
          signalSelectionChange();
        }
        // begin editing another part
        selected_shape = shape;
        editor = make_intrusive<SymbolPointEditor>(this, selected_shape);
        Refresh(false);
      }
      break;
    } case ID_MODE_SYMMETRY: {
      // can only select a single part!
      SymbolSymmetryP symmetry = selected_parts.getASymmetry();
      if (!symmetry) {
        if (selected_symmetry && selected_parts.select(selected_symmetry)) {
          signalSelectionChange();
        }
        break;
      }
      if (symmetry != selected_symmetry) {
        if (symmetry && selected_parts.select(symmetry)) {
          signalSelectionChange();
        }
        // begin editing another part
        selected_symmetry = symmetry;
        Refresh(false);
      }
      break;
    } case ID_MODE_SHAPES:
      if (!selected_parts.empty()) {
        // there can't be a selection
        selected_parts.clear();
        signalSelectionChange();
      }
      break;
    default:
      Refresh(false);
      break;
  }
}

void SymbolControl::selectPart(const SymbolPartP& part) {
  selected_parts.select(part);
  switchEditor(make_intrusive<SymbolSelectEditor>(this, false));
  signalSelectionChange();
}

void SymbolControl::activatePart(const SymbolPartP& part) {
  if (part->isSymbolShape()) {
    selected_parts.select(part);
    switchEditor(make_intrusive<SymbolPointEditor>(this, static_pointer_cast<SymbolShape>(part)));
  } else if (part->isSymbolSymmetry()) {
    selected_parts.select(part);
    switchEditor(make_intrusive<SymbolSymmetryEditor>(this, static_pointer_cast<SymbolSymmetry>(part)));
  }
}

void SymbolControl::signalSelectionChange() {
  parent->onSelectFromControl();
}

bool SymbolControl::isEditing() {
  return editor && editor->isEditing();
}

int SymbolControl::gridDensity() const {
  return zoom_percent >= ZOOM_DENSE_GRID ? 2 : 1;
}

// ----------------------------------------------------------------------------- : Drawing

void SymbolControl::draw(DC& dc) {
  // clear the background
  clearDC(dc, Color(0, 128, 0));
  // draw symbol iself
  SymbolViewer::draw(dc);
  // draw grid
  if (settings.symbol_grid) {
    RotatedDC rdc(dc, rotation, QUALITY_LOW);
    int density = gridDensity();
    double lines = settings.symbol_grid_size * density;
    for (int i = 0 ; i <= lines ; ++i) {
      double x = (double)i/lines;
      // the major lines stay where they were when the grid gets denser
      bool major = i % (5 * density) == 0;
      rdc.SetLogicalFunction(wxAND);
      rdc.SetPen(major ? Color(191,255,191) : Color(191, 255, 191));
      rdc.DrawLine(RealPoint(0,x), RealPoint(1,x));
      rdc.DrawLine(RealPoint(x,0), RealPoint(x,1));
      rdc.SetLogicalFunction(wxOR);
      rdc.SetPen(major ? Color(0,63,0) : Color(0, 31, 0));
      rdc.DrawLine(RealPoint(0,x), RealPoint(1,x));
      rdc.DrawLine(RealPoint(x,0), RealPoint(x,1));
    }
    dc.SetLogicalFunction(wxCOPY);
  }
  // draw aspect ratio indicators
  double ar = symbol->aspectRatio();
  // TODO: limit aspect ratio
  if (ar > 0) {
  } else if (ar < 0) {
  }
  // draw editing overlay
  if (editor) {
    editor->draw(dc);
  }
}
void SymbolControl::onPaint(wxPaintEvent&) {
  wxBufferedPaintDC dc(this);
  try {
    draw(dc);
  } CATCH_ALL_ERRORS(false); // don't show message boxes in onPaint!
}

// ----------------------------------------------------------------------------- : Events

// Mouse events, convert position, forward event

void SymbolControl::onLeftDown(wxMouseEvent& ev) {
  Vector2D pos = rotation.trInv(RealPoint(ev.GetX(), ev.GetY()));
  if (editor) editor->onLeftDown(pos, ev);
  last_pos = pos;
  ev.Skip(); // for focus
}
void SymbolControl::onLeftUp(wxMouseEvent& ev) {
  Vector2D pos = rotation.trInv(RealPoint(ev.GetX(), ev.GetY()));
  if (editor) editor->onLeftUp(pos, ev);
  last_pos = pos;
}
void SymbolControl::onLeftDClick(wxMouseEvent& ev) {
  Vector2D pos = rotation.trInv(RealPoint(ev.GetX(), ev.GetY()));
  if (editor) editor->onLeftDClick(pos, ev);
  last_pos = pos;
}
void SymbolControl::onRightDown(wxMouseEvent& ev) {
  Vector2D pos = rotation.trInv(RealPoint(ev.GetX(), ev.GetY()));
  if (editor) editor->onRightDown(pos, ev);
  last_pos = pos;
}

void SymbolControl::onMotion(wxMouseEvent& ev) {
  if (panning) {
    if (ev.MiddleIsDown()) {
      panTo(ev.GetPosition());
      return;
    }
    // we missed the release of the button
    endPan();
  }
  Vector2D pos = rotation.trInv(RealPoint(ev.GetX(), ev.GetY()));
  // Dragging something?
  if (ev.LeftIsDown()) {
    if (editor) editor->onMouseDrag(last_pos, pos, ev);
  } else {
    if (editor) editor->onMouseMove(last_pos, pos, ev);
  }
  last_pos = pos;
}

// Key events, just forward

void SymbolControl::onKeyChange(wxKeyEvent& ev) {
  if (editor) editor->onKeyChange(ev);
  ev.Skip(); // so we get char events
}
void SymbolControl::onChar(wxKeyEvent& ev) {
  if (editor) editor->onChar(ev);
  else        ev.Skip();
}

void SymbolControl::onMouseWheel(wxMouseEvent& ev) {
  // only zoom from vertical wheel motion, don't zoom while something is being dragged or panned
  if (ev.GetWheelAxis() != wxMOUSE_WHEEL_VERTICAL || ev.LeftIsDown() || panning) {
    ev.Skip();
    return;
  }
  // high resolution wheels send many small events, collect them until we have a whole step
  int wheel = ev.GetWheelRotation();
  if ((wheel > 0) != (wheel_accumulator > 0)) wheel_accumulator = 0; // changed direction
  wheel_accumulator += wheel;
  int delta = ev.GetWheelDelta() > 0 ? ev.GetWheelDelta() : 120;
  int steps = wheel_accumulator / delta;
  if (steps == 0) return;
  wheel_accumulator -= steps * delta;
  wxPoint mouse = ev.GetPosition();
  if (zoomBy(steps, mouse)) {
    // update editor hover state
    Vector2D pos = rotation.trInv(RealPoint(mouse.x, mouse.y));
    if (editor) editor->onMouseMove(last_pos, pos, ev);
    last_pos = pos;
  }
}

// ----------------------------------------------------------------------------- : Zooming

// How far the view may scroll past the edge of the symbol area, as a fraction of the size of the symbol area
static const double VIEW_OVERSCROLL = 0.3;

// Where can the top left corner of the symbol area be along one axis?
static double clamp_view_origin(double origin, double symbol_area_size, double window_size) {
  double margin = symbol_area_size * VIEW_OVERSCROLL;
  double lo = min(0., window_size - symbol_area_size) - margin;
  double hi = max(0., window_size - symbol_area_size) + margin;
  return origin < lo ? lo : origin > hi ? hi : origin;
}

double SymbolControl::symbolAreaSize() const {
  wxSize s = GetClientSize();
  return min(s.x, s.y) * zoom_percent / 100.0;
}

void SymbolControl::updateView() {
  wxSize s = GetClientSize();
  if (s.x <= 0 || s.y <= 0) return; // not laid out yet
  // size in pixels of the symbol area at this zoom level
  double zoom = symbolAreaSize();
  // pixel position of the symbol's (0,0), such that view_center is in the middle of the window
  Vector2D origin(
    s.x * 0.5 - zoom * view_center.x,
    s.y * 0.5 - zoom * view_center.y
  );
  origin.x = clamp_view_origin(origin.x, zoom, s.x);
  origin.y = clamp_view_origin(origin.y, zoom, s.y);
  // remember the (possibly limited) result, so later changes of the window size start from it
  view_center = Vector2D(
    (s.x * 0.5 - origin.x) / zoom,
    (s.y * 0.5 - origin.y) / zoom
  );
  // NOTE: setZoom must come before setOrigin
  setZoom(zoom);
  setOrigin(origin);
}

bool SymbolControl::zoomBy(int steps, const wxPoint& mouse) {
  int new_percent = zoom_percent + steps * ZOOM_STEP;
  if (new_percent < ZOOM_MIN) new_percent = ZOOM_MIN;
  if (new_percent > ZOOM_MAX) new_percent = ZOOM_MAX;
  if (new_percent == zoom_percent) return false;
  wxSize s = GetClientSize();
  if (s.x <= 0 || s.y <= 0) return false;
  // the point of the symbol that is under the mouse, this should not move
  Vector2D p = rotation.trInv(RealPoint(mouse.x, mouse.y));
  zoom_percent = new_percent;
  // The mouse position on screen is
  //   mouse = window_center - zoom * view_center + zoom * p
  // so to keep p under the mouse
  //   view_center = p - (mouse - window_center) / zoom
  double zoom = symbolAreaSize();
  view_center = Vector2D(
    p.x - (mouse.x - s.x * 0.5) / zoom,
    p.y - (mouse.y - s.y * 0.5) / zoom
  );
  updateView();
  Refresh(false);
  return true;
}

// ----------------------------------------------------------------------------- : Panning

void SymbolControl::onMiddleDown(wxMouseEvent& ev) {
  // don't pan while something is being dragged with the left button
  if (panning || ev.LeftIsDown()) {
    ev.Skip();
    return;
  }
  panning          = true;
  pan_start        = ev.GetPosition();
  pan_start_center = view_center;
  pan_old_cursor   = GetCursor();
  SetCursor(wxCursor(wxCURSOR_HAND));
  if (!HasCapture()) CaptureMouse(); // keep getting events when the mouse leaves the window
}

void SymbolControl::onMiddleUp(wxMouseEvent& ev) {
  if (!panning) {
    ev.Skip();
    return;
  }
  endPan();
  // the symbol has moved under the mouse, give the editor a chance to update its hover state
  Vector2D pos = rotation.trInv(RealPoint(ev.GetX(), ev.GetY()));
  if (editor) editor->onMouseMove(last_pos, pos, ev);
  last_pos = pos;
}

void SymbolControl::onLoseCapture(wxMouseCaptureLostEvent&) {
  // something else took the mouse (a dialog for instance), so we will not see the button being released
  endPan();
}

void SymbolControl::panTo(const wxPoint& mouse) {
  double zoom = symbolAreaSize();
  if (zoom <= 0) return;
  // drag the view so that the point of the symbol that was under the mouse when we started stays under it
  view_center = Vector2D(
    pan_start_center.x - (mouse.x - pan_start.x) / zoom,
    pan_start_center.y - (mouse.y - pan_start.y) / zoom
  );
  updateView();
  Refresh(false);
}

void SymbolControl::endPan() {
  if (!panning) return;
  panning = false;
  if (HasCapture()) ReleaseMouse();
  SetCursor(pan_old_cursor);
}

void SymbolControl::onSize(wxSizeEvent& ev) {
  updateView();
  Refresh(false);
}
void SymbolControl::onUpdateUI(wxUpdateUIEvent& ev) {
  if (!editor) return;
  switch (ev.GetId()) {
    case ID_MODE_SELECT: case ID_MODE_ROTATE: case ID_MODE_POINTS:
    case ID_MODE_SHAPES: case ID_MODE_SYMMETRY: //case ID_MODE_PAINT:
      ev.Check(editor->modeToolId() == ev.GetId());
      if (ev.GetId() == ID_MODE_POINTS) {
        // can only edit points when a shape is available
        ev.Enable((bool)selected_parts.getAShape());
      }
      if (ev.GetId() == ID_MODE_SYMMETRY) {
        ev.Enable(!selected_parts.empty());
      }
      break;
    case ID_MODE_PAINT:
      ev.Enable(false); // TODO
      break;
    case ID_VIEW_GRID:
      ev.Check(settings.symbol_grid);
      break;
    case ID_VIEW_GRID_SNAP:
      ev.Check(settings.symbol_grid_snap);
      break;
    default:
      if (ev.GetId() >= ID_CHILD_MIN && ev.GetId() < ID_CHILD_MAX) {
        editor->onUpdateUI(ev); // foward to editor
      }
  }
}

// ----------------------------------------------------------------------------- : Event table

BEGIN_EVENT_TABLE(SymbolControl, wxControl)
  EVT_PAINT          (SymbolControl::onPaint)
  EVT_SIZE           (SymbolControl::onSize)
  EVT_LEFT_UP        (SymbolControl::onLeftUp)
  EVT_LEFT_DOWN      (SymbolControl::onLeftDown)
  EVT_RIGHT_DOWN     (SymbolControl::onRightDown)
  EVT_LEFT_DCLICK    (SymbolControl::onLeftDClick)
  EVT_MIDDLE_DOWN    (SymbolControl::onMiddleDown)
  EVT_MIDDLE_UP      (SymbolControl::onMiddleUp)
  EVT_MOTION         (SymbolControl::onMotion)
  EVT_MOUSEWHEEL     (SymbolControl::onMouseWheel)
  EVT_MOUSE_CAPTURE_LOST(SymbolControl::onLoseCapture)
  EVT_KEY_UP         (SymbolControl::onKeyChange)
  EVT_KEY_DOWN       (SymbolControl::onKeyChange)
  EVT_CHAR           (SymbolControl::onChar)
END_EVENT_TABLE  ()

//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <render/symbol/viewer.hpp>
#include <util/error.hpp> // clearDC_black
#include <gui/util.hpp> // clearDC_black

// ----------------------------------------------------------------------------- : Simple rendering

Image render_symbol(const SymbolP& symbol, double border_radius, int width, int height, bool editing_hints, bool allow_smaller) {
  SymbolViewer viewer(symbol, editing_hints, width, border_radius);
  // limit width/height ratio to aspect ratio of symbol
  double ar  = symbol->aspectRatio();
  double par = (double)width/height;
  if (par > ar && (ar > 1 || (allow_smaller && height < width))) {
    width  = int(height * ar);
  } else if (par < ar && (ar < 1 || (allow_smaller && width < height))) {
    height = int(width / ar);
  }
  if (width > height) {
    viewer.setZoom(width);
    viewer.setOrigin(Vector2D(0,-(width-height) * 0.5));
    viewer.border_radius *= (double)height / width;
  } else {
    viewer.setZoom(height);
    viewer.setOrigin(Vector2D(-(height-width) * 0.5,0));
    viewer.border_radius *= (double)width / height;
  }
  Bitmap bmp(width, height);
  wxMemoryDC dc;
  dc.SelectObject(bmp);
  clearDC(dc, Color(0,128,0));
  viewer.draw(dc);
  dc.SelectObject(wxNullBitmap);
  return bmp.ConvertToImage();
}

// ----------------------------------------------------------------------------- : Constructor

SymbolViewer::SymbolViewer(const SymbolP& symbol, bool editing_hints, double size, double border_radius)
  : border_radius(border_radius), editing_hints(editing_hints)
  , rotation(0, RealRect(0,0,size,size), size)
  , multiply(size,0,0,size)
  , origin(0,0)
  , in_symmetry(0)
{
  setSymbol(symbol);
}

void SymbolViewer::setZoom(double zoom) {
  rotation.setZoom(zoom);
  rotation.setOrigin(zoom * origin);
  multiply = Matrix2D(zoom,0, 0,zoom);
}
void SymbolViewer::setOrigin(const Vector2D& origin) {
  this->origin = origin;
  rotation.setOrigin(origin);
}

// ----------------------------------------------------------------------------- : Drawing : Combining

typedef shared_ptr<wxMemoryDC> MemoryDCP;

// Return a temporary DC with the same size as the parameter
MemoryDCP getTempDC(DC& dc) {
  wxSize s = dc.GetSize();
  Bitmap buffer(s.GetWidth(), s.GetHeight(), 24);
  MemoryDCP newDC(new wxMemoryDC);
  newDC->SelectObject(buffer);
  clearDC(*newDC, *wxBLACK_BRUSH);
  return newDC;
}

// Copy a DC into an image
static Image dc_to_image(DC& from) {
  wxSize size = from.GetSize();
  Bitmap bmp(size.GetWidth(), size.GetHeight(), 24);
  wxMemoryDC tmpDC;
  tmpDC.SelectObject(bmp);
  tmpDC.Blit(0, 0, size.GetWidth(), size.GetHeight(), &from, 0, 0, wxCOPY);
  tmpDC.SelectObject(wxNullBitmap);
  return bmp.ConvertToImage();
}

// Combine all the temporary DCs with the main DC.
// The borders (of the fill, of the accent, and the extra border) are OR-ed together,
// then hidden below the interiors (fill and accent).
// Finally the accent is painted where it is set and there is no fill, or where it is on top of the fill.
void SymbolViewer::combineBuffers(DC& dc, Buffers& b) {
  DC* borders[]   = {b.border.get(), b.accent_border.get(), b.extra_border.get()};
  DC* interiors[] = {b.interior.get(), b.accent.get()};
  bool any = false;
  for (DC* d : borders)   if (d) any = true;
  for (DC* d : interiors) if (d) any = true;
  if (!any) return;

  wxSize size = dc.GetSize();
  size_t count = (size_t)size.GetWidth() * (size_t)size.GetHeight();

#ifdef __WXMSW__
  for (DC* d : borders)   if (d) dc.Blit(0, 0, size.GetWidth(), size.GetHeight(), d, 0, 0, wxOR);
  for (DC* d : interiors) if (d) dc.Blit(0, 0, size.GetWidth(), size.GetHeight(), d, 0, 0, wxAND_INVERT);
  if (!b.accent) return;
  // the accent is painted by hand
  Image out = dc_to_image(dc);
  Byte* outData = out.GetData();
#else
  // wxOR and wxAND_INVERT are currently only implemented on Windows, so we have to do them manually
  Image out = dc_to_image(dc);
  Byte* outData = out.GetData();
  for (DC* d : borders) {
    if (!d) continue;
    Image img = dc_to_image(*d);
    Byte* data = img.GetData();
    for (size_t i = 0; i < 3 * count; ++i) {
      outData[i] = outData[i] | data[i];
    }
  }
  for (DC* d : interiors) {
    if (!d) continue;
    Image img = dc_to_image(*d);
    Byte* data = img.GetData();
    for (size_t i = 0; i < 3 * count; ++i) {
      outData[i] = outData[i] & (Byte)~data[i];
    }
  }
#endif

  if (b.accent) {
    Image accentImg = dc_to_image(*b.accent);
    Image interiorImg, topImg;
    if (b.interior)   interiorImg = dc_to_image(*b.interior);
    if (b.accent_top) topImg      = dc_to_image(*b.accent_top);
    Byte* accentData   = accentImg.GetData();
    Byte* interiorData = b.interior   ? interiorImg.GetData() : nullptr;
    Byte* topData      = b.accent_top ? topImg.GetData()      : nullptr;
    for (size_t i = 0; i < count; ++i) {
      // The extra copies of a symmetry are drawn with a lower value when editing, those are only a hint, don't color them
      if (accentData[3 * i] < 248) continue;
      bool has_fill = interiorData && interiorData[3 * i] >= 128;
      bool on_top   = topData      && topData[3 * i]      >= 128;
      if (!has_fill || on_top) {
        outData[3 * i + 0] = 160;
        outData[3 * i + 1] = 0;
        outData[3 * i + 2] = 160;
      }
    }
  }

  Bitmap finalBmp(out);
  dc.DrawBitmap(finalBmp, 0, 0, false);
}

// Is there a shape that can not be drawn directly to the dc?
// Using the dc itself as the border of the fill only works if nothing has to combine with that border (intersection),
// or clear it or read it (shapes that act on the border and subtract or toggle).
static bool needs_buffering(const SymbolGroup& group) {
  FOR_EACH_CONST(p, group.parts) {
    if (const SymbolShape* s = p->isSymbolShape()) {
      if (s->combine == SYMBOL_COMBINE_INTERSECTION) return true;
      if (s->region == SYMBOL_REGION_BORDER && (s->combine == SYMBOL_COMBINE_SUBTRACT || s->combine == SYMBOL_COMBINE_DIFFERENCE)) return true;
    } else if (const SymbolGroup* g = p->isSymbolGroup()) {
      if (needs_buffering(*g)) return true;
    }
  }
  return false;
}

void SymbolViewer::draw(DC& dc) {
  bool paintedSomething = false;
  bool buffersFilled    = false;
  in_symmetry = 0;
  // Temporary dcs
  Buffers buffers;
  // Check if we can paint directly to the dc
  // If not, everything is buffered
  if (needs_buffering(*symbol)) {
    paintedSomething = true;
  }
  // Draw all parts
  combineSymbolPart(dc, *symbol, paintedSomething, buffersFilled, true, buffers);
  // Output the final parts from the buffer
  if (buffersFilled) {
    combineBuffers(dc, buffers);
  }
  // Editing hints?
  if (editing_hints) {
    drawEditingHints(dc);
  }
}
void SymbolViewer::combineSymbolPart(DC& dc, const SymbolPart& part, bool& paintedSomething, bool& buffersFilled, bool allow_overlap, Buffers& buffers) {
  if (const SymbolShape* s = part.isSymbolShape()) {
    if (s->combine == SYMBOL_COMBINE_OVERLAP && buffersFilled && allow_overlap) {
      // We will be overlapping some previous parts, write them to the screen
      combineBuffers(dc, buffers);
      // Clear the buffers
      buffersFilled = false;
      paintedSomething = true;
      wxSize s = dc.GetSize();
      if (buffers.border) {
        buffers.border->SetBrush(*wxBLACK_BRUSH);
        buffers.border->SetPen(  *wxTRANSPARENT_PEN);
        buffers.border->DrawRectangle(0, 0, s.GetWidth(), s.GetHeight());
      }
      if (buffers.interior) { // there is no interior if only accent shapes were drawn
        buffers.interior->SetBrush(*wxBLACK_BRUSH);
        buffers.interior->SetPen(  *wxTRANSPARENT_PEN);
        buffers.interior->DrawRectangle(0, 0, s.GetWidth(), s.GetHeight());
      }
      // the other masks are made again when they are needed
      buffers.accent_border.reset();
      buffers.accent.reset();
      buffers.accent_top.reset();
      buffers.extra_border.reset();
    }
    
    // Paint the part itself
    if (s->region == SYMBOL_REGION_ACCENT) {
      // The part acts on the accent, which has its own masks.
      // These are always buffered, and they are completely independent of the fill.
      if (!buffers.accent_border) buffers.accent_border = getTempDC(dc);
      if (!buffers.accent)        buffers.accent        = getTempDC(dc);
      if (!buffers.accent_top)    buffers.accent_top    = getTempDC(dc);
      combineSymbolShape(*s, *buffers.accent_border, *buffers.accent, false, false);
      buffersFilled = true;
    } else if (s->region == SYMBOL_REGION_BORDER) {
      // The part acts on the border as a whole, it can also change the borders of the fill and the accent.
      // Those are buffered if that is needed, see needs_buffering.
      if (!buffers.extra_border) buffers.extra_border = getTempDC(dc);
      combineBorderShape(*s, buffers.border.get(), buffers.accent_border.get(), *buffers.extra_border);
      buffersFilled = true;
    } else if (!paintedSomething) {
      // No need to buffer
      if (!buffers.interior) buffers.interior = getTempDC(dc);
      combineSymbolShape(*s, dc, *buffers.interior, true, false);
      buffersFilled = true;
    } else {
      if (!buffers.border)   buffers.border   = getTempDC(dc);
      if (!buffers.interior) buffers.interior = getTempDC(dc);
      // Draw this shape to the buffer
      combineSymbolShape(*s, *buffers.border, *buffers.interior, false, false);
      buffersFilled = true;
    }
    
    // Where the fill and the accent overlap, the one that was added last is on top.
    // Shapes that add something (so not subtract or intersection) are on top of what was already there.
    // (there is no accent yet if accent_top doesn't exist, so the fill is on top anyway)
    // A shape that acts on the border has no influence on this.
    if (buffers.accent_top && s->region != SYMBOL_REGION_BORDER
        && (s->combine == SYMBOL_COMBINE_OVERLAP || s->combine == SYMBOL_COMBINE_MERGE || s->combine == SYMBOL_COMBINE_DIFFERENCE)) {
      drawSymbolShape(*s, nullptr, buffers.accent_top.get(), 0, s->region == SYMBOL_REGION_ACCENT ? 255 : 0, false, false);
    }
  } else if (const SymbolSymmetry* s = part.isSymbolSymmetry()) {
    // Draw all parts, in reverse order (bottom to top), also draw rotated copies
    Radians b = 2 * s->handle.angle();
    Matrix2D old_m = multiply;
    Vector2D old_o = origin;
    int copies = s->kind == SYMMETRY_REFLECTION ? s->copies / 2 * 2 : s->copies;
    FOR_EACH_CONST_REVERSE(p, s->parts) {
      if (copies > 1) ++in_symmetry;
      for (int i = copies - 1 ; i >= 0 ; --i) {
        if (i == 0) --in_symmetry;
        if (s->clip) {
          // todo: clip
        }
        double a = i * 2 * M_PI / copies;
        if (s->kind == SYMMETRY_ROTATION || i % 2 == 0) {
          // set matrix
          // Calling:
          //  - p  the input point
          //  - p' the output point
          //  - rot our rotation matrix
          //  - d   out origin
          //  - o   the current origin (old_o)
          //  - m   the current matrix (old_m)
          // We want:
          //   p' = ((p - d) * rot + d) * m + o
          //      =  (p * rot - d * rot + d) * m + o
          //      =  p * rot * m + (d - d * rot) * m + o
          Matrix2D rot(cos(a),-sin(a), sin(a),cos(a));
          multiply = rot * old_m;
          origin = old_o + (s->center - s->center * rot) * old_m;
        } else {
          // reflection
          //  Calling angle = b
          // Matrix2D ref(cos(b),sin(b), sin(b),-cos(b));
          // Matrix2D rot(cos(a),-sin(a), sin(a),cos(a));
          // 
          //  ref * rot
          //    [ cos b   sin b !  [ cos a  -sin a !
          //  = ! sin b  -cos b ]  ! sin a   cos a ]
          //  = [ cos(a+b)  sin(a+b) !
          //    ! sin(a+b) -cos(a+b) ]
          Matrix2D rot(cos(a+b),sin(a+b), sin(a+b),-cos(a+b));
          multiply = rot * old_m;
          origin = old_o + (s->center - s->center * rot) * old_m;
        }
        // draw rotated copy
        combineSymbolPart(dc, *p, paintedSomething, buffersFilled, allow_overlap && i == copies - 1, buffers);
      }
    }
    multiply = old_m;
    origin   = old_o;
    if (editing_hints) {
      highlightPart(dc, *s, HIGHLIGHT_LESS);
    }
  } else if (const SymbolGroup* g = part.isSymbolGroup()) {
    // Draw all parts, in reverse order (bottom to top)
    FOR_EACH_CONST_REVERSE(p, g->parts) {
      combineSymbolPart(dc, *p, paintedSomething, buffersFilled, allow_overlap, buffers);
    }
  }
}


void SymbolViewer::combineSymbolShape(const SymbolShape& shape, DC& border, DC& interior, bool directB, bool directI) {
  // what color should the interior be?
  // use black when drawing to the screen
  Byte interiorCol = directI ? 0 : 255;
  if (editing_hints && in_symmetry) {
    interiorCol = directI ? 16 : 240;
  }
  // how to draw depends on combining mode
  switch(shape.combine) {
    case SYMBOL_COMBINE_OVERLAP:
    case SYMBOL_COMBINE_MERGE: {
      drawSymbolShape(shape, &border, &interior, 255, interiorCol, directB, false);
      break;
    } case SYMBOL_COMBINE_SUBTRACT: {
      border.SetLogicalFunction(wxAND);
      drawSymbolShape(shape, &border, &interior, 0, ~interiorCol, directB, false);
      border.SetLogicalFunction(wxCOPY);
      break;
    } case SYMBOL_COMBINE_INTERSECTION: {
      MemoryDCP keepBorder   = getTempDC(border);
      MemoryDCP keepInterior = getTempDC(interior);
      drawSymbolShape(shape, keepBorder.get(), keepInterior.get(), 255, 255, false, false);
      // combine the temporary dcs with the result using the AND operator
      wxSize s = border.GetSize();
      border  .Blit(0, 0, s.GetWidth(), s.GetHeight(), &*keepBorder  , 0, 0, wxAND);
      interior.Blit(0, 0, s.GetWidth(), s.GetHeight(), &*keepInterior, 0, 0, wxAND);
      break;
    } case SYMBOL_COMBINE_DIFFERENCE: {
      interior.SetLogicalFunction(wxXOR);
      drawSymbolShape(shape, &border, &interior, 0, interiorCol, directB, true);
      interior.SetLogicalFunction(wxCOPY);
      break;
    } case SYMBOL_COMBINE_BORDER: {
      break; // not used anymore, shapes that act on the border use their own region now
    }
  }
}


// ----------------------------------------------------------------------------- : Drawing : Basic


void SymbolViewer::combineBorderShape(const SymbolShape& shape, DC* border, DC* accent_border, DC& extra_border) {
  switch (shape.combine) {
    case SYMBOL_COMBINE_OVERLAP:
    case SYMBOL_COMBINE_MERGE: {
      // add the shape to the border
      drawSymbolShape(shape, nullptr, &extra_border, 0, 255, false, false);
      break;
    } case SYMBOL_COMBINE_BORDER: {
      break; // not used anymore, symbols with this are converted when they are read
    } case SYMBOL_COMBINE_SUBTRACT: {
      // remove the shape from the border
      for (DC* mask : {border, accent_border, &extra_border}) {
        if (mask) drawSymbolShape(shape, nullptr, mask, 0, 0, false, false);
      }
      break;
    } case SYMBOL_COMBINE_INTERSECTION: {
      // only keep the part of the border that is inside the shape
      MemoryDCP keep = getTempDC(extra_border);
      drawSymbolShape(shape, nullptr, keep.get(), 0, 255, false, false);
      wxSize s = extra_border.GetSize();
      for (DC* mask : {border, accent_border, &extra_border}) {
        if (mask) mask->Blit(0, 0, s.GetWidth(), s.GetHeight(), &*keep, 0, 0, wxAND);
      }
      break;
    } case SYMBOL_COMBINE_DIFFERENCE: {
      // toggle, where there is border it is removed, where there is none it is added.
      MemoryDCP shapeDC = getTempDC(extra_border);
      drawSymbolShape(shape, nullptr, shapeDC.get(), 0, 255, false, false);
      wxSize size = extra_border.GetSize();
      Image shapeImg = dc_to_image(*shapeDC);
      Image extraImg = dc_to_image(extra_border);
      Image borderImg, accentImg;
      if (border)        borderImg = dc_to_image(*border);
      if (accent_border) accentImg = dc_to_image(*accent_border);
      Byte* shapeData  = shapeImg.GetData();
      Byte* extraData  = extraImg.GetData();
      Byte* borderData = border        ? borderImg.GetData() : nullptr;
      Byte* accentData = accent_border ? accentImg.GetData() : nullptr;
      size_t count = (size_t)size.GetWidth() * (size_t)size.GetHeight();
      for (size_t i = 0; i < count; ++i) {
        size_t j = 3 * i;
        if (shapeData[j] < 128) continue; // not inside the shape
        bool on_border = extraData[j] >= 128
                      || (borderData && borderData[j] >= 128)
                      || (accentData && accentData[j] >= 128);
        for (size_t k = j; k < j + 3; ++k) {
          extraData[k] = on_border ? 0 : 255;
          if (borderData) borderData[k] = 0;
          if (accentData) accentData[k] = 0;
        }
      }
      extra_border.DrawBitmap(Bitmap(extraImg), 0, 0, false);
      if (border)        border       ->DrawBitmap(Bitmap(borderImg), 0, 0, false);
      if (accent_border) accent_border->DrawBitmap(Bitmap(accentImg), 0, 0, false);
      break;
    }
  }
}

void SymbolViewer::drawSymbolShape(const SymbolShape& shape, DC* border, DC* interior, Byte borderCol, Byte interiorCol, bool directB, bool clear) {
  // create point list
  vector<wxPoint> points;
  size_t size = shape.points.size();
  for(size_t i = 0 ; i < size ; ++i) {
    segment_subdivide(*shape.getPoint((int)i), *shape.getPoint((int)i+1), origin, multiply, points);
  }
  // draw border
  if (border && border_radius > 0) {
    // white/black or, if directB white/green
    border->SetBrush(Color(borderCol, (directB && borderCol == 0 ? 128 : borderCol), borderCol));
    border->SetPen(wxPen(*wxWHITE, (int) rotation.trS(border_radius)));
    border->DrawPolygon((int)points.size(), &points[0]);

    if (clear) {
      border->SetPen(*wxTRANSPARENT_PEN);
      border->SetBrush(Color(0, (directB ? 128 : 0), 0));

      wxRasterOperationMode func = border->GetLogicalFunction();
      border->SetLogicalFunction(wxCOPY);
      border->DrawPolygon((int)points.size(), &points[0]);
      border->SetLogicalFunction(func);
    }
  }
  // draw interior
  if (interior) {
    interior->SetBrush(Color(interiorCol,interiorCol,interiorCol));
    interior->SetPen(*wxTRANSPARENT_PEN);
    interior->DrawPolygon((int)points.size(), &points[0]);
  }
}

// ----------------------------------------------------------------------------- : Drawing : Highlighting

void SymbolViewer::highlightPart(DC& dc, const SymbolPart& part, HighlightStyle style) {
  if (const SymbolShape* s = part.isSymbolShape()) {
    highlightPart(dc, *s, style);
  } else if (const SymbolSymmetry* s = part.isSymbolSymmetry()) {
    highlightPart(dc, *s, style);
  } else if (const SymbolGroup* g = part.isSymbolGroup()) {
    highlightPart(dc, *g, style);
  } else {
    throw InternalError(_("Invalid symbol part type"));
  }
}

void SymbolViewer::highlightPart(DC& dc, const SymbolShape& shape, HighlightStyle style) {
  if (style == HIGHLIGHT_LESS) return;
  // create point list
  vector<wxPoint> points;
  size_t size = shape.points.size();
  for(size_t i = 0 ; i < size ; ++i) {
    segment_subdivide(*shape.getPoint((int)i), *shape.getPoint((int)i+1), origin, multiply, points);
  }
  // draw
  if (style == HIGHLIGHT_BORDER) {
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.SetPen  (wxPen(Color(255,0,0), 2));
    dc.DrawPolygon((int)points.size(), &points[0]);
  } else if (style == HIGHLIGHT_BORDER_DOT) {
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.SetPen  (wxPen(Color(255,0,0), 1, wxPENSTYLE_DOT));
    dc.DrawPolygon((int)points.size(), &points[0]);
  } else {
    dc.SetLogicalFunction(wxOR);
    dc.SetBrush(Color(0,0,64));
    dc.SetPen  (*wxTRANSPARENT_PEN);
    dc.DrawPolygon((int)points.size(), &points[0]);
    if (shape.combine == SYMBOL_COMBINE_SUBTRACT || shape.region == SYMBOL_REGION_BORDER) {
      dc.SetLogicalFunction(wxAND);
      dc.SetBrush(Color(191,191,255));
      dc.DrawPolygon((int)points.size(), &points[0]);
    }
    dc.SetLogicalFunction(wxCOPY);
  }
}

void SymbolViewer::highlightPart(DC& dc, const SymbolSymmetry& sym, HighlightStyle style) {
  // highlight parts?
  FOR_EACH_CONST(part, sym.parts) {
    highlightPart(dc, *part, (HighlightStyle)(style | HIGHLIGHT_LESS));
  }
  // Color?
  Color color = style & HIGHLIGHT_BORDER   ? Color(255,100,0)
              : style & HIGHLIGHT_INTERIOR ? Color(255,200,0)
              : Color(200,170,0);
  // center
  RealPoint center = rotation.tr(sym.center);
  // draw 'spokes'
  Radians angle = atan2(sym.handle.y, sym.handle.x);
  dc.SetPen(wxPen(color, sym.kind == SYMMETRY_ROTATION ? 1 : 3));
  int copies = sym.kind == SYMMETRY_REFLECTION ? sym.copies / 2 * 2 : sym.copies;
  for (int i = 0; i < copies ; ++i) {
    Radians a = angle + (i + 0.5) * 2 * M_PI / copies;
    Vector2D dir(cos(a), sin(a));
    Vector2D dir2 = rotation.tr(sym.center + 2.0 * dir);
    dc.DrawLine(int(center.x), int(center.y), int(dir2.x), int(dir2.y));
  }
  // draw center
  dc.SetPen(*wxBLACK_PEN);
  dc.SetBrush(color);
  dc.DrawCircle(int(center.x), int(center.y), sym.kind == SYMMETRY_ROTATION ? 7 : 5);
}

void SymbolViewer::highlightPart(DC& dc, const SymbolGroup& group, HighlightStyle style) {
  if (style == HIGHLIGHT_BORDER) {
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.SetPen  (wxPen(Color(255,0,0), 2));
    dc.DrawRectangle(rotation.trRectToBB(RealRect(group.bounds)));
  }
  FOR_EACH_CONST(part, group.parts) {
    highlightPart(dc, *part, (HighlightStyle)(style | HIGHLIGHT_LESS));
  }
}


void SymbolViewer::drawEditingHints(DC& dc) {
  // TODO?
}

//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gui/print_window.hpp>
#include <gui/card_select_window.hpp>
#include <gui/util.hpp>
#include <data/set.hpp>
#include <data/card.hpp>
#include <data/stylesheet.hpp>
#include <render/card/viewer.hpp>
#include <wx/print.h>
#include <wx/valnum.h>
#include <unordered_set>

DECLARE_POINTER_TYPE(PageLayout);

// ----------------------------------------------------------------------------- : Layout

void PrintJob::init(const RealSize& page_size) {
  this->page_size = page_size;
  if (cards.empty()) return;
  measure_cards();
  layout_cards();
  align_cards();
  center_cards();
}

void PrintJob::measure_cards() {
  unordered_set<CardP> already_measured_cards;
  FOR_EACH(card, cards) {
    if (already_measured_cards.find(card) != already_measured_cards.end()) continue;
    already_measured_cards.emplace(card);
    card_layouts.push_back(measure_card(card));
    CardP other_face = card->getLinkedOtherFace(cards);
    if (other_face && already_measured_cards.find(other_face) == already_measured_cards.end()) {
      already_measured_cards.emplace(other_face);
      card_layouts.push_back(measure_card(other_face));
      int index = card_layouts.size()-1;
      card_layouts[index].other_face = index-1;
      card_layouts[index-1].other_face = index;
    }
  }
  sorted_layouts = vector<CardLayout>(card_layouts);
  std::sort(sorted_layouts.begin(), sorted_layouts.end());
}
PrintJob::CardLayout PrintJob::measure_card(const CardP& card) {
  // initial measure
  const StyleSheet& stylesheet = set->stylesheetFor(card);
  RealSize size_mm(stylesheet.card_width * 25.4 / stylesheet.card_dpi, stylesheet.card_height * 25.4 / stylesheet.card_dpi);
  RealSize size_px(stylesheet.card_width,                              stylesheet.card_height);
  // check if we need to rotate to align best to default card height
  Radians rotation = 0.0;
  if (abs(size_mm.width - default_size_mm.height) < abs(size_mm.height - default_size_mm.height)) {
    swap(size_mm.width, size_mm.height);
    swap(size_px.width, size_px.height);
    rotation = rad90;
  }
  // snap to default_size_mm if we are close
  if (abs(size_mm.width  - default_size_mm.width)  < threshold_size.width)  size_mm.width  = default_size_mm.width;
  if (abs(size_mm.height - default_size_mm.height) < threshold_size.height) size_mm.height = default_size_mm.height;
  // add bleed
  double bleed_size_px = 0.0;
  if (!almost_equal(settings.print_bleed, 0.0)) {
    bleed_size_px = stylesheet.card_dpi * settings.print_bleed / 25.4;
    size_mm = RealSize(size_mm.width + 2 * settings.print_bleed, size_mm.height + 2 * settings.print_bleed);
    size_px = RealSize(size_px.width + 2 * bleed_size_px, size_px.height + 2 * bleed_size_px);
  }
  return CardLayout(card, size_mm, size_px, bleed_size_px, rotation);
}

void PrintJob::layout_cards() {
  page_layouts.push_back(vector<CardLayout>());
  double row_top = 0.0, row_height = 0.0, row_width = 0.0;
  unordered_set<CardP> already_laidout_cards;
  while (true) {
    // try to find a card that will fit on the current row
    FOR_EACH(layout, sorted_layouts) {
      if (already_laidout_cards.find(layout.card) != already_laidout_cards.end()) continue;
      if (layout.other_face >= 0) {
        CardLayout& other_layout = card_layouts[layout.other_face];
        if (    layout.size_mm.width + other_layout.size_mm.width   + settings.print_spacing + row_width >= page_size.width)  continue;
        if (max(layout.size_mm.height, other_layout.size_mm.height)                          + row_top   >= page_size.height) continue;
        // the card and its other face fit
        layout.pos.width  = row_width;
        layout.pos.height = row_top;
        page_layouts[page_layouts.size()-1].push_back(layout);
        already_laidout_cards.emplace(layout.card);
        other_layout.pos.width  = row_width + layout.size_mm.width + settings.print_spacing;
        other_layout.pos.height = row_top;
        page_layouts[page_layouts.size()-1].push_back(other_layout);
        already_laidout_cards.emplace(other_layout.card);
        if (already_laidout_cards.size() >= card_layouts.size()) return;
        // move to next spot on the row
        row_width += layout.size_mm.width + other_layout.size_mm.width + 2 * settings.print_spacing;
        row_height = max(max(row_height, layout.size_mm.height + settings.print_spacing), other_layout.size_mm.height + settings.print_spacing);
      }
      else {
        if (layout.size_mm.width  + row_width >= page_size.width)  continue;
        if (layout.size_mm.height + row_top   >= page_size.height) continue;
        // the card fits
        layout.pos.width  = row_width;
        layout.pos.height = row_top;
        page_layouts[page_layouts.size()-1].push_back(layout);
        already_laidout_cards.emplace(layout.card);
        if (already_laidout_cards.size() >= card_layouts.size()) return;
        // move to next spot on the row
        row_width += layout.size_mm.width + settings.print_spacing;
        row_height = max(row_height, layout.size_mm.height + settings.print_spacing);
      }
      goto continue_outer;
    }
    // no card fits
    if (row_top == 0.0 && row_height == 0.0 && row_width == 0.0) {
      // none of the remaining cards can fit on an empty page, return
      page_layouts.pop_back();
      queue_message(MESSAGE_WARNING, _ERROR_("cards bigger than page"));
      return;
    }
    if (row_height == 0.0 && row_width == 0.0) {
      // none of the remaining cards can fit on an empty row, create a new page
      page_layouts.push_back(vector<CardLayout>());
      row_top = 0.0;
      continue;
    }
    // none of the remaining cards can fit on this row, create a new row
    row_top += row_height;
    row_width = row_height = 0.0;
  continue_outer:;
  }
}

void PrintJob::align_cards() {
  // for each page
  for (int p = 0; p < page_layouts.size(); ++p) {
    vector<CardLayout>& page_layout = page_layouts[p];
    // for each card on the page
    for (int max = 0, j = 0; j < page_layout.size(); ++max, ++j) {
      if (max > 100) {
        queue_message(MESSAGE_WARNING, _("DEBUG: large amount of iterations when aligning cards for print"));
        break;
      }
      double x = page_layout[j].pos.width;
      double y = page_layout[j].pos.height;
      // if another card is almost aligned
      for (int i = 0; i < page_layout.size(); ++i) {
        if (i == j) continue;
        double difference = page_layout[i].pos.width - x;
        if (threshold_bottom < difference && difference <= threshold_top) {
          // get the card, and all cards to the right on the same row
          vector<int> cards;
          cards.push_back(j);
          for (int h = 0; h < page_layout.size(); ++h) {
            if (h == j) continue;
            double difference_x =     page_layout[h].pos.width  - x;
            double difference_y = abs(page_layout[h].pos.height - y);
            if (difference_y < threshold_bottom && difference_x > threshold_bottom) {
              cards.push_back(h);
            }
          }
          // check if all these cards can be moved to the right
          bool can_move = true;
          for (int h = 0; h < cards.size(); ++h) {
            if (page_layout[cards[h]].pos.width + page_layout[cards[h]].size_mm.width + difference > page_size.width) {
              can_move = false;
              break;
            }
          }
          // move the cards
          if (can_move) {
            for (int h = 0; h < cards.size(); ++h) {
              page_layout[cards[h]].pos.width += difference;
            }
            j = -1; // restart, new cards may be in range now
            goto continue_outer;
          }
        }
      }
    continue_outer:;
    }
  }
}

void PrintJob::center_cards() {
  for (int p = 0; p < page_layouts.size(); ++p) {
    vector<CardLayout>& page_layout = page_layouts[p];
    RealSize page_margin(0.0, 0.0);
    for (int i = 0; i < page_layout.size(); ++i) {
      double width  = page_layout[i].pos.width  + page_layout[i].size_mm.width;
      double height = page_layout[i].pos.height + page_layout[i].size_mm.height;
      if (page_margin.width  < width)  page_margin.width  = width;
      if (page_margin.height < height) page_margin.height = height;
    }
    page_margin.width  = (page_size.width  - page_margin.width)  / 2;
    page_margin.height = (page_size.height - page_margin.height) / 2;
    page_margins.push_back(page_margin);
    for (int i = 0; i < page_layout.size(); ++i) {
      page_layout[i].pos.width  += page_margin.width;
      page_layout[i].pos.height += page_margin.height;
    }
  }
}

// ----------------------------------------------------------------------------- : Printout

/// A printout object specifying how to print a specified set of cards
class CardsPrintout : public wxPrintout {
public:
  CardsPrintout(PrintJobP const& job);
  /// Number of pages, and something else I don't understand...
  void GetPageInfo(int* pageMin, int* pageMax, int* pageFrom, int* pageTo) override;
  /// Again, 'number of pages', strange wx interface
  bool HasPage(int page) override;
  /// Determine the layout
  void OnPreparePrinting() override;
  /// Print a page
  bool OnPrintPage(int page) override;
  
private:
  PrintJobP job; ///< Cards to print
  DataViewer viewer;
  RealSize printer_px_per_mm;
  
  int pageCount() {
    return job->page_layouts.size();
  }
  
  /// Draw cutter lines in the page margins, corresponding to the edges of cards
  void drawCutterLines(DC& dc, PrintJobP& job, int page);
  static constexpr double CUTTER_LINE_SIZE_MM = 8.0;
  /// Draw cards according to their CardLayout info
  void drawCards      (DC& dc, PrintJobP& job, int page);
  void drawCard       (DC& dc, PrintJob::CardLayout& card_layout);
};

CardsPrintout::CardsPrintout(PrintJobP const& job)
  : job(job)
{
  viewer.setSet(job->set);
}

void CardsPrintout::GetPageInfo(int* page_min, int* page_max, int* page_from, int* page_to) {
  *page_from = *page_min = 1;
  *page_to   = *page_max = pageCount();
}

bool CardsPrintout::HasPage(int page) {
  return page <= pageCount(); // page number is 1 based
}

void CardsPrintout::OnPreparePrinting() {
  if (job->empty()) {
    int pw_mm, ph_mm;
    GetPageSizeMM(&pw_mm, &ph_mm);
    job->init(RealSize(pw_mm, ph_mm));
  }
}


bool CardsPrintout::OnPrintPage(int page) {
  DC& dc = *GetDC();
  // page size in millimeters
  int page_width_mm, page_height_mm;
  GetPageSizeMM(&page_width_mm, &page_height_mm);
  // page size in pixels
  int page_width_px, page_height_px;
  dc.GetSize(&page_width_px, &page_height_px);
  // scale factor (pixels per mm)
  printer_px_per_mm = RealSize((double)page_width_px / page_width_mm, (double)page_height_px / page_height_mm);
  // print the cards that belong on this page
  if (settings.print_cutter_lines != CUTTER_NONE) drawCutterLines(dc, job, page);
  drawCards(dc, job, page);
  return true;
}

void CardsPrintout::drawCards(DC& dc, PrintJobP& job, int page) {
  FOR_EACH(card_layout, job->page_layouts[page - 1]) {
    drawCard(dc, card_layout);
  }
  dc.SetDeviceOrigin(0, 0);
}
void CardsPrintout::drawCard(DC& dc, PrintJob::CardLayout& card_layout) {
  // upscale to printer dpi
  RealPoint scale(printer_px_per_mm.width  * card_layout.size_mm.width  / card_layout.size_px.width,
                  printer_px_per_mm.height * card_layout.size_mm.height / card_layout.size_px.height);
  int bleed = lround(card_layout.bleed_size_px * scale.x);
  int width = lround(card_layout.size_px.width * scale.x), height = lround(card_layout.size_px.height * scale.y);
  // render card to its own dc
  wxBitmap buffer(width, height, 32);
  wxMemoryDC bufferDC;
  bufferDC.SelectObject(buffer);
  clearDC(bufferDC,*wxWHITE_BRUSH);
  RotatedDC rdc(bufferDC, card_layout.rot, RealRect(0, 0, card_layout.size_px.width, card_layout.size_px.height), scale.x, QUALITY_AA, ROTATION_ATTACH_TOP_LEFT);
  if (is_rad90(card_layout.rot)) bufferDC.SetDeviceOrigin(bleed, height - width - 1 - bleed); // I don't understand why this is needed
  else bufferDC.SetDeviceOrigin(bleed, bleed);
  viewer.setCard(card_layout.card);
  viewer.draw(rdc, *wxWHITE);
  bufferDC.SetDeviceOrigin(0,0);
  /// add print bleed edge
  if (bleed > 0) {
    auto left_rect = wxRect(bleed + 1, bleed, bleed, height - 2 * bleed);
    Image left_border = bufferDC.GetAsBitmap(&left_rect).ConvertToImage();
    bufferDC.DrawBitmap(left_border.Mirror(true), 0, bleed);
    auto right_rect = wxRect(width - 2 * bleed - 1, bleed, bleed, height - 2 * bleed);
    Image right_border = bufferDC.GetAsBitmap(&right_rect).ConvertToImage();
    bufferDC.DrawBitmap(right_border.Mirror(true), width - bleed - 1, bleed);
    auto top_rect = wxRect(0, bleed + 1, width, bleed);
    Image top_border = bufferDC.GetAsBitmap(&top_rect).ConvertToImage();
    bufferDC.DrawBitmap(top_border.Mirror(false), 0, 0);
    auto bottom_rect = wxRect(0, height - 2 * bleed - 1, width, bleed);
    Image bottom_border = bufferDC.GetAsBitmap(&bottom_rect).ConvertToImage();
    bufferDC.DrawBitmap(bottom_border.Mirror(false), 0, height - bleed - 1);
  }
  // render card dc to page dc
  bufferDC.SelectObject(wxNullBitmap);
  dc.SetDeviceOrigin(int(printer_px_per_mm.width * card_layout.pos.width), int(printer_px_per_mm.height * card_layout.pos.height));
  dc.DrawBitmap(buffer, 0, 0);
}

void CardsPrintout::drawCutterLines(DC& dc, PrintJobP& job, int page) {
  const vector<PrintJob::CardLayout>& page_layouts = job->page_layouts[page - 1];
  const RealSize& page_margin = job->page_margins[page - 1];
  int page_width, page_height;
  GetPageSizeMM(&page_width, &page_height);
  wxPen pen(wxColour(0, 0, 0), 2, wxPENSTYLE_SOLID);
  pen.SetQuality(wxPEN_QUALITY_HIGH);
  dc.SetPen(pen);
  // vertical
  int v_bleed = printer_px_per_mm.width * settings.print_bleed;
  for (int i = 0; i < page_layouts.size(); ++i) {
    double left_line  = page_layouts[i].pos.width;
    double right_line = left_line + page_layouts[i].size_mm.width;
    bool draw_left_line  = true;
    bool draw_right_line = true;
    if (settings.print_cutter_lines == CUTTER_NO_INTERSECTION) {
      // check if another card is in the way of this cutter line
      for (int j = 0; j < page_layouts.size(); ++j) {
        if (i == j) continue;
        double other_left_line = page_layouts[j].pos.width;
        double other_right_line = other_left_line + page_layouts[j].size_mm.width;
        if (draw_left_line  && left_line  - other_left_line > job->threshold_bottom && other_right_line - left_line  > job->threshold_bottom) {
          draw_left_line = false;
          if (!draw_right_line) break;
        }
        if (draw_right_line && right_line - other_left_line > job->threshold_bottom && other_right_line - right_line > job->threshold_bottom) {
          draw_right_line = false;
          if (!draw_left_line) break;
        }
      }
    }
    if (draw_left_line) {
      int left = printer_px_per_mm.width * left_line;
      dc.DrawLine(wxPoint(left  + v_bleed, 0.0),
                  wxPoint(left  + v_bleed, printer_px_per_mm.height * CUTTER_LINE_SIZE_MM));
      dc.DrawLine(wxPoint(left  + v_bleed, printer_px_per_mm.height * page_height),
                  wxPoint(left  + v_bleed, printer_px_per_mm.height * (page_height - CUTTER_LINE_SIZE_MM)));
    }
    if (draw_right_line) {
      int right = printer_px_per_mm.width * right_line;
      dc.DrawLine(wxPoint(right - v_bleed, 0.0),
                  wxPoint(right - v_bleed, printer_px_per_mm.height * CUTTER_LINE_SIZE_MM));
      dc.DrawLine(wxPoint(right - v_bleed, printer_px_per_mm.height * page_height),
                  wxPoint(right - v_bleed, printer_px_per_mm.height * (page_height - CUTTER_LINE_SIZE_MM)));
    }
  }
  // horizontal
  int h_bleed = printer_px_per_mm.height * settings.print_bleed;
  for (int i = 0; i < page_layouts.size(); ++i) {
    double top_line  = page_layouts[i].pos.height;
    double bottom_line = top_line + page_layouts[i].size_mm.height;
    bool draw_top_line  = true;
    bool draw_bottom_line = true;
    if (settings.print_cutter_lines == CUTTER_NO_INTERSECTION) {
      // check if another card is in the way of this cutter line
      for (int j = 0; j < page_layouts.size(); ++j) {
        if (i == j) continue;
        double other_top_line = page_layouts[j].pos.height;
        double other_bottom_line = other_top_line + page_layouts[j].size_mm.height;
        if (draw_top_line    && top_line    - other_top_line > job->threshold_bottom && other_bottom_line - top_line    > job->threshold_bottom) {
          draw_top_line = false;
          if (!draw_bottom_line) break;
        }
        if (draw_bottom_line && bottom_line - other_top_line > job->threshold_bottom && other_bottom_line - bottom_line > job->threshold_bottom) {
          draw_bottom_line = false;
          if (!draw_top_line) break;
        }
      }
    }
    if (draw_top_line) {
      int top = printer_px_per_mm.height * top_line;
      dc.DrawLine(wxPoint(0.0,                                                          top    + h_bleed),
                  wxPoint(printer_px_per_mm.width * CUTTER_LINE_SIZE_MM,                top    + h_bleed));
      dc.DrawLine(wxPoint(printer_px_per_mm.width * page_width,                         top    + h_bleed),
                  wxPoint(printer_px_per_mm.width * (page_width - CUTTER_LINE_SIZE_MM), top    + h_bleed));
    }
    if (draw_bottom_line) {
      int bottom = printer_px_per_mm.height * bottom_line;
      dc.DrawLine(wxPoint(0.0,                                                          bottom - h_bleed),
                  wxPoint(printer_px_per_mm.width * CUTTER_LINE_SIZE_MM,                bottom - h_bleed));
      dc.DrawLine(wxPoint(printer_px_per_mm.width * page_width,                         bottom - h_bleed),
                  wxPoint(printer_px_per_mm.width * (page_width - CUTTER_LINE_SIZE_MM), bottom - h_bleed));
    }
  }
}

// ----------------------------------------------------------------------------- : PrintWindow

PrintJobP make_print_job(Window* parent, const SetP& set, const ExportCardSelectionChoices& choices) {
  // Let the user choose cards and spacing
  // controls
  ExportWindowBase wnd(parent, _TITLE_("select cards print"), set, choices);
  wxFloatingPointValidator<double> validator(2, NULL);
  validator.SetRange(0.0, 10.0);
  wxTextCtrl* space = new wxTextCtrl(&wnd, wxID_ANY, wxString::Format(wxT("%.2f"), settings.print_spacing), wxDefaultPosition, wxDefaultSize, 0L, validator);
  wxTextCtrl* bleed = new wxTextCtrl(&wnd, wxID_ANY, wxString::Format(wxT("%.2f"), settings.print_bleed),   wxDefaultPosition, wxDefaultSize, 0L, validator);
  wxChoice* cutter  = new wxChoice(&wnd, wxID_ANY);
  cutter->Clear();
  cutter->Append(_LABEL_("cutter lines all"));
  cutter->Append(_LABEL_("cutter lines no intersect"));
  cutter->Append(_LABEL_("cutter lines none"));
  cutter->SetSelection((int)settings.print_cutter_lines);
  // layout
  wxSizer* s = new wxBoxSizer(wxVERTICAL);
    wxSizer* s2 = new wxBoxSizer(wxHORIZONTAL);
      wxSizer* s3 = wnd.Create();
      s2->Add(s3, 1, wxEXPAND | wxALL, 8);
      wxSizer* s4 = new wxStaticBoxSizer(wxVERTICAL, &wnd, _TITLE_("settings"));
        s4->Add(new wxStaticText(&wnd, -1, _LABEL_("spacing print")), 0, wxALL, 8);
        s4->Add(space, 0, wxALL & ~wxTOP, 8);
        s4->Add(new wxStaticText(&wnd, -1, _LABEL_("bleed print")), 0, wxALL, 8);
        s4->Add(bleed, 0, wxALL & ~wxTOP, 8);
        s4->Add(new wxStaticText(&wnd, -1, _LABEL_("cutter lines print")), 0, wxALL, 8);
        s4->Add(cutter, 0, wxALL & ~wxTOP, 8);
      s2->Add(s4, 1, wxEXPAND | (wxALL & ~wxLEFT), 8);
    s->Add(s2, 1, wxEXPAND);
    s->Add(wnd.CreateButtonSizer(wxOK | wxCANCEL) , 0, wxEXPAND | wxALL, 8);
  s->SetSizeHints(&wnd);
  wnd.SetSizer(s);
  wnd.SetMinSize(wxSize(300,-1));
  // show window
  if (wnd.ShowModal() != wxID_OK) {
    return PrintJobP(); // cancel
  } else {
    // save settings, make print job
    String spacing = space->GetValue();
    if (spacing.empty()) spacing = _("0");
    spacing.ToDouble(&settings.print_spacing);
    String bleeding = bleed->GetValue();
    if (bleeding.empty()) bleeding = _("0");
    bleeding.ToDouble(&settings.print_bleed);
    settings.print_cutter_lines = (CutterLinesType)cutter->GetSelection();
    PrintJobP job = make_intrusive<PrintJob>(set, wnd.getSelection());
    return job;
  }
}

void print_preview(Window* parent, const PrintJobP& job) {
  if (!job) return;
  // Show the print preview
  wxPreviewFrame* frame = new wxPreviewFrame(
    new wxPrintPreview(
      new CardsPrintout(job),
      new CardsPrintout(job)
    ), parent, _TITLE_("print preview"));
  frame->Initialize();
  frame->Maximize(true);
  frame->Show();
}

void print_set(Window* parent, const PrintJobP& job) {
  if (!job) return;
  // Print the cards
  wxPrinter p;
  CardsPrintout pout(job);
  p.Print(parent, &pout, true);
}

void print_preview(Window* parent, const SetP& set, const ExportCardSelectionChoices& choices) {
  print_preview(parent, make_print_job(parent, set, choices));
}
void print_set(Window* parent, const SetP& set, const ExportCardSelectionChoices& choices) {
  print_set(parent, make_print_job(parent, set, choices));
}


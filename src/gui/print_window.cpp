//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
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
  FOR_EACH(card, cards) {
    const StyleSheet& stylesheet = set->stylesheetFor(card);
    RealSize size_px(stylesheet.card_width,                              stylesheet.card_height);
    RealSize size_mm(stylesheet.card_width * 25.4 / stylesheet.card_dpi, stylesheet.card_height * 25.4 / stylesheet.card_dpi);
    Radians rotation = 0.0;
    bool rotated = abs(size_mm.width - default_size_mm.height) < abs(size_mm.height - default_size_mm.height); // try to align best to default card height
    if (rotated) {
      swap(size_mm.width, size_mm.height);
      swap(size_px.width, size_px.height);
      rotation = rad90;
    }
    if (abs(size_mm.width  - default_size_mm.width)  < threshold_size.width)  size_mm.width  = default_size_mm.width; // snap to default_size_mm if we are close
    if (abs(size_mm.height - default_size_mm.height) < threshold_size.height) size_mm.height = default_size_mm.height;
    CardLayout layout(card, size_mm, size_px, rotation);
    card_layouts.push_back(layout);
  }
  std::sort(card_layouts.begin(), card_layouts.end());
}
void PrintJob::layout_cards() {
  page_layouts.push_back(vector<CardLayout>());
  double row_top = 0.0, row_height = 0.0, row_width = 0.0;
  unordered_set<int> already_laidout_cards;
  while (true) {
    // try to find a card that will fit on the current row
    for (int i = 0; i < card_layouts.size(); ++i) {
      if (already_laidout_cards.find(i) != already_laidout_cards.end())  continue;
      if (card_layouts[i].size_mm.width + row_width >= page_size.width)  continue;
      if (card_layouts[i].size_mm.height + row_top  >= page_size.height) continue;
      // the card fits
      card_layouts[i].pos.width  = row_width;
      card_layouts[i].pos.height = row_top;
      page_layouts[page_layouts.size()-1].push_back(card_layouts[i]);
      already_laidout_cards.insert(i);
      if (already_laidout_cards.size() == card_layouts.size()) return;
      // move to next spot on the row
      row_width += card_layouts[i].size_mm.width + settings.print_spacing;
      row_height = max(row_height, card_layouts[i].size_mm.height + settings.print_spacing);
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
  drawCards(dc, job, page);
  if (settings.print_cutter_lines != CUTTER_NONE) drawCutterLines(dc, job, page);
  return true;
}

void CardsPrintout::drawCards(DC& dc, PrintJobP& job, int page) {
  FOR_EACH(card_layout, job->page_layouts[page - 1]) {
    drawCard(dc, card_layout);
  }
  dc.SetUserScale(printer_px_per_mm.width, printer_px_per_mm.height);
}
void CardsPrintout::drawCard(DC& dc, PrintJob::CardLayout& card_layout) {
  // draw card to its own buffer
  wxBitmap buffer(card_layout.size_px.width, card_layout.size_px.height, 32);
  wxMemoryDC bufferDC;
  bufferDC.SelectObject(buffer);
  clearDC(bufferDC,*wxWHITE_BRUSH);
  RotatedDC rdc(bufferDC, card_layout.rot, RealRect(0, 0, card_layout.size_px.width, card_layout.size_px.height), 1.0, QUALITY_AA, ROTATION_ATTACH_TOP_LEFT);
  viewer.setCard(card_layout.card);
  viewer.draw(rdc, *wxWHITE);
  bufferDC.SelectObject(wxNullBitmap);
  // draw card buffer to page dc
  dc.SetUserScale(printer_px_per_mm.width / card_layout.px_per_mm.width, printer_px_per_mm.height / card_layout.px_per_mm.height);
  dc.DrawBitmap(buffer, int(card_layout.pos.width * card_layout.px_per_mm.width), int(card_layout.pos.height * card_layout.px_per_mm.height));
}

void CardsPrintout::drawCutterLines(DC& dc, PrintJobP& job, int page) {
  const vector<PrintJob::CardLayout>& page_layout = job->page_layouts[page - 1];
  const RealSize& page_margin = job->page_margins[page - 1];
  int page_width, page_height;
  GetPageSizeMM(&page_width, &page_height);

  double vertical_line_size = min(10.0, page_margin.height - 3.0);
  if (vertical_line_size > 0.0) {
    for (int i = 0; i < page_layout.size(); ++i) {
      double left_line  = page_layout[i].pos.width;
      double right_line = left_line + page_layout[i].size_mm.width;
      bool draw_left_line  = true;
      bool draw_right_line = true;
      if (settings.print_cutter_lines == CUTTER_NO_INTERSECTION) {
        // check if another card is in the way of this cutter line
        for (int j = 0; j < page_layout.size(); ++j) {
          if (i == j) continue;
          double other_left_line = page_layout[j].pos.width;
          double other_right_line = other_left_line + page_layout[j].size_mm.width;
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
      const RealSize& px_per_mm = page_layout[i].px_per_mm;
      dc.SetUserScale(printer_px_per_mm.width / px_per_mm.width, printer_px_per_mm.height / px_per_mm.height);
      if (draw_left_line) {
        dc.DrawLine(wxPoint(px_per_mm.width * left_line,  0.0),                            wxPoint(px_per_mm.width * left_line,  px_per_mm.height * vertical_line_size));
        dc.DrawLine(wxPoint(px_per_mm.width * left_line,  px_per_mm.height * page_height), wxPoint(px_per_mm.width * left_line,  px_per_mm.height * (page_height - vertical_line_size)));
      }
      if (draw_right_line) {
        dc.DrawLine(wxPoint(px_per_mm.width * right_line, 0.0),                            wxPoint(px_per_mm.width * right_line, px_per_mm.height * vertical_line_size));
        dc.DrawLine(wxPoint(px_per_mm.width * right_line, px_per_mm.height * page_height), wxPoint(px_per_mm.width * right_line, px_per_mm.height * (page_height - vertical_line_size)));
      }
    }
  } else {
    queue_message(MESSAGE_WARNING, _ERROR_("v margin too small for cutter"));
  }
  
  double horizontal_line_size = min(10.0, page_margin.width - 3.0);
  if (horizontal_line_size > 0.0) {
    for (int i = 0; i < page_layout.size(); ++i) {
      double top_line  = page_layout[i].pos.height;
      double bottom_line = top_line + page_layout[i].size_mm.height;
      bool draw_top_line  = true;
      bool draw_bottom_line = true;
      if (settings.print_cutter_lines == CUTTER_NO_INTERSECTION) {
        // check if another card is in the way of this cutter line
        for (int j = 0; j < page_layout.size(); ++j) {
          if (i == j) continue;
          double other_top_line = page_layout[j].pos.height;
          double other_bottom_line = other_top_line + page_layout[j].size_mm.height;
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
      const RealSize& px_per_mm = page_layout[i].px_per_mm;
      dc.SetUserScale(printer_px_per_mm.width / px_per_mm.width, printer_px_per_mm.height / px_per_mm.height);
      if (draw_top_line) {
        dc.DrawLine(wxPoint(0.0,                          px_per_mm.height * top_line),    wxPoint(px_per_mm.width * horizontal_line_size,                px_per_mm.height * top_line));
        dc.DrawLine(wxPoint(px_per_mm.width * page_width, px_per_mm.height * top_line),    wxPoint(px_per_mm.width * (page_width - horizontal_line_size), px_per_mm.height * top_line));
      }
      if (draw_bottom_line) {
        dc.DrawLine(wxPoint(0.0,                          px_per_mm.height * bottom_line), wxPoint(px_per_mm.width * horizontal_line_size,                px_per_mm.height * bottom_line));
        dc.DrawLine(wxPoint(px_per_mm.width * page_width, px_per_mm.height * bottom_line), wxPoint(px_per_mm.width * (page_width - horizontal_line_size), px_per_mm.height * bottom_line));
      }
    }
  } else {
    queue_message(MESSAGE_WARNING, _ERROR_("h margin too small for cutter"));
  }
  dc.SetUserScale(printer_px_per_mm.width, printer_px_per_mm.height);
}

// ----------------------------------------------------------------------------- : PrintWindow

PrintJobP make_print_job(Window* parent, const SetP& set, const ExportCardSelectionChoices& choices) {
  // Let the user choose cards and spacing
  // controls
  ExportWindowBase wnd(parent, _TITLE_("select cards print"), set, choices);
  wxFloatingPointValidator<float> validator(2, NULL, wxNUM_VAL_ZERO_AS_BLANK);
  validator.SetRange(0, 100);
  wxTextCtrl* space = new wxTextCtrl(&wnd, wxID_ANY, _(""), wxDefaultPosition, wxDefaultSize, 0L, validator);
  space->SetValue(wxString::Format(wxT("%lf"), settings.print_spacing));
  wxChoice* cutter = new wxChoice(&wnd, wxID_ANY);
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
    queue_message(MESSAGE_WARNING, wxString::Format(_("%f"),settings.print_spacing));
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

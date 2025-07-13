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
    RealSize size(stylesheet.card_width * 25.4 / stylesheet.card_dpi, stylesheet.card_height * 25.4 / stylesheet.card_dpi);
    Radians rotation = 0.0;
    bool rotated = abs(size.width - default_size.height) < abs(size.height - default_size.height); // try to align best to default card height
    if (rotated) {
      swap(size.width, size.height);
      rotation = rad90;
    }
    CardLayout layout(card, size, rotation);
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
      if (already_laidout_cards.find(i) != already_laidout_cards.end()) continue;
      if (card_layouts[i].size.width + row_width >= page_size.width)    continue;
      if (card_layouts[i].size.height + row_top  >= page_size.height)   continue;
      // the card fits
      card_layouts[i].pos.width = row_width;
      card_layouts[i].pos.height = row_top;
      page_layouts[page_layouts.size()-1].push_back(card_layouts[i]);
      already_laidout_cards.insert(i);
      if (already_laidout_cards.size() == card_layouts.size()) return;
      // move to next spot on the row
      row_width += card_layouts[i].size.width + spacing;
      row_height = max(row_height, card_layouts[i].size.height + spacing);
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
  // align cards that are at most this far appart in millimeters
  double threshold_top = 0.2 * default_size.width;
  // consider cards that are this close to already be aligned
  double threshold_bottom = 0.05;
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
            if (page_layout[cards[h]].pos.width + page_layout[cards[h]].size.width + difference > page_size.width) {
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
      double width  = page_layout[i].pos.width  + page_layout[i].size.width;
      double height = page_layout[i].pos.height + page_layout[i].size.height;
      if (page_margin.width  < width)  page_margin.width  = width;
      if (page_margin.height < height) page_margin.height = height;
    }
    page_margin.width  = (page_size.width  - page_margin.width)  / 2;
    page_margin.height = (page_size.height - page_margin.height) / 2;
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
  double scale_x, scale_y; // priter pixel per mm
  
  int pageCount() {
    return job->page_layouts.size();
  }
  
  /// Draw a card according to it's CardLayout info
  void drawCard(DC& dc, const PrintJob::CardLayout& card_layout);
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
  int pw_mm, ph_mm;
  GetPageSizeMM(&pw_mm, &ph_mm);
  // page size in pixels
  int pw_px, ph_px;
  dc.GetSize(&pw_px, &ph_px);
  // scale factors (pixels per mm)
  scale_x = (double)pw_px / pw_mm;
  scale_y = (double)ph_px / ph_mm;
  // print the cards that belong on this page
  FOR_EACH(card_layout, job->page_layouts[page - 1]) {
    drawCard(dc, card_layout);
  }
  return true;
}

void CardsPrintout::drawCard(DC& dc, const PrintJob::CardLayout& card_layout) {
  const StyleSheet& stylesheet = job->set->stylesheetFor(card_layout.card);
  // draw card to its own buffer
  int w = int(stylesheet.card_width), h = int(stylesheet.card_height); // in pixels
  if (is_rad90(card_layout.rotation)) swap(w,h);
  wxBitmap buffer(w,h,32);
  wxMemoryDC bufferDC;
  bufferDC.SelectObject(buffer);
  clearDC(bufferDC,*wxWHITE_BRUSH);
  RotatedDC rdc(bufferDC, card_layout.rotation, stylesheet.getCardRect(), 1.0, QUALITY_AA, ROTATION_ATTACH_TOP_LEFT);
  viewer.setCard(card_layout.card);
  viewer.draw(rdc, *wxWHITE);
  bufferDC.SelectObject(wxNullBitmap);
  // draw card buffer to page dc
  double px_per_mm = stylesheet.card_dpi / 25.4;
  dc.SetUserScale(scale_x / px_per_mm, scale_y / px_per_mm);
  dc.DrawBitmap(buffer, int(card_layout.pos.width * px_per_mm), int(card_layout.pos.height * px_per_mm));
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
  // layout
  wxSizer* s = new wxBoxSizer(wxVERTICAL);
    wxSizer* s2 = new wxBoxSizer(wxHORIZONTAL);
      wxSizer* s3 = wnd.Create();
      s2->Add(s3, 1, wxEXPAND | wxALL, 8);
      wxSizer* s4 = new wxStaticBoxSizer(wxVERTICAL, &wnd, _TITLE_("settings"));
        s4->Add(new wxStaticText(&wnd, -1, _LABEL_("spacing print")), 0, wxALL, 8);
        s4->Add(space, 0, wxALL & ~wxTOP, 8);
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
    // make print job
    double spacing;
    space->GetValue().ToDouble(&spacing);
    settings.print_spacing = spacing;
    PrintJobP job = make_intrusive<PrintJob>(set, wnd.getSelection(), spacing);
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

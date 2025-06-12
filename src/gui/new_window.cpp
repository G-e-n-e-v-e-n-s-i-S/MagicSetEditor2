//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gui/new_window.hpp>
#include <gui/control/gallery_list.hpp>
#include <gui/control/package_list.hpp>
#include <data/game.hpp>
#include <data/stylesheet.hpp>
#include <data/set.hpp>
#include <data/card.hpp>
#include <data/settings.hpp>
#include <util/window_id.hpp>

// ----------------------------------------------------------------------------- : NewSetWindow

SetP new_set_window(Window* parent) {
  NewSetWindow wnd(parent);
  wnd.ShowModal();
  return wnd.set;
}

NewSetWindow::NewSetWindow(Window* parent)
  : wxDialog(parent, wxID_ANY, _TITLE_("new set"), wxDefaultPosition, wxSize(830,360), wxDEFAULT_DIALOG_STYLE)
{
  wxBusyCursor wait;
  // init controls
  game_list       = new PackageList (this, ID_GAME_LIST,       wxHORIZONTAL, false);
  stylesheet_list = new PackageList (this, ID_STYLESHEET_LIST, wxHORIZONTAL, false);
  wxStaticText* game_text       = new wxStaticText(this, ID_GAME_LIST,       _LABEL_("game type"));
  wxStaticText* stylesheet_text = new wxStaticText(this, ID_STYLESHEET_LIST, _LABEL_("style type"));

  game_filter = new FilterCtrl(this, ID_GAME_FILTER, _LABEL_("search game list"), _HELP_("search game list control"));
  game_filter->setFilter(game_filter_value);

  stylesheet_filter = new FilterCtrl(this, ID_STYLESHEET_FILTER, _LABEL_("search stylesheet list"), _HELP_("search stylesheet list control"));
  stylesheet_filter->setFilter(stylesheet_filter_value);

  // init sizer
  wxSizer* s = new wxBoxSizer(wxVERTICAL);
    wxSizer* s2 = new wxBoxSizer(wxHORIZONTAL);
      s2->Add(game_text, 0, wxALL & ~wxLEFT, 4);
      s2->AddStretchSpacer();
      s2->Add(game_filter, 1, wxRIGHT, 4);
    s->Add(s2, wxSizerFlags().Expand().Border(wxALL, 6));
    s->Add(game_list,       0, wxEXPAND | (wxALL & ~wxTOP), 4);
    wxSizer* s3 = new wxBoxSizer(wxHORIZONTAL);
      s3->Add(stylesheet_text, 0, wxALL & ~wxLEFT, 4);
      s3->AddStretchSpacer();
      s3->Add(stylesheet_filter, 1, wxRIGHT, 4);
    s->Add(s3, wxSizerFlags().Expand().Border(wxALL, 6));
    s->Add(stylesheet_list, 0, wxEXPAND | (wxALL & ~wxTOP), 4);
    s->Add(CreateButtonSizer(wxOK | wxCANCEL) , 0, wxEXPAND | wxALL, 8);
    s->SetSizeHints(this);
  SetSizer(s);
  // Resize
  Layout();
  wxSize min_size = GetSizer()->GetMinSize() + GetSize() - GetClientSize();
  SetSize(830, min_size.y);
  // init lists
  game_list->showData<Game>();
  try {
    game_list->select(settings.default_game);
  } catch (FileNotFoundError const& e) {
    handle_error(e);
  }
  game_list->SetFocus();
  UpdateWindowUI(wxUPDATE_UI_RECURSE);
}

void NewSetWindow::onGameSelect(wxCommandEvent&) {
  wxBusyCursor wait;
  GameP game = game_list->getSelection<Game>(false);
  //handle_pending_errors(); // errors are ignored until set window is shown
  settings.default_game = game->name();
  stylesheet_list->showData<StyleSheet>(game->name() + _("-*"));
  stylesheet_list->select(settings.gameSettingsFor(*game).default_stylesheet);
  UpdateWindowUI(wxUPDATE_UI_RECURSE);
  // resize (yuck)
  Layout();
  wxSize min_size = GetSizer()->GetMinSize() + GetSize() - GetClientSize();
  SetSize(830,min_size.y);
}

void NewSetWindow::onStyleSheetSelect(wxCommandEvent&) {
  // store this as default selection
  GameP       game       = game_list      ->getSelection<Game>(false);
  StyleSheetP stylesheet = stylesheet_list->getSelection<StyleSheet>(false);
  //handle_pending_errors(); // errors are ignored until set window is shown
  settings.gameSettingsFor(*game).default_stylesheet = stylesheet->name();
  UpdateWindowUI(wxUPDATE_UI_RECURSE);
}

void NewSetWindow::onStyleSheetActivate(wxCommandEvent&) {
  done();
}

void NewSetWindow::onGameFilterUpdate(wxCommandEvent&) {
    if (game_list->hasSelection()) {
        GameP existingGameSelection = game_list->getSelection<Game>(false);
        game_list->setFilter(game_filter->getFilter<PackageData>());
        game_list->select(existingGameSelection->name());
    }
    else {
        game_list->setFilter(game_filter->getFilter<PackageData>());
    }
}

void NewSetWindow::onStylesheetFilterUpdate(wxCommandEvent&) {
    if (stylesheet_list->hasSelection()) {
        StyleSheetP existingStylesheetSelection = stylesheet_list->getSelection<StyleSheet>(false);
        stylesheet_list->setFilter(stylesheet_filter->getFilter<PackageData>());
        stylesheet_list->select(existingStylesheetSelection->name());
    }
    else {
        stylesheet_list->setFilter(stylesheet_filter->getFilter<PackageData>());
    }
}

void NewSetWindow::OnOK(wxCommandEvent&) {
  done();
}

void NewSetWindow::done() {
  try {
    if (!stylesheet_list->hasSelection()) return;
    StyleSheetP stylesheet = stylesheet_list->getSelection<StyleSheet>();
    set = make_intrusive<Set>(stylesheet);
    set->validate();
    EndModal(wxID_OK);
  } catch (const Error& e) {
    handle_error_now(e);
  }
}

void NewSetWindow::onUpdateUI(wxUpdateUIEvent& ev) {
  switch (ev.GetId()) {
    case ID_STYLESHEET_LIST:
      ev.Enable(game_list->hasSelection());
      break;
    case wxID_OK:
      ev.Enable(stylesheet_list->hasSelection());
      break;
  }
}

void NewSetWindow::onIdle(wxIdleEvent& ev) {
  // Stuff that must be done in the main thread
  //handle_pending_errors(); // errors are ignored until set window is shown
}

BEGIN_EVENT_TABLE(NewSetWindow, wxDialog)
  EVT_GALLERY_SELECT(ID_GAME_LIST, NewSetWindow::onGameSelect)
  EVT_GALLERY_SELECT(ID_STYLESHEET_LIST, NewSetWindow::onStyleSheetSelect)
  EVT_GALLERY_ACTIVATE(ID_STYLESHEET_LIST, NewSetWindow::onStyleSheetActivate)
  EVT_COMMAND_RANGE(ID_STYLESHEET_FILTER, ID_STYLESHEET_FILTER, wxEVT_COMMAND_TEXT_UPDATED, NewSetWindow::onStylesheetFilterUpdate)
  EVT_COMMAND_RANGE(ID_GAME_FILTER, ID_GAME_FILTER, wxEVT_COMMAND_TEXT_UPDATED, NewSetWindow::onGameFilterUpdate)
  EVT_BUTTON          (wxID_OK,            NewSetWindow::OnOK)
  EVT_UPDATE_UI       (wxID_ANY,           NewSetWindow::onUpdateUI)
  EVT_IDLE            (                    NewSetWindow::onIdle)
END_EVENT_TABLE  ()

// ----------------------------------------------------------------------------- : SelectStyleSheetWindow


StyleSheetP select_stylesheet(const Game& game, const String& failed_name) {
  SelectStyleSheetWindow wnd(nullptr, game, failed_name);
  wnd.ShowModal();
  return wnd.stylesheet;
}

SelectStyleSheetWindow::SelectStyleSheetWindow(Window* parent, const Game& game, const String& failed_name)
  : wxDialog(parent, wxID_ANY, _TITLE_("select stylesheet"), wxDefaultPosition, wxSize(830,320), wxDEFAULT_DIALOG_STYLE)
  , game(game)
{
  wxBusyCursor wait;
  // init controls
  stylesheet_list = new PackageList (this, ID_STYLESHEET_LIST);
  wxStaticText* description     = new wxStaticText(this, ID_GAME_LIST,       _LABEL_1_("stylesheet not found", failed_name));
  wxStaticText* stylesheet_text = new wxStaticText(this, ID_STYLESHEET_LIST, _LABEL_("style type"));

  stylesheet_filter = new FilterCtrl(this, ID_STYLESHEET_FILTER, _LABEL_("search stylesheet list"), _HELP_("search stylesheet list control"));
  stylesheet_filter->setFilter(stylesheet_filter_value);

  // init sizer
  wxSizer* s = new wxBoxSizer(wxVERTICAL);
    s->Add(description,     0, wxALL,                     4);
    wxSizer* s2 = new wxBoxSizer(wxHORIZONTAL);
      s2->Add(stylesheet_text, 0, wxALL & ~wxLEFT, 4);
      s2->AddStretchSpacer();
      s2->Add(stylesheet_filter, 1, wxRIGHT, 4);
    s->Add(s2, wxSizerFlags().Expand().Border(wxALL, 6));
    s->Add(stylesheet_list, 0, wxEXPAND | (wxALL & ~wxTOP), 4);
    s->Add(CreateButtonSizer(wxOK | wxCANCEL) , 0, wxEXPAND | wxALL, 8);
    s->SetSizeHints(this);
  SetSizer(s);
  // init list
  stylesheet_list->showData<StyleSheet>(game.name() + _("-*"));
  stylesheet_list->select(settings.gameSettingsFor(game).default_stylesheet);
  // Resize
  Layout();
  wxSize min_size = GetSizer()->GetMinSize() + GetSize() - GetClientSize();
  SetSize(830,min_size.y);
  UpdateWindowUI(wxUPDATE_UI_RECURSE);
}

void SelectStyleSheetWindow::onStyleSheetSelect(wxCommandEvent&) {
  //handle_pending_errors(); // errors are ignored until set window is shown
  UpdateWindowUI(wxUPDATE_UI_RECURSE);
}
void SelectStyleSheetWindow::onStyleSheetActivate(wxCommandEvent&) {
  done();
}

void SelectStyleSheetWindow::onStylesheetFilterUpdate(wxCommandEvent&) {
  if (stylesheet_list->hasSelection()) {
    StyleSheetP existingStylesheetSelection = stylesheet_list->getSelection<StyleSheet>(false);
    stylesheet_list->setFilter(stylesheet_filter->getFilter<PackageData>());
    stylesheet_list->select(existingStylesheetSelection->name());
  }
  else {
    stylesheet_list->setFilter(stylesheet_filter->getFilter<PackageData>());
  }
}

void SelectStyleSheetWindow::OnOK(wxCommandEvent&) {
  done();
}

void SelectStyleSheetWindow::done() {
  try {
    stylesheet = stylesheet_list->getSelection<StyleSheet>();
    EndModal(wxID_OK);
  } catch (const Error& e) {
    handle_error(e);
    EndModal(wxID_CANCEL);
  }
}

void SelectStyleSheetWindow::onUpdateUI(wxUpdateUIEvent& ev) {
  switch (ev.GetId()) {
    case wxID_OK:
      ev.Enable(stylesheet_list->hasSelection());
      break;
  }
}

void SelectStyleSheetWindow::onIdle(wxIdleEvent& ev) {
  // Stuff that must be done in the main thread
  //handle_pending_errors(); // errors are ignored until set window is shown
}

BEGIN_EVENT_TABLE(SelectStyleSheetWindow, wxDialog)
  EVT_GALLERY_SELECT  (ID_STYLESHEET_LIST, SelectStyleSheetWindow::onStyleSheetSelect)
  EVT_GALLERY_ACTIVATE(ID_STYLESHEET_LIST, SelectStyleSheetWindow::onStyleSheetActivate)
  EVT_COMMAND_RANGE(ID_STYLESHEET_FILTER, ID_STYLESHEET_FILTER, wxEVT_COMMAND_TEXT_UPDATED, SelectStyleSheetWindow::onStylesheetFilterUpdate)
  EVT_BUTTON          (wxID_OK,            SelectStyleSheetWindow::OnOK)
  EVT_UPDATE_UI       (wxID_ANY,           SelectStyleSheetWindow::onUpdateUI)
  EVT_IDLE            (                    SelectStyleSheetWindow::onIdle)
END_EVENT_TABLE  ()


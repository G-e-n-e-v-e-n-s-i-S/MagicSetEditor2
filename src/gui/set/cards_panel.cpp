//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gui/set/cards_panel.hpp>
#include <gui/control/image_card_list.hpp>
#include <gui/control/card_editor.hpp>
#include <gui/control/text_ctrl.hpp>
#include <gui/control/filter_ctrl.hpp>
#include <gui/about_window.hpp> // for HoverButton
#include <gui/update_checker.hpp>
#include <gui/util.hpp>
#include <data/set.hpp>
#include <data/game.hpp>
#include <data/card.hpp>
#include <data/add_cards_script.hpp>
#include <data/action/set.hpp>
#include <data/settings.hpp>
#include <util/find_replace.hpp>
#include <util/tagged_string.hpp>
#include <util/window_id.hpp>
#include <wx/splitter.h>
#include <wx/gbsizer.h>

// ----------------------------------------------------------------------------- : CardsPanel

CardsPanel::CardsPanel(Window* parent, int id)
  : SetWindowPanel(parent, id)
{
  // init controls
  editor          = new CardEditor(this, ID_EDITOR);
  link_editor     = new CardEditor(this, ID_CARD_LINK_EDITOR);
  focused_editor  = editor;
  link_viewer_1   = new CardViewer(this, ID_CARD_LINK_VIEWER);
  link_viewer_2   = new CardViewer(this, ID_CARD_LINK_VIEWER);
  link_viewer_3   = new CardViewer(this, ID_CARD_LINK_VIEWER);
  link_viewer_4   = new CardViewer(this, ID_CARD_LINK_VIEWER);
  link_relation_1 = new wxStaticText(this, ID_CARD_LINK_RELATION_1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
  link_relation_2 = new wxStaticText(this, ID_CARD_LINK_RELATION_2, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
  link_relation_3 = new wxStaticText(this, ID_CARD_LINK_RELATION_3, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
  link_relation_4 = new wxStaticText(this, ID_CARD_LINK_RELATION_4, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
  link_select     = new wxButton(this, ID_CARD_LINK_SELECT, _BUTTON_("link select"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
  link_unlink_1   = new wxButton(this, ID_CARD_LINK_UNLINK_1, _BUTTON_("unlink"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
  link_unlink_2   = new wxButton(this, ID_CARD_LINK_UNLINK_2, _BUTTON_("unlink"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
  link_unlink_3   = new wxButton(this, ID_CARD_LINK_UNLINK_3, _BUTTON_("unlink"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
  link_unlink_4   = new wxButton(this, ID_CARD_LINK_UNLINK_4, _BUTTON_("unlink"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
  splitter        = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
  card_list       = new FilteredImageCardList(splitter, ID_CARD_LIST);
  nodes_panel     = new wxPanel(splitter, wxID_ANY);
  notes           = new TextCtrl(nodes_panel, ID_NOTES, true);
  collapse_notes  = new HoverButton(nodes_panel, ID_COLLAPSE_NOTES, _("btn_collapse"), Color(), false);
  collapse_notes->SetExtraStyle(wxWS_EX_PROCESS_UI_UPDATES);
  filter          = nullptr;
  editor->next_in_tab_order = card_list;
  wxFont font = link_relation_1->GetFont();
  font.SetWeight(wxFONTWEIGHT_BOLD);
  link_relation_1->SetFont(font);
  link_relation_2->SetFont(font);
  link_relation_3->SetFont(font);
  link_relation_4->SetFont(font);
  // init sizer for notes panel
  wxSizer* sn = new wxBoxSizer(wxVERTICAL);
    wxSizer* sc = new wxBoxSizer(wxHORIZONTAL);
    sc->Add(new wxStaticText(nodes_panel, wxID_ANY, _LABEL_("card notes")), 1, wxEXPAND | wxLEFT, 2);
    sc->Add(collapse_notes, 0, wxALIGN_CENTER | wxRIGHT, 2);
  sn->Add(sc, 0, wxEXPAND, 2);
  sn->Add(notes, 1, wxEXPAND | wxTOP, 2);
  nodes_panel->SetSizer(sn);
  // init splitter
  splitter->SetMinimumPaneSize(15);
  splitter->SetSashGravity(1.0);
  splitter->SplitHorizontally(card_list, nodes_panel, -40);
  notes_below_editor = false;
  // init sizer for editors and viewers
  wxSizer* s = new wxBoxSizer(wxHORIZONTAL); // Global Sizer
    s_left = new wxBoxSizer(wxVERTICAL); // Sizer for the selected card, and it's linked cards
      wxSizer* card_and_link = new wxBoxSizer(wxHORIZONTAL);
      s_left->Add(card_and_link);
        card_and_link->Add(editor);
        wxGridBagSizer* link_boxes = new wxGridBagSizer(); // Sizer for the linked cards
        card_and_link->Add(link_boxes, 0, wxLEFT, 2);
          link_box_1 = new wxStaticBoxSizer(wxVERTICAL, this); // Box around the first linked card, it's relation, and buttons to select and unlink
          link_boxes->Add(link_box_1, wxGBPosition(0, 0), wxGBSpan(1, 1));
          link_box_2 = new wxStaticBoxSizer(wxVERTICAL, this); // Box around the second linked card, it's relation, and a button to unlink
          link_boxes->Add(link_box_2, wxGBPosition(1, 0), wxGBSpan(1, 1));
          link_box_3 = new wxStaticBoxSizer(wxVERTICAL, this); // Box around the third linked card, it's relation, and a button to unlink
          link_boxes->Add(link_box_3, wxGBPosition(0, 1), wxGBSpan(1, 1));
          link_box_4 = new wxStaticBoxSizer(wxVERTICAL, this); // Box around the fourth linked card, it's relation, and a button to unlink
          link_boxes->Add(link_box_4, wxGBPosition(1, 1), wxGBSpan(1, 1));
            wxGridBagSizer* link_grid_1 = new wxGridBagSizer(); // Sizer for the first linked card, with it's relation, and a button to unlink
            link_box_1->Add(link_grid_1);
            wxGridBagSizer* link_grid_2 = new wxGridBagSizer();
            link_box_2->Add(link_grid_2);
            wxGridBagSizer* link_grid_3 = new wxGridBagSizer();
            link_box_3->Add(link_grid_3);
            wxGridBagSizer* link_grid_4 = new wxGridBagSizer();
            link_box_4->Add(link_grid_4);
              wxSizer* link_grid_1_buttons = new wxBoxSizer(wxHORIZONTAL);
              link_grid_1->Add(link_relation_1,     wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
              link_grid_1->Add(link_viewer_1,       wxGBPosition(1, 0), wxGBSpan(1, 2));
              link_grid_1->Add(link_editor,         wxGBPosition(2, 0), wxGBSpan(1, 2));
              link_grid_1->Add(link_grid_1_buttons, wxGBPosition(0, 1), wxGBSpan(1, 1), wxALIGN_RIGHT);
                link_grid_1_buttons->Add(link_select);
                link_grid_1_buttons->Add(link_unlink_1);
              link_grid_2->Add(link_relation_2, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
              link_grid_2->Add(link_unlink_2,   wxGBPosition(0, 1), wxGBSpan(1, 1), wxALIGN_RIGHT);
              link_grid_2->Add(link_viewer_2,   wxGBPosition(1, 0), wxGBSpan(1, 2));
              link_grid_3->Add(link_relation_3, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
              link_grid_3->Add(link_unlink_3,   wxGBPosition(0, 1), wxGBSpan(1, 1), wxALIGN_RIGHT);
              link_grid_3->Add(link_viewer_3,   wxGBPosition(1, 0), wxGBSpan(1, 2));
              link_grid_4->Add(link_relation_4, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
              link_grid_4->Add(link_unlink_4,   wxGBPosition(0, 1), wxGBSpan(1, 1), wxALIGN_RIGHT);
              link_grid_4->Add(link_viewer_4,   wxGBPosition(1, 0), wxGBSpan(1, 2));
  s->Add(s_left,   0, wxEXPAND | wxRIGHT, 2);
  s->Add(splitter, 1, wxEXPAND);
  s->SetSizeHints(this);
  SetSizer(s);
  
  // init menus
  menuCard = new wxMenu();
    add_menu_item_tr(menuCard, ID_CARD_PREV, nullptr, "previous card");
    add_menu_item_tr(menuCard, ID_CARD_NEXT, nullptr, "next card");
    add_menu_item_tr(menuCard, ID_CARD_SEARCH, nullptr, "search cards");
    menuCard->AppendSeparator();
    insertManyCardsMenu = add_menu_item_tr(menuCard, ID_CARD_ADD_MULT, "card_add_multiple", "add cards");
    // NOTE: space after "Del" prevents wx from making del an accellerator
    // otherwise we delete a card when delete is pressed inside the editor
    // Adding a space never hurts, please keep it just to be safe.
    add_menu_item(menuCard, ID_CARD_ADD_CSV, "card_add_multiple", _MENU_("add card csv") + _(" "), _HELP_("add card csv"));
    add_menu_item(menuCard, ID_CARD_ADD_JSON, "card_add_multiple", _MENU_("add card json") + _(" "), _HELP_("add card json"));
    add_menu_item_tr(menuCard, ID_CARD_ADD, "card_add", "add_card");
    add_menu_item(menuCard, ID_CARD_REMOVE, "card_del", _MENU_("remove card")+_(" "), _HELP_("remove card"));
    add_menu_item(menuCard, ID_CARD_LINK, "card_link", _MENU_("link card") + _(" "), _HELP_("link card"));
    add_menu_item(menuCard, ID_CARD_AND_LINK_COPY, "card_copy", _MENU_("copy card and links") + _(" "), _HELP_("copy card and links"));
    menuCard->AppendSeparator();
    auto menuRotate = new wxMenu();
      add_menu_item_tr(menuRotate, ID_CARD_ROTATE_0, "card_rotate_0", "rotate_0", wxITEM_CHECK);
      add_menu_item_tr(menuRotate, ID_CARD_ROTATE_270, "card_rotate_270", "rotate_270", wxITEM_CHECK);
      add_menu_item_tr(menuRotate, ID_CARD_ROTATE_180, "card_rotate_180", "rotate_180", wxITEM_CHECK);
      add_menu_item_tr(menuRotate, ID_CARD_ROTATE_90, "card_rotate_90", "rotate_90", wxITEM_CHECK);
    add_menu_item_tr(menuCard, wxID_ANY, "card_rotate", "orientation", wxITEM_NORMAL, menuRotate);
    menuCard->AppendSeparator();
    // This probably belongs in the window menu, but there we can't remove the separator once it is added
    add_menu_item_tr(menuCard, ID_SELECT_COLUMNS, nullptr, "card_list_columns");
  
  menuFormat = new wxMenu();
    add_menu_item_tr(menuFormat, ID_FORMAT_BOLD, settings.darkModePrefix() + "bold", "bold", wxITEM_CHECK);
    add_menu_item_tr(menuFormat, ID_FORMAT_ITALIC, settings.darkModePrefix() + "italic", "italic", wxITEM_CHECK);
    add_menu_item_tr(menuFormat, ID_FORMAT_UNDERLINE, settings.darkModePrefix() + "underline", "underline", wxITEM_CHECK);
    add_menu_item_tr(menuFormat, ID_FORMAT_SYMBOL, settings.darkModePrefix() + "symbol", "symbols", wxITEM_CHECK);
    add_menu_item_tr(menuFormat, ID_FORMAT_REMINDER, settings.darkModePrefix() + "reminder", "reminder_text", wxITEM_CHECK);
    menuFormat->AppendSeparator();
    insertSymbolMenu = new wxMenuItem(menuFormat, ID_INSERT_SYMBOL, _MENU_("insert symbol"));
    menuFormat->Append(insertSymbolMenu);
  
  toolAddCard = nullptr;
}

void CardsPanel::updateCardCounts() {
  if (counts && card_list && set) {
    int selected = card_list->GetSelectedItemCount();
    int filtered = card_list->GetItemCount();
    int total = set->cards.size();

    if (
          selected_cards_count == selected
      &&  filtered_cards_count == filtered
      &&  total_cards_count == total
      &&  !counts->GetLabel().empty()
    ) return;

    selected_cards_count = selected;
    filtered_cards_count = filtered;
    total_cards_count = total;

    if (filtered == total) {
      counts->SetLabel(_TOOL_2_("card counts 2",
        wxString::Format(wxT("%i"), selected),
        wxString::Format(wxT("%i"), total)));
    }
    else {
      counts->SetLabel(_TOOL_3_("card counts 3",
        wxString::Format(wxT("%i"), selected),
        wxString::Format(wxT("%i"), filtered),
        wxString::Format(wxT("%i"), total)));
    }
  }
}

void CardsPanel::updateNotesPosition() {
  wxSize editor_size = editor->GetBestSize();
  int room_below_editor = GetSize().y - editor_size.y;
  bool should_be_below = room_below_editor > 100;
  // move?
  if (should_be_below && !notes_below_editor) {
    notes_below_editor = true;
    // move the notes_panel to below the editor, it gets this as its parent
    splitter->Unsplit(nodes_panel);
    nodes_panel->Reparent(this);
    s_left->Add(nodes_panel, 1, wxEXPAND | wxTOP, 2);
    collapse_notes->Hide();
    nodes_panel->Show();
  } else if (!should_be_below && notes_below_editor) {
    notes_below_editor = false;
    // move the notes_panel back to below the card list
    s_left->Detach(nodes_panel);
    nodes_panel->Reparent(splitter);
    collapse_notes->Show();
    splitter->SplitHorizontally(card_list, nodes_panel, -80);
  }
}
bool CardsPanel::Layout() {
  updateNotesPosition();
  return SetWindowPanel::Layout();
}

/*void removeInsertSymbolMenu() {
    menuFormat->Append(ID_INSERT_SYMBOL,  _(""),         _MENU_("insert symbol"));
}*/// TODO
CardsPanel::~CardsPanel() {
//  settings.card_notes_height = splitter->GetSashPosition();
  // we don't own the submenu
  wxMenu* menu = insertSymbolMenu->GetSubMenu();
  if (menu && menu->GetParent() == menuFormat) {
    menu->SetParent(nullptr);
  }
  insertSymbolMenu->SetSubMenu(nullptr); 
  // delete menus
  delete menuCard;
  delete menuFormat;
}

void CardsPanel::onChangeSet() {
  editor->setSet(set);
  link_editor->setSet(set);
  link_viewer_1->setSet(set);
  link_viewer_2->setSet(set);
  link_viewer_3->setSet(set);
  link_viewer_4->setSet(set);
  notes->setSet(set);
  card_list->setSet(set);
  
  // change insertManyCardsMenu
  delete insertManyCardsMenu->GetSubMenu();
  insertManyCardsMenu->SetSubMenu(makeAddCardsSubmenu(false));
  // re-add the menu
  menuCard->Remove(ID_CARD_ADD_MULT);
  ((wxMenu*)menuCard)->Insert(4,insertManyCardsMenu); // HACK: the position is hardcoded
  // also for the toolbar dropdown menu
  if (toolAddCard) {
    // Originally this was using the menu directly, but there are compatibility issues apparently.
    // At this point it might be possible to just store a reference to the toolbar directly instead.
    toolAddCard->GetToolBar()->SetDropdownMenu(ID_CARD_ADD, makeAddCardsSubmenu(true));
  }
}

wxMenu* CardsPanel::makeAddCardsSubmenu(bool add_single_card_option) {
  wxMenu* cards_scripts_menu = nullptr;
  // default item?
  if (add_single_card_option) {
    cards_scripts_menu = new wxMenu();
    add_menu_item_tr(cards_scripts_menu, ID_CARD_ADD, "card_add", "add_card");
    cards_scripts_menu->AppendSeparator();
  }
  // create menu for add_cards_scripts
  if (set && set->game && !set->game->add_cards_scripts.empty()) {
    int id = ID_ADD_CARDS_MENU_MIN;
    if (!cards_scripts_menu) cards_scripts_menu = new wxMenu();
    FOR_EACH(cs, set->game->add_cards_scripts) {
      cards_scripts_menu->Append(id++, cs->name, cs->description);
    }
  }
  return cards_scripts_menu;
}

// ----------------------------------------------------------------------------- : UI

void CardsPanel::initUI(wxToolBar* tb, wxMenuBar* mb) {
  // Toolbar
  add_tool_tr(tb, ID_FORMAT_BOLD, settings.darkModePrefix() + "bold", "bold", false, wxITEM_CHECK);
  add_tool_tr(tb, ID_FORMAT_ITALIC, settings.darkModePrefix() + "italic", "italic", false, wxITEM_CHECK);
  add_tool_tr(tb, ID_FORMAT_UNDERLINE, settings.darkModePrefix() + "underline", "underline", false, wxITEM_CHECK);
  add_tool_tr(tb, ID_FORMAT_SYMBOL, settings.darkModePrefix() + "symbol", "symbols", false, wxITEM_CHECK);
  add_tool_tr(tb, ID_FORMAT_REMINDER, settings.darkModePrefix() + "reminder", "reminder_text", false, wxITEM_CHECK);
  tb->AddSeparator();
  toolAddCard = add_tool_tr(tb, ID_CARD_ADD, "card_add", "add_card", false, wxITEM_DROPDOWN);
  tb->SetDropdownMenu(ID_CARD_ADD, makeAddCardsSubmenu(true));
  add_tool_tr(tb, ID_CARD_REMOVE, "card_del", "remove_card");
  add_tool_tr(tb, ID_CARD_LINK, "card_link", "link_card");
  tb->AddSeparator();
  add_tool_tr(tb, ID_CARD_ROTATE, "card_rotate", "rotate_card", false, wxITEM_DROPDOWN);
  auto menuRotate = new wxMenu();
    add_menu_item_tr(menuRotate, ID_CARD_ROTATE_0, "card_rotate_0", "rotate_0", wxITEM_CHECK);
    add_menu_item_tr(menuRotate, ID_CARD_ROTATE_270, "card_rotate_270", "rotate_270", wxITEM_CHECK);
    add_menu_item_tr(menuRotate, ID_CARD_ROTATE_180, "card_rotate_180", "rotate_180", wxITEM_CHECK);
    add_menu_item_tr(menuRotate, ID_CARD_ROTATE_90, "card_rotate_90", "rotate_90", wxITEM_CHECK);
  tb->SetDropdownMenu(ID_CARD_ROTATE, menuRotate);
  // Filter/search textbox
  tb->AddSeparator();
  assert(!filter);
  filter = new FilterCtrl(tb, ID_CARD_FILTER, _TOOL_("search cards"), _HELP_("search cards control"));
  filter->setFilter(filter_value);
  tb->AddControl(filter);
  counts = new wxStaticText(tb, ID_CARD_COUNTER, _(""));
  updateCardCounts();
  tb->AddControl(counts);
  tb->Realize();
  // Menus
  mb->Insert(2, menuCard,   _MENU_("cards"));
  mb->Insert(3, menuFormat, _MENU_("format"));
}

void CardsPanel::destroyUI(wxToolBar* tb, wxMenuBar* mb) {
  // Toolbar
  tb->DeleteTool(ID_FORMAT_BOLD);
  tb->DeleteTool(ID_FORMAT_ITALIC);
  tb->DeleteTool(ID_FORMAT_UNDERLINE);
  tb->DeleteTool(ID_FORMAT_SYMBOL);
  tb->DeleteTool(ID_FORMAT_REMINDER);
  tb->DeleteTool(ID_CARD_ADD);
  tb->DeleteTool(ID_CARD_REMOVE);
  tb->DeleteTool(ID_CARD_LINK);
  tb->DeleteTool(ID_CARD_ROTATE);
  tb->DeleteTool(ID_CARD_COUNTER);
  // remember the value in the filter control, because the card list remains filtered
  // the control is destroyed by DeleteTool
  filter_value = filter->getFilterString();
  tb->DeleteTool(filter->GetId());
  filter = nullptr;
  // HACK: hardcoded size of rest of toolbar
  tb->DeleteToolByPos(12); // delete separator
  tb->DeleteToolByPos(12); // delete separator
  tb->DeleteToolByPos(12); // delete separator
  // Menus
  mb->Remove(3);
  mb->Remove(2);
  toolAddCard = nullptr;
}

void CardsPanel::onUpdateUI(wxUpdateUIEvent& ev) {
  switch (ev.GetId()) {
    case ID_CARD_PREV:        ev.Enable(card_list->canSelectPrevious()); break;
    case ID_CARD_NEXT:        ev.Enable(card_list->canSelectNext());     break;
    case ID_CARD_ROTATE_0: case ID_CARD_ROTATE_90: case ID_CARD_ROTATE_180: case ID_CARD_ROTATE_270: {
      StyleSheetSettings& ss = settings.stylesheetSettingsFor(set->stylesheetFor(card_list->getCard()));
      int a = ev.GetId() == ID_CARD_ROTATE_0   ? 0
            : ev.GetId() == ID_CARD_ROTATE_90  ? 90
            : ev.GetId() == ID_CARD_ROTATE_180 ? 180
            :                                    270;
      ev.Check(ss.card_angle() == a);
      break;
    }
    case ID_CARD_ADD_MULT: {
      ev.Enable(insertManyCardsMenu->GetSubMenu() != nullptr);
      break;
    }
    case ID_CARD_REMOVE:        ev.Enable(card_list->canDelete());      break;
    case ID_CARD_LINK:          ev.Enable(card_list->canLink());        break;
    case ID_CARD_AND_LINK_COPY: ev.Enable(card_list->canCopy());        break;
    case ID_FORMAT_BOLD: case ID_FORMAT_ITALIC: case ID_FORMAT_UNDERLINE: case ID_FORMAT_SYMBOL: case ID_FORMAT_REMINDER: {
      if (focused_control(this) == ID_EDITOR) {
        ev.Enable(editor->canFormat(ev.GetId()));
        ev.Check (editor->hasFormat(ev.GetId()));
      } else if (focused_control(this) == ID_CARD_LINK_EDITOR) {
        ev.Enable(link_editor->canFormat(ev.GetId()));
        ev.Check (link_editor->hasFormat(ev.GetId()));
      } else {
        ev.Enable(false);
        ev.Check(false);
      }
      break;
    }
    case ID_COLLAPSE_NOTES: {
      bool collapse = notes->GetSize().y > 0;
      collapse_notes->loadBitmaps(settings.darkModePrefix() + (collapse ? _("btn_collapse") : _("btn_expand")));
      collapse_notes->SetHelpText(collapse ? _HELP_("collapse notes") : _HELP_("expand notes"));
      break;
    }
#if 0 //ifdef __WXGTK__ //crashes on GTK
    case ID_INSERT_SYMBOL: ev.Enable(false); break;
#else
    case ID_INSERT_SYMBOL: {
      wxMenu* menu = focused_editor->getMenu(ID_INSERT_SYMBOL);
      ev.Enable(menu);
      break;
    }
#endif
  }
  updateCardCounts();
}

void CardsPanel::onMenuOpen(wxMenuEvent& ev) {
  if (ev.GetMenu() != menuFormat) return;
  wxMenu* menu = focused_editor->getMenu(ID_INSERT_SYMBOL);
  if (insertSymbolMenu->GetSubMenu() != menu || (menu && menu->GetParent() != menuFormat)) {
    // re-add the menu
    menuFormat->Remove(ID_INSERT_SYMBOL);
    insertSymbolMenu->SetSubMenu(menu);
    menuFormat->Append(insertSymbolMenu);
  }
}

void CardsPanel::onCommand(int id) {
  switch (id) {
    case ID_CARD_PREV:
      // Note: Forwarded events may cause this to occur even at the top.
      if (card_list->canSelectPrevious()) card_list->selectPrevious();
      break;
    case ID_CARD_NEXT:
      // Note: Forwarded events may cause this to occur even at the bottom.
      if (card_list->canSelectNext()) card_list->selectNext();
      break;
    case ID_CARD_SEARCH:
      filter->focusAndSelect();
      break;
    case ID_CARD_ADD:
      set->actions.addAction(make_unique<AddCardAction>(*set));
      break;
    case ID_CARD_ADD_CSV:
      card_list->doAddCSV();
      break;
    case ID_CARD_ADD_JSON:
      card_list->doAddJSON();
      break;
    case ID_CARD_REMOVE:
      card_list->doDelete();
      break;
    case ID_CARD_LINK:
      card_list->doLink();
      setCard(card_list->getCard(), true);
      break;
    case ID_CARD_LINK_UNLINK_1: case ID_CARD_LINK_UNLINK_2: case ID_CARD_LINK_UNLINK_3: case ID_CARD_LINK_UNLINK_4: {
      card_list->doUnlink((
          id == ID_CARD_LINK_UNLINK_1 ? link_viewer_1
        : id == ID_CARD_LINK_UNLINK_2 ? link_viewer_2
        : id == ID_CARD_LINK_UNLINK_3 ? link_viewer_3
        :                               link_viewer_4
      )->getCard());
      setCard(card_list->getCard(), true);
      break;
    }
    case ID_CARD_LINK_SELECT: {
      setCard(link_viewer_1->getCard(), true);
      break;
    }
    case ID_CARD_AND_LINK_COPY:
      card_list->doCopyCardAndLinkedCards();
      break;
    case ID_CARD_ROTATE:
    case ID_CARD_ROTATE_0: case ID_CARD_ROTATE_90: case ID_CARD_ROTATE_180: case ID_CARD_ROTATE_270: {
      StyleSheetSettings& ss = settings.stylesheetSettingsFor(set->stylesheetFor(card_list->getCard()));
      ss.card_angle.assign(
          id == ID_CARD_ROTATE     ? fmod((360 - 90 + ss.card_angle()), 360)
        : id == ID_CARD_ROTATE_0   ? 0
        : id == ID_CARD_ROTATE_90  ? 90
        : id == ID_CARD_ROTATE_180 ? 180
        :                            270
      );
      set->actions.tellListeners(DisplayChangeAction(),true);
      break;
    }
    case ID_SELECT_COLUMNS: {
      card_list->selectColumns();
    }
    case ID_FORMAT_BOLD: case ID_FORMAT_ITALIC: case ID_FORMAT_UNDERLINE: case ID_FORMAT_SYMBOL: case ID_FORMAT_REMINDER: {
      if (focused_control(this) == ID_EDITOR) {
        editor->doFormat(id);
      }
      else if (focused_control(this) == ID_CARD_LINK_EDITOR) {
        link_editor->doFormat(id);
      }
      break;
    }
    case ID_COLLAPSE_NOTES: {
      bool collapse = notes->GetSize().y > 0;
      if (collapse) {
        splitter->SetSashPosition(-1);
        card_list->SetFocus();
      } else {
        splitter->SetSashPosition(-150);
        notes->SetFocus();
      }
      break;
    }
    case ID_CARD_FILTER: {
      // card filter has changed, update the card list
      card_list->setFilter(filter->getFilter<Card>());
      break;
    }
    default: {
      if (id >= ID_INSERT_SYMBOL_MENU_MIN && id <= ID_INSERT_SYMBOL_MENU_MAX) {
        // pass on to editor
        focused_editor->onCommand(id);
      } else if (id >= ID_ADD_CARDS_MENU_MIN && id <= ID_ADD_CARDS_MENU_MAX) {
        // add multiple cards
        AddCardsScriptP script = set->game->add_cards_scripts.at(id - ID_ADD_CARDS_MENU_MIN);
        script->perform(*set);
      }
    }
  }
}

// ----------------------------------------------------------------------------- : Actions

bool CardsPanel::wantsToHandle(const Action&, bool undone) const {
  return false;
}

// ----------------------------------------------------------------------------- : Clipboard

// determine what control to use for clipboard actions
#define CUT_COPY_PASTE(op,return) \
  int id = focused_control(this); \
  if      (id == ID_EDITOR)           { return editor->op();      } \
  else if (id == ID_CARD_LINK_EDITOR) { return link_editor->op(); } \
  else if (id == ID_CARD_LIST)        { return card_list->op();   } \
  else if (id == ID_NOTES)            { return notes->op();       } \
  else                                { return false;             }

bool CardsPanel::canCut()   const { CUT_COPY_PASTE(canCut,   return) }
bool CardsPanel::canCopy()  const { CUT_COPY_PASTE(canCopy,  return) }
void CardsPanel::doCut()          { CUT_COPY_PASTE(doCut,    return (void)) }
void CardsPanel::doCopy()         { CUT_COPY_PASTE(doCopy,   return (void)) }

// always alow pasting cards, even if something else is selected
bool CardsPanel::canPaste() const {
  if (card_list->canPaste()) return true;
  int id = focused_control(this);
  if      (id == ID_EDITOR)           return editor->canPaste();
  else if (id == ID_CARD_LINK_EDITOR) return link_editor->canPaste();
  else if (id == ID_NOTES)            return notes->canPaste();
  else                                return false;
}
void CardsPanel::doPaste() {
  if (card_list->canPaste()) {
    card_list->doPaste();
  } else {
    int id = focused_control(this);
    if      (id == ID_EDITOR)           editor->doPaste();
    else if (id == ID_CARD_LINK_EDITOR) link_editor->doPaste();
    else if (id == ID_NOTES)            notes->doPaste();
  }
}

// ----------------------------------------------------------------------------- : Text selection

bool CardsPanel::canSelectAll() const {
  CUT_COPY_PASTE(canSelectAll, return)
}

void CardsPanel::doSelectAll() {
  CUT_COPY_PASTE(doSelectAll, return (void))
}

// ----------------------------------------------------------------------------- : Searching

class CardsPanel::SearchFindInfo : public FindInfo {
public:
  SearchFindInfo(CardsPanel& panel, wxFindReplaceData& what) : FindInfo(what), panel(panel) {}
  bool handle(const CardP& card, const TextValueP& value, size_t pos, bool was_selection) override {
    // Select the card
    panel.setCard(card, true);
    return true;
  }
private:
  CardsPanel& panel;
};

class CardsPanel::ReplaceFindInfo : public FindInfo {
public:
  ReplaceFindInfo(CardsPanel& panel, wxFindReplaceData& what) : FindInfo(what), panel(panel) {}
  bool handle(const CardP& card, const TextValueP& value, size_t pos, bool was_selection) override {
    // Select the card
    panel.setCard(card, true);
    // Replace
    if (was_selection) {
      panel.editor->insert(escape(what.GetReplaceString()), _("Replace"));
      return false;
    } else {
      return true;
    }
  }
  bool searchSelection() const override { return true; }
private:
  CardsPanel& panel;
};

bool CardsPanel::doFind(wxFindReplaceData& what) {
  SearchFindInfo find(*this, what);
  return search(find, false);
}
bool CardsPanel::doReplace(wxFindReplaceData& what) {
  ReplaceFindInfo find(*this, what);
  return search(find, false);
}
bool CardsPanel::doReplaceAll(wxFindReplaceData& what) {
  return false; // TODO
}

bool CardsPanel::search(FindInfo& find, bool from_start) {
  bool include = from_start;
  CardP current = card_list->getCard();
  for (size_t i = 0 ; i < set->cards.size() ; ++i) {
    CardP card = card_list->getCard( (long) (find.forward() ? i : set->cards.size() - i - 1) );
    if (card == current) include = true;
    if (include) {
      editor->setCard(card);
      if (editor->search(find, from_start || card != current)) {
        // found a card, call handle
        return true;
      }
    }
  }
  // didn't find anything, put editor back in its previous state
  editor->setCard(current);
  return false;
}

// ----------------------------------------------------------------------------- : Selection

CardP CardsPanel::selectedCard() const {
  return card_list->getCard();
}
void CardsPanel::selectCard(const CardP& card) {
  if (!set) return; // we want onChangeSet first

  card_list->setCard(card);

  editor->setCard(card);
  vector<pair<CardP, String>> linked_cards = card->getLinkedCards(*set);
  int count = linked_cards.size();
  if (count >= 1) {
    link_box_1->Show(true);
    link_editor->setCard(linked_cards[0].first);
    link_viewer_1->setCard(linked_cards[0].first);
    link_relation_1->SetLabel(linked_cards[0].second);
    if (count == 1) {
      link_editor->Show(true);
      link_viewer_1->Show(false);
      link_select->Show(true);
      link_editor->InvalidateBestSize();
      link_relation_1->SetMaxSize(wxSize(link_editor->GetSize().x - link_unlink_1->GetSize().x, -1));
    } else {
      link_editor->Show(false);
      link_viewer_1->Show(true);
      link_select->Show(false);
      link_viewer_1->InvalidateBestSize();
      link_relation_1->SetMaxSize(wxSize(link_viewer_1->GetSize().x - link_unlink_1->GetSize().x, -1));
    }
    link_relation_1->InvalidateBestSize();
  } else {
    link_box_1->Show(false);
    link_editor->setCard(card);
    link_viewer_1->setCard(card);
    //link_relation_1->SetLabel(wxEmptyString);
  }
  if (count >= 2) {
    link_box_2->Show(true);
    link_viewer_2->setCard(linked_cards[1].first);
    link_relation_2->SetLabel(linked_cards[1].second);
    link_relation_2->SetMaxSize(wxSize(link_viewer_2->GetSize().x - link_unlink_2->GetSize().x, -1));
    link_relation_2->InvalidateBestSize();
  } else {
    link_box_2->Show(false);
    link_viewer_2->setCard(card);
    //link_relation_2->SetLabel(wxEmptyString);
  }
  if (count >= 3) {
    link_box_3->Show(true);
    link_viewer_3->setCard(linked_cards[2].first);
    link_relation_3->SetLabel(linked_cards[2].second);
    link_relation_3->SetMaxSize(wxSize(link_viewer_3->GetSize().x - link_unlink_3->GetSize().x, -1));
    link_relation_3->InvalidateBestSize();
  } else {
    link_box_3->Show(false);
    link_viewer_3->setCard(card);
    //link_relation_3->SetLabel(wxEmptyString);
  }
  if (count >= 4) {
    link_box_4->Show(true);
    link_viewer_4->setCard(linked_cards[3].first);
    link_relation_4->SetLabel(linked_cards[3].second);
    link_relation_4->SetMaxSize(wxSize(link_viewer_4->GetSize().x - link_unlink_4->GetSize().x, -1));
    link_relation_4->InvalidateBestSize();
  } else {
    link_box_4->Show(false);
    link_viewer_4->setCard(card);
    //link_relation_4->SetLabel(wxEmptyString);
  }
  if (count >= 5) {
    queue_message(MESSAGE_WARNING, "DEBUG More than 4 linked cards found for card: " + card->identification());
  }

  notes->setValue(card ? &card->notes : nullptr);

  Layout();
  updateNotesPosition();
}

void CardsPanel::selectFirstCard() {
  if (!set) return; // we want onChangeSet first
  card_list->selectFirst();
}

void CardsPanel::setCard(const CardP& card, bool event) {
  if (!set) return; // we want onChangeSet first
  card_list->setCard(card, event);
}

void CardsPanel::refreshCard(const CardP& card) {
  if (!set) return; // we want onChangeSet first
  card_list->RefreshItem(card_list->findGivenItemPos(card));
}

void CardsPanel::getCardLists(vector<CardListBase*>& out) {
  out.push_back(card_list);
}

void CardsPanel::setFocusedEditor(DataEditor* editor) {
  focused_editor = editor;
}

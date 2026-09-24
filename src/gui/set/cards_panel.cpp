//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
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
#include <wx/scrolwin.h>
#include <wx/settings.h>

// ----------------------------------------------------------------------------- : CardsPanel

CardsPanel::CardsPanel(Window* parent, int id)
  : SetWindowPanel(parent, id)
{
  // init controls
  editor          = new CardEditor(this, ID_EDITOR);
  focused_editor  = editor;
  link_scroller   = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
  link_scroller->SetScrollRate(0, 12);
  link_editor     = new CardEditor(link_scroller, ID_CARD_LINK_EDITOR);
  link_editor->is_focused = false;
  link_select     = new wxButton(link_scroller, ID_CARD_LINK_SELECT, _BUTTON_("link select"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
  // one box per possible link slot; slot 0 doubles as the full card editor
  wxFont bold_font = GetFont();
  bold_font.SetWeight(wxFONTWEIGHT_BOLD);
  link_viewers.resize(Card::MAX_LINKS);
  link_relations.resize(Card::MAX_LINKS);
  link_unlinks.resize(Card::MAX_LINKS);
  link_boxes.resize(Card::MAX_LINKS);
  for (int i = 0; i < Card::MAX_LINKS; ++i) {
    link_viewers[i] = new CardViewer(link_scroller, ID_CARD_LINK_VIEWER);
    link_viewers[i]->is_focused = false;
    link_relations[i] = new wxStaticText(link_scroller, wxID_ANY, _(""), wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
    link_relations[i]->SetFont(bold_font);
    link_unlinks[i] = new wxButton(link_scroller, wxID_ANY, _BUTTON_("unlink"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    link_unlinks[i]->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent&) { onUnlink(i); });
  }
  splitter        = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
  card_list       = new FilteredImageCardList(splitter, ID_CARD_LIST);
  nodes_panel     = new wxPanel(splitter, wxID_ANY);
  notes           = new TextCtrl(nodes_panel, ID_NOTES, true);
  collapse_notes  = new HoverButton(nodes_panel, ID_COLLAPSE_NOTES, _("btn_collapse"), Color(), false);
  collapse_notes->SetExtraStyle(wxWS_EX_PROCESS_UI_UPDATES);
  filter          = nullptr;
  editor->next_in_tab_order = card_list;
  SetDropTarget(card_list->drop_target);
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
      card_and_link = new wxBoxSizer(wxHORIZONTAL);
      s_left->Add(card_and_link);
        card_and_link->Add(editor);
        card_and_link->Add(link_scroller, 0, wxEXPAND | wxLEFT, 2);
          link_boxes_sizer = new wxGridBagSizer(); // 2-column grid of link boxes; extra rows scroll
          link_scroller->SetSizer(link_boxes_sizer);
          for (int i = 0; i < Card::MAX_LINKS; ++i) {
            // Box around the linked card, its relation, and buttons to select/unlink.
            wxStaticBoxSizer* link_box = new wxStaticBoxSizer(wxVERTICAL, link_scroller);
            link_boxes[i] = link_box;
            link_boxes_sizer->Add(link_box, wxGBPosition(i / LINK_BOX_COLUMNS, i % LINK_BOX_COLUMNS), wxGBSpan(1, 1));
            wxGridBagSizer* link_grid = new wxGridBagSizer(); // relation label / viewer / editor / buttons for this slot
            link_box->Add(link_grid);
            wxSizer* link_grid_buttons = new wxBoxSizer(wxHORIZONTAL);
            if (i == 0) link_grid_buttons->Add(link_select); // only slot 0 can show the "select" button
            link_grid_buttons->Add(link_unlinks[i]);
            link_grid->Add(link_relations[i], wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
            link_grid->Add(link_grid_buttons,  wxGBPosition(0, 1), wxGBSpan(1, 1), wxALIGN_RIGHT);
            link_grid->Add(link_viewers[i],    wxGBPosition(1, 0), wxGBSpan(1, 2));
            if (i == 0) link_grid->Add(link_editor, wxGBPosition(2, 0), wxGBSpan(1, 2)); // shown instead of link_viewers[0] when there's exactly 1 link
          }
  s->Add(s_left,   0, wxEXPAND | wxRIGHT, 2);
  s->Add(splitter, 1, wxEXPAND);
  SetSizer(s);
  s->SetSizeHints(this);
  
  // init menus
  menuCard = new wxMenu();
    add_menu_item_tr(menuCard, ID_CARD_PREV, nullptr, "previous card");
    add_menu_item_tr(menuCard, ID_CARD_NEXT, nullptr, "next card");
    add_menu_item_tr(menuCard, ID_CARD_SEARCH, nullptr, "search cards");
    menuCard->AppendSeparator();
    add_menu_item_tr(menuCard, ID_CARD_ADD, "card_add", "add_card");
    add_menu_item_tr(menuCard, ID_CARD_ADD_DOUBLE, "card_add_double", "add_card_double");
    insertManyCardsMenu = add_menu_item_tr(menuCard, ID_CARD_ADD_MULT, "card_add_multiple", "add cards", wxITEM_NORMAL, new wxMenu());
    // NOTE: space after "Del" prevents wx from making del an accellerator
    // otherwise we delete a card when delete is pressed inside the editor
    // Adding a space never hurts, please keep it just to be safe.
    add_menu_item(menuCard, ID_CARD_ADD_CSV, "card_add_multiple", _MENU_("add card csv") + _(" "), _HELP_("add card csv"));
    add_menu_item(menuCard, ID_CARD_ADD_JSON, "card_add_multiple", _MENU_("add card json") + _(" "), _HELP_("add card json"));
    add_menu_item(menuCard, ID_CARD_REMOVE, "card_del", _MENU_("remove card")+_(" "), _HELP_("remove card"));
    add_menu_item(menuCard, ID_CARD_LINK, settings.darkModePrefix() + "card_link", _MENU_("link card") + _(" "), _HELP_("link card"));
    add_menu_item(menuCard, ID_CARD_AND_LINK_COPY, "card_copy", _MENU_("copy card and links") + _(" "), _HELP_("copy card and links"));
    add_menu_item(menuCard, ID_CARD_BULK, "card_modify_multiple", _MENU_("bulk modify") + _(" "), _HELP_("bulk modify"));
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
    add_menu_item_tr(menuFormat, ID_FORMAT_FONT,          settings.darkModePrefix() + "font",          "font",          wxITEM_CHECK);
    add_menu_item_tr(menuFormat, ID_FORMAT_BOLD,          settings.darkModePrefix() + "bold",          "bold",          wxITEM_CHECK);
    add_menu_item_tr(menuFormat, ID_FORMAT_ITALIC,        settings.darkModePrefix() + "italic",        "italic",        wxITEM_CHECK);
    add_menu_item_tr(menuFormat, ID_FORMAT_UNDERLINE,     settings.darkModePrefix() + "underline",     "underline",     wxITEM_CHECK);
    add_menu_item_tr(menuFormat, ID_FORMAT_STRIKETHROUGH, settings.darkModePrefix() + "strikethrough", "strikethrough", wxITEM_CHECK);
    add_menu_item_tr(menuFormat, ID_FORMAT_COLOR,                                     "color_text",    "color_text",    wxITEM_CHECK);
    add_menu_item_tr(menuFormat, ID_FORMAT_BULLETPOINT,   settings.darkModePrefix() + "bullet_point",  "bullet_point",  wxITEM_CHECK);
    add_menu_item_tr(menuFormat, ID_FORMAT_SYMBOL,        settings.darkModePrefix() + "symbol",        "symbols",       wxITEM_CHECK);
    add_menu_item_tr(menuFormat, ID_FORMAT_REMINDER,      settings.darkModePrefix() + "reminder",      "reminder_text", wxITEM_CHECK);
    menuFormat->AppendSeparator();
    insertSymbolMenu = new wxMenuItem(menuFormat, ID_INSERT_SYMBOL, _MENU_("insert symbol"));
    menuFormat->Append(insertSymbolMenu);
  
  toolAddCard = nullptr;
}

void CardsPanel::updateCardCounts() {
  if (counts && card_list && set) {
    int total = set->cards.size();
    int selected = card_list->GetSelectedItemCount();
    int filtered = total - card_list->GetItemCount();
    int back = card_list->getHiddenBackFacesCount();

    if (
      selected_cards_count == selected
      &&  filtered_cards_count == filtered
      &&  filtered_back_cards_count == back
      &&  total_cards_count == total
      &&  !counts->GetLabel().empty()
    ) return;

    selected_cards_count = selected;
    filtered_cards_count = filtered;
    filtered_back_cards_count = back;
    total_cards_count = total;

    String selected_string = selected > 1 ?            _TOOL_1_("card counts selecteds", wxString::Format(wxT("%i"), selected)) :
                                                       _TOOL_("card counts selected");
    String filtered_string = filtered > 1 ? _(",  ") + _TOOL_1_("card counts hiddens", wxString::Format(wxT("%i"), filtered)) :
                             filtered > 0 ? _(",  ") + _TOOL_("card counts hidden") :
                                            _("");
    String back_string =     back > 1     ? _(" ") +   _TOOL_1_("card counts backs", wxString::Format(wxT("%i"), back)) :
                             back > 0     ? _(" ") +   _TOOL_("card counts back") :
                                            _("");
    String total_string =                   _(",  ") + _TOOL_1_("card counts total", wxString::Format(wxT("%i"), total));

    counts->SetLabel(selected_string + filtered_string + back_string + total_string);
  }
}

void CardsPanel::updateNotesPosition() {
  wxSize card_and_link_size = card_and_link->CalcMin();
  int room_below_editor = GetSize().y - card_and_link_size.y;
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
  if (updating_card) return false;
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
  for (int i = 0; i < Card::MAX_LINKS; ++i) {
    link_viewers[i]->setSet(set);
  }
  notes->setSet(set);
  card_list->setSet(set);
  updateLinkScrollerCap();
  
  // change insertManyCardsMenu. it seems Remove()ing and re-Insert()ing a wxMenuItem into
  // menuCard mid-list is buggy, so instead empty the submenu and rebuild it
  {
    wxMenu* add_cards_menu = insertManyCardsMenu->GetSubMenu();
    while (add_cards_menu->GetMenuItemCount() > 0) {
      add_cards_menu->Destroy(add_cards_menu->FindItemByPosition(0));
    }
    if (set && set->game && !set->game->add_cards_scripts.empty()) {
      int id = ID_ADD_CARDS_MENU_MIN;
      FOR_EACH(script, set->game->add_cards_scripts) {
        add_cards_menu->Append(id++, script->name, script->description);
      }
    }
  }
  // also for the toolbar dropdown menu
  if (toolAddCard) {
    // Originally this was using the menu directly, but there are compatibility issues apparently.
    // At this point it might be possible to just store a reference to the toolbar directly instead.
    toolAddCard->GetToolBar()->SetDropdownMenu(ID_CARD_ADD, makeAddCardsSubmenu(true));
  }
}

void CardsPanel::updateLinkScrollerCap() {
  if (link_boxes.size() < (size_t)LINK_BOX_COLUMNS) return;
  wxSize box_size(0, 0);
  for (int i = 0; i < LINK_BOX_COLUMNS; ++i) {
    wxSize s = link_boxes[i]->CalcMin();
    box_size.x = std::max(box_size.x, s.x);
    box_size.y = std::max(box_size.y, s.y);
  }
  if (box_size.x <= 0 || box_size.y <= 0) return; // boxes aren't laid out yet, try again later
  // Reserve extra width for the vertical scrollbar
  int scrollbar_width = wxSystemSettings::GetMetric(wxSYS_VSCROLL_X, link_scroller);
  link_scroller->SetMaxSize(wxSize(box_size.x * LINK_BOX_COLUMNS + scrollbar_width, box_size.y * 2));
}

wxMenu* CardsPanel::makeAddCardsSubmenu(bool add_single_card_option) {
  wxMenu* cards_scripts_menu = nullptr;
  // default item?
  if (add_single_card_option) {
    cards_scripts_menu = new wxMenu();
    add_menu_item_tr(cards_scripts_menu, ID_CARD_ADD, "card_add", "add_card");
    add_menu_item_tr(cards_scripts_menu, ID_CARD_ADD_DOUBLE, "card_add_double", "add_card_double");
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
  add_tool_tr(tb, ID_FORMAT_FONT,          settings.darkModePrefix() + "font",          "font",          false, wxITEM_CHECK);
  add_tool_tr(tb, ID_FORMAT_BOLD,          settings.darkModePrefix() + "bold",          "bold",          false, wxITEM_CHECK);
  add_tool_tr(tb, ID_FORMAT_ITALIC,        settings.darkModePrefix() + "italic",        "italic",        false, wxITEM_CHECK);
  add_tool_tr(tb, ID_FORMAT_UNDERLINE,     settings.darkModePrefix() + "underline",     "underline",     false, wxITEM_CHECK);
  add_tool_tr(tb, ID_FORMAT_STRIKETHROUGH, settings.darkModePrefix() + "strikethrough", "strikethrough", false, wxITEM_CHECK);
  add_tool_tr(tb, ID_FORMAT_COLOR,                                     "color_text",    "color_text",    false, wxITEM_CHECK);
  add_tool_tr(tb, ID_FORMAT_BULLETPOINT,   settings.darkModePrefix() + "bullet_point",  "bullet_point",  false, wxITEM_CHECK);
  add_tool_tr(tb, ID_FORMAT_SYMBOL,        settings.darkModePrefix() + "symbol",        "symbols",       false, wxITEM_CHECK);
  add_tool_tr(tb, ID_FORMAT_REMINDER,      settings.darkModePrefix() + "reminder",      "reminder_text", false, wxITEM_CHECK);
  tb->AddSeparator();
  toolAddCard = add_tool_tr(tb, ID_CARD_ADD, "card_add", "add_card", false, wxITEM_DROPDOWN);
  tb->SetDropdownMenu(ID_CARD_ADD, makeAddCardsSubmenu(true));
  add_tool_tr(tb, ID_CARD_REMOVE, "card_del", "remove_card");
  add_tool_tr(tb, ID_CARD_LINK, settings.darkModePrefix() + "card_link", "link_card");
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
  tb->DeleteTool(ID_FORMAT_FONT);
  tb->DeleteTool(ID_FORMAT_BOLD);
  tb->DeleteTool(ID_FORMAT_ITALIC);
  tb->DeleteTool(ID_FORMAT_UNDERLINE);
  tb->DeleteTool(ID_FORMAT_STRIKETHROUGH);
  tb->DeleteTool(ID_FORMAT_COLOR);
  tb->DeleteTool(ID_FORMAT_BULLETPOINT);
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
      wxMenu* add_cards_menu = insertManyCardsMenu->GetSubMenu();
      ev.Enable(add_cards_menu && add_cards_menu->GetMenuItemCount() > 0);
      break;
    }
    case ID_CARD_REMOVE:        ev.Enable(card_list->canDelete());      break;
    case ID_CARD_LINK:          ev.Enable(card_list->canLink());        break;
    case ID_CARD_AND_LINK_COPY: ev.Enable(card_list->canCopy());        break;
    case ID_FORMAT_FONT: case ID_FORMAT_BOLD: case ID_FORMAT_ITALIC: case ID_FORMAT_UNDERLINE: case ID_FORMAT_STRIKETHROUGH:
    case ID_FORMAT_COLOR: case ID_FORMAT_BULLETPOINT: case ID_FORMAT_SYMBOL: case ID_FORMAT_REMINDER: {
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
    case ID_CARD_ADD_DOUBLE: {
      vector<CardP> cards;
      cards.push_back(make_intrusive<Card>(*set->game));
      cards.push_back(make_intrusive<Card>(*set->game));
      cards[0]->linked_card_1 = cards[1]->uid;
      cards[1]->linked_card_1 = cards[0]->uid;
      cards[0]->linked_relation_1 = "Back Face";
      cards[1]->linked_relation_1 = "Front Face";
      set->actions.addAction(make_unique<AddCardAction>(ADD, *set, cards));
      break;
    }
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
    case ID_CARD_LINK_SELECT: {
      setCard(link_editor->getCard(), true);
      break;
    }
    case ID_CARD_AND_LINK_COPY:
      card_list->doCopyCardAndLinkedCards();
      break;
    case ID_CARD_BULK:
      card_list->doBulkModification();
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
      break;
    }
    case ID_FORMAT_FONT: case ID_FORMAT_BOLD: case ID_FORMAT_ITALIC: case ID_FORMAT_UNDERLINE: case ID_FORMAT_STRIKETHROUGH:
    case ID_FORMAT_COLOR: case ID_FORMAT_BULLETPOINT: case ID_FORMAT_SYMBOL: case ID_FORMAT_REMINDER: {
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

void CardsPanel::onUnlink(int index) {
  const CardP& card = card_list->getCard();
  const CardP& linked_card = index == 0 && card->getLinkedCards(*set).size() == 1 ? link_editor->getCard() : link_viewers[index]->getCard();
  card_list->doUnlink(linked_card);
  setCard(card, true);
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

// always allow pasting cards, even if something else is selected
bool CardsPanel::canPaste() const {
  if (card_list->canPaste()) return true;
  int id = focused_control(this);
  if      (id == ID_EDITOR)           return editor->canPaste();
  else if (id == ID_CARD_LINK_EDITOR) return link_editor->canPaste();
  else if (id == ID_NOTES)            return notes->canPaste();
  else                                return false;
}
void CardsPanel::doPaste() {
  if (card_list->doPaste()) return;
  
  int id = focused_control(this);
  if      (id == ID_EDITOR)           editor->doPaste();
  else if (id == ID_CARD_LINK_EDITOR) link_editor->doPaste();
  else if (id == ID_NOTES)            notes->doPaste();
}

// ----------------------------------------------------------------------------- : Text selection

bool CardsPanel::canSelectAll() const {
  CUT_COPY_PASTE(canSelectAll, return)
}

void CardsPanel::doSelectAll() {
  CUT_COPY_PASTE(doSelectAll, return (void))
}

// ----------------------------------------------------------------------------- : Resetting

bool CardsPanel::canDefaultReset() const {
  int id = focused_control(this); if (id == ID_EDITOR) {
    return editor->canDefaultReset();
  }
  else if (id == ID_CARD_LINK_EDITOR) {
    return link_editor->canDefaultReset();
  }
  else {
    return false;
  }
}

void CardsPanel::doDefaultReset() {
  int id = focused_control(this); if (id == ID_EDITOR) {
    return (void)editor->doDefaultReset();
  }
  else if (id == ID_CARD_LINK_EDITOR) {
    return (void)link_editor->doDefaultReset();
  }
  else {
    return (void)false;
  }
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
  CardP current = card_list->getCard();
  bool include = from_start || !current;
  size_t count = set->cards.size();
  for (size_t i = 0 ; i < count ; ++i) {
    CardP card = set->cards[ find.forward() ? i : count - i - 1 ];
    if (card == current) include = true;
    if (include) {
      editor->setCard(card);
      editor->updateStyles(false);
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

  updating_card = true;

  //card_list->SetFocus(); // I don't rememeber what bug this was solving, but it is preventing the search dialog from highlighting what it finds, so until I remember, this gets turned off
  card_list->setCard(card);

  editor->setCard(card);
  vector<pair<CardP, String>> linked_cards = card ? card->getLinkedCards(*set) : vector<pair<CardP, String>>();
  int count = (int)linked_cards.size();

  // when there are exactly 2 linked cards, lay them out vertically (1 column)
  int link_box_columns = (count == 2) ? 1 : LINK_BOX_COLUMNS;
  for (int i = 0; i < Card::MAX_LINKS; ++i) {
    link_boxes_sizer->Detach(link_boxes[i]);
  }
  for (int i = 0; i < Card::MAX_LINKS; ++i) {
    link_boxes_sizer->Add(link_boxes[i], wxGBPosition(i / link_box_columns, i % link_box_columns), wxGBSpan(1, 1));
  }

  for (int i = 0; i < Card::MAX_LINKS; ++i) {
    if (i >= count) {
      // this slot is unused: hide its box, and point its widgets back at the
      // selected card so they don't hang on to a stale/removed linked card
      link_boxes[i]->Show(false);
      if (i == 0) link_editor->setCard(card);
      link_viewers[i]->setCard(card);
      continue;
    }
    link_boxes[i]->Show(true);
    link_relations[i]->SetLabel(linked_cards[i].second);
    if (i == 0 && count == 1) {
      // exactly one linked card: show the full editor instead of a viewer
      link_editor->setCard(linked_cards[0].first);
      link_editor->Show(true);
      link_viewers[0]->Show(false);
      link_select->Show(true);
      link_editor->InvalidateBestSize();
      link_relations[0]->SetMaxSize(wxSize(link_editor->GetSize().x - link_unlinks[0]->GetSize().x, -1));
    } else {
      link_viewers[i]->setCard(linked_cards[i].first);
      link_viewers[i]->Show(true);
      if (i == 0) {
        link_editor->Show(false);
        link_select->Show(false);
      }
      link_viewers[i]->InvalidateBestSize();
      link_relations[i]->SetMaxSize(wxSize(link_viewers[i]->GetSize().x - link_unlinks[i]->GetSize().x, -1));
    }
    link_relations[i]->InvalidateBestSize();
  }

  notes->setValue(card ? &card->notes : nullptr);

  updating_card = false;

  // the number of visible boxes just changed, so recompute the scrollable area
  link_boxes_sizer->Layout();
  link_scroller->FitInside();
  updateLinkScrollerCap();

  Layout();
  updateNotesPosition();
  wxCommandEvent ev(EVENT_SIZE_CHANGE, GetId());
  ProcessEvent(ev);
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
  long pos = card_list->findGivenItemPos(card);
  if (pos != -1L) {
    card_list->RefreshItem(pos);
  }
}

void CardsPanel::getCardLists(vector<CardListBase*>& out) {
  out.push_back(card_list);
}

void CardsPanel::setFocusedEditor(DataEditor* editor) {
  focused_editor->is_focused = false;
  focused_editor = editor;
  focused_editor->is_focused = true;
  focused_editor->Refresh();
}

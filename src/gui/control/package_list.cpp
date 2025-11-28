//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gui/control/package_list.hpp>
#include <util/io/package_manager.hpp>
#include <util/alignment.hpp>
#include <script/profiler.hpp>
#include <gui/util.hpp>

bool PackageData::contains(QuickFilterPart const& query) const {
    if (query.match(_("full_name"), package->full_name)) return true;
    if (query.match(_("short_name"), package->short_name)) return true;
    return false;
}

// ----------------------------------------------------------------------------- : PackageList

PackageList::PackageList(Window* parent, int id, int direction, bool always_focused)
  : GalleryList(parent, id, direction, always_focused)
{
  item_size = subcolumns[0].size = wxSize(130, 150);
  SetThemeEnabled(true);
}

size_t PackageList::itemCount() const {
  return filtered_packages.size();
}

void PackageList::drawItem(DC& dc, int x, int y, size_t item) {
  dc.SetClippingRegion(x+1, y+2, item_size.x-2, item_size.y-2);
  PackageDataP& d = filtered_packages.at(item);
  RealRect rect(RealPoint(x,y),item_size);
  RealPoint pos;
  int w, h;
  // draw image
  if (d->image.Ok()) {
    dc.DrawBitmap(d->image, x + int(align_delta_x(ALIGN_CENTER, item_size.x, d->image.GetWidth())), y + 3, true);
  }
  // draw short name
  wxFont font = wxFont(11,wxFONTFAMILY_SWISS,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_BOLD,false,_("Arial"));
  dc.SetFont(font);
  dc.GetTextExtent(capitalize(d->package->short_name), &w, &h);
  pos = align_in_rect(ALIGN_CENTER, RealSize(w,h), rect);
  dc.DrawText(capitalize(d->package->short_name), max(x+1,(int)pos.x), (int)pos.y + 110);
  // draw full name
  font.SetPointSize(9);
  font.SetWeight(wxFONTWEIGHT_NORMAL);
  dc.SetFont(font);
  dc.GetTextExtent(d->package->full_name, &w, &h);
  if (w > item_size.x) {
    font.SetPointSize(max(7,9*item_size.x/w));
    dc.SetFont(font);
    dc.GetTextExtent(d->package->full_name, &w, &h);
  }
  RealPoint text_pos = align_in_rect(ALIGN_CENTER, RealSize(w,h), rect);
  dc.DrawText(d->package->full_name, max(x+1,(int)text_pos.x), (int)(text_pos.y + 130 + 9-font.GetPointSize()));
  dc.DestroyClippingRegion();
}

struct PackageList::ComparePackagePosHint {
  bool operator () (const PackageData& a, const PackageData& b) {
    // use position_hints to determine order
    if (a.package->position_hint < b.package->position_hint) return true;
    if (a.package->position_hint > b.package->position_hint) return false;
    // ensure a deterministic order: use the names
    return a.package->name() < b.package->name();
  }
};

void PackageList::showData(const String& pattern) {
  // clear
  packages.clear();
  filtered_packages.clear();
  filter.reset();
  
  // find matching packages
  vector<PackagedP> matching;
  {
    PROFILER(_("find matching packages"));
    package_manager.findMatching(pattern, matching);
  }
  FOR_EACH(p, matching) {
    // open image
    PROFILER(_("load package image"));
    auto stream = p->openIconFile();
    Image img;
    Bitmap bmp;
    if (stream && image_load_file(img, *stream)) {
      bmp = Bitmap(img);
    }
    // add to list
    packages.push_back(PackageData(p, bmp));
  }
  // sort list
  sort(packages.begin(), packages.end(), ComparePackagePosHint());

  // update list
  applyFilter();
  update();
}

void PackageList::clear() {
  packages.clear();
  filtered_packages.clear();
  filter.reset();
  update();
}

void PackageList::select(const String& name, bool send_event) {
  for (vector<PackageDataP>::const_iterator it = filtered_packages.begin() ; it != filtered_packages.end() ; ++it) {
    if (it->get()->package->name() == name) {
      subcolumns[0].selection = it - filtered_packages.begin();
      update();
      if (send_event) {
        sendEvent(EVENT_GALLERY_SELECT);
      }
      return;
    }
  }
  subcolumns[0].selection = NO_SELECTION;
  update();
  return;
}

int PackageList::requiredWidth() const {
  return (item_size.x + SPACING) * (int)itemCount();
}

void PackageList::setFilter(const PackageDataFilterP& filter) {
    this->filter = filter;
    applyFilter();
    update();
}

void PackageList::applyFilter() {
    this->filtered_packages.clear();

    FOR_EACH(p, packages) {
        if (!filter || filter->keep(p)) {
            filtered_packages.push_back(make_intrusive<PackageData>(p));
        }
    }
}

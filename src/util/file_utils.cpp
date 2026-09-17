//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/file_utils.hpp>
#include <wx/filename.h>
#include <wx/dir.h>
#include <errno.h>
#include <sys/stat.h>

// ----------------------------------------------------------------------------- : File names

String normalize_filename(const String& name) {
  wxFileName fn(name);
  fn.Normalize();
  return fn.GetFullPath();
}

String normalize_internal_filename(const String& name) {
  String ret;
  FOR_EACH_CONST(c, name) {
    if (c==_('\\')) ret += _('/');
    else            ret += toLower(c);
  }
  return ret;
}

bool ignore_file(const String& name) {
  // Files that are never part of a package,
  // i.e. random stuff the OS file manager dumps without being asked
  return name == _("Thumbs.db"); // winXP explorer thumbnails
}

bool match_filename_wildcard(const String& text, const String& pattern) {
  size_t text_index = 0, pattern_index = 0;
  // if a '*' is hit but the rest of the pattern doesn't match, we backtrack here:
  // retry with the '*' consuming one more character of text than last time.
  size_t star_pattern_index = String::npos, star_text_index = 0;
  while (text_index < text.size()) {
    if (pattern_index < pattern.size() && (pattern[pattern_index] == _('?') || pattern[pattern_index] == text[text_index])) {
      // literal character (or '?') matches the current text character
      ++text_index; ++pattern_index;
    } else if (pattern_index < pattern.size() && pattern[pattern_index] == _('*')) {
      // remember this '*' in case we need to backtrack to it
      star_pattern_index = pattern_index++;
      star_text_index = text_index;
    } else if (star_pattern_index != String::npos) {
      // mismatch, but we saw a '*' earlier: let it consume one more character and retry
      pattern_index = star_pattern_index + 1;
      text_index = ++star_text_index;
    } else {
      return false;
    }
  }
  // skip any trailing '*'s, they can match zero characters
  while (pattern_index < pattern.size() && pattern[pattern_index] == _('*')) ++pattern_index;
  return pattern_index == pattern.size();
}

String add_extension(const String& filename, String const& extension) {
  if (extension.size() <= filename.size() && is_substr(filename, filename.size() - extension.size(), extension)) {
    return filename;
  } else {
    return filename + extension;
  }
}

bool is_filename_char(Char c) {
  return isAlnum(c) || c == _(' ') || c == _('_') || c == _('-') || c == _('.');
}
bool is_filename_char_trim(Char c) {
  return c == _(' ') || c == _('.');
}

String clean_filename(const String& name) {
  String clean;
  // allow only valid characters, and remove leading whitespace
  bool start = true;
  FOR_EACH_CONST(c, name) {
    if (is_filename_char(c) && !(start && is_filename_char_trim(c))) {
      start = false;
      clean += c;
    }
  }
  // remove trailing whitespace and dots
  while (!clean.empty() && is_filename_char_trim(clean[clean.size()-1])) {
    clean.resize(clean.size()-1);
  }
  if (clean.empty()) {
    clean = _("no-name") + clean;
  }
  return clean;
}

bool resolve_filename_conflicts(wxFileName& fn, FilenameConflicts conflicts, set<String>& used) {
  switch (conflicts) {
    case CONFLICT_KEEP_OLD:
      return !fn.FileExists();
    case CONFLICT_OVERWRITE:
      return true;
    case CONFLICT_NUMBER: {
      int i = 0;
      String ext = fn.GetExt();
      while(fn.FileExists()) {
        fn.SetExt(String() << ++i << _(".") << ext);
      }
      return true;
    }
    case CONFLICT_NUMBER_OVERWRITE: {
      int i = 0;
      String ext = fn.GetExt();
      while(used.find(fn.GetFullPath()) != used.end()) {
        fn.SetExt(String() << ++i << _(".") << ext);
      }
      return true;
    }
    default: {
      throw InternalError(_("resolve_filename_conflicts: default case"));
    }
  }
}

// ----------------------------------------------------------------------------- : File info

time_t file_modified_time(const String& path) {
  // Note: wxFileName also provides a function for this, but that is very slow.
  struct stat statbuf;
  if (stat(path.mb_str(), &statbuf) != 0) {
    if (errno == ENOENT) {
      return 0;
    } else {
      throw InternalError(_("could not stat ") + path);
    }
  }
  return statbuf.st_mtime;
}

// ----------------------------------------------------------------------------- : Directories

bool create_directory(const String& path) {
  #if defined(__WXMSW__)
    return _wmkdir(path.fn_str()) == 0;
  #else
    return mkdir(path.fn_str(), 0777) == 0;
  #endif
}

bool create_parent_dirs(const String& file) {
  for (size_t pos = file.find_first_of(_("\\/"), 1) ;
       pos != String::npos ;
       pos = file.find_first_of(_("\\/"),pos+1)) {
    String part = file.substr(0,pos);
    if (!wxDirExists(part)) {
      if (!wxMkdir(part)) return false;
    }
  }
  return true;
}

// ----------------------------------------------------------------------------- : Removing

class RecursiveDeleter : public wxDirTraverser {
public:
  RecursiveDeleter(const String& start) {
    to_delete.push_back(start);
    ok = true;
  }
  
  bool ok;
  
  void remove() {
    FOR_EACH_REVERSE(dir, to_delete) {
      if (!wxRmdir(dir)) {
        ok = false;
        handle_error(_("Cannot delete ") + dir + _("\n")
          _("The remainder of the package has still been removed, if possible.\n")
          _("Other packages may have been removed, including packages that this on is dependent on. Please remove manually."));
      }
    }
  }
  
  wxDirTraverseResult OnFile(const String& filename) override {
    if (wxRemove(filename.fn_str()) != 0) {
      ok = false;
      handle_error(_("Cannot delete ") + filename + _("\n")
        _("The remainder of the package has still been removed, if possible.\n")
        _("Other packages may have been removed, including packages that this on is dependent on. Please remove manually."));
    }
    return wxDIR_CONTINUE;
  }
  wxDirTraverseResult OnDir(const String& dirname) override {
    to_delete.push_back(dirname);
    return wxDIR_CONTINUE;
  }
private:
  vector<String> to_delete;
};

bool remove_file(const String& filename) {
  // Based on wxRemoveFile (wxRemove is just wxRemoveFile with no logging)
  if (!wxFileExists(filename)) {
    // nothing there to remove, just call this once as cleanup
    return wxRemove(filename.fn_str()) == 0;
  }
  return retry_io([&]{ return wxRemove(filename.fn_str()) == 0; });
}

static bool remove_file_or_dir_once(const String& name) {
  wxLogNull noLog;
  if (wxFileExists(name)) {
    return wxRemove(name.fn_str()) == 0;
  } else if (wxDirExists(name)) {
    RecursiveDeleter rd(name);
    {
      wxDir dir(name);
      dir.Traverse(rd);
    }
    rd.remove();
    return rd.ok;
  } else {
    return true;
  }
}

bool remove_file_or_dir(const String& name) {
  return retry_io([&]{ return remove_file_or_dir_once(name); });
}

// ----------------------------------------------------------------------------- : Renaming

bool copy_file(const String& from, const String& to) {
  return retry_io([&]{ wxLogNull noLog; return wxCopyFile(from, to); });
}

static bool rename_file_or_dir_once(const String& from, const String& to) {
  wxLogNull noLog;
  create_parent_dirs(to);
  if (wxRename(from, to) == 0) return true;
  // wxRename can't move across volumes or if something inside a directory is locked, fall back to copy + delete
  if (wxDirExists(from)) {
    if (!wxFileName::Mkdir(to, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) return false;
    wxDir dir(from);
    if (!dir.IsOpened()) return false;
    wxString filename;
    bool cont = dir.GetFirst(&filename);
    bool ok = true;
    while (cont) {
      ok &= rename_file_or_dir_once(from + wxFILE_SEP_PATH + filename, to + wxFILE_SEP_PATH + filename);
      cont = dir.GetNext(&filename);
    }
    return ok && remove_file_or_dir_once(from);
  } else if (wxFileExists(from) && wxCopyFile(from, to)) {
    return wxRemove(from.fn_str()) == 0;
  }
  return false;
}

bool rename_file_or_dir(const String& from, const String& to) {
  return retry_io([&]{ return rename_file_or_dir_once(from, to); });
}

// ----------------------------------------------------------------------------- : Moving

class IgnoredMover : public wxDirTraverser {
public:
  IgnoredMover(const String& from, const String& to, const vector<String>& ignore_patterns)
    : from(from), to(to), ignore_patterns(ignore_patterns)
  {}
  wxDirTraverseResult OnFile(const String& filename) override {
    tryMove(filename);
    return wxDIR_CONTINUE;
  }
  wxDirTraverseResult OnDir(const String& dirname) override {
    return tryMove(dirname) ? wxDIR_IGNORE : wxDIR_CONTINUE;
  }
private:
  String from, to;
  const vector<String>& ignore_patterns;
  bool isIgnored(const String& from_path) const {
    // path relative to 'from', in normalized (package-internal) form
    String rel = normalize_internal_filename(from_path.substr(from.size()));
    while (!rel.empty() && rel[0] == _('/')) rel = rel.substr(1);
    for (const String& pattern : ignore_patterns) {
      if (match_filename_wildcard(rel, normalize_internal_filename(pattern))) return true;
    }
    return false;
  }
  bool tryMove(const String& from_path) {
    if (!is_substr(normalize_internal_filename(from_path),0,normalize_internal_filename(from))) {
      // This shouldn't happen
      return false;
    }
    if (!isIgnored(normalize_internal_filename(from_path))) {
      // Not one of the package's read_only_files files/patterns: this is content
      // managed by the package itself, so leave it for the fresh install to provide.
      return false;
    }
    String to_path = to + from_path.substr(from.size());
    remove_file_or_dir(to_path);
    return rename_file_or_dir(from_path, to_path);
  }
};

void move_ignored_files(const String& from_dir, const String& to_dir, const vector<String>& ignore_patterns) {
  if (ignore_patterns.empty()) return;
  if (wxDirExists(from_dir) && wxDirExists(to_dir)) {
    wxDir dir(from_dir);
    IgnoredMover im(from_dir, to_dir, ignore_patterns);
    dir.Traverse(im);
  }
}

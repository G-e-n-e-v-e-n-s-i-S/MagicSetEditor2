//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/io/package.hpp>
#include <util/io/package_manager.hpp>
#include <util/error.hpp>
#include <script/to_value.hpp> // for reflection
#include <script/profiler.hpp> // for PROFILER
#include <data/set.hpp>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>
#include <wx/dir.h>

// ----------------------------------------------------------------------------- : Package : outside

IMPLEMENT_DYNAMIC_ARG(Package*, writing_package,   nullptr);
IMPLEMENT_DYNAMIC_ARG(Package*, clipboard_package, nullptr);

Package::Package()
  : zipStream (nullptr)
{}

Package::~Package() {
  // remove any remaining temporary files
  FOR_EACH(f, files) {
    if (f.second.wasWritten()) {
      remove_file(f.second.tempName);
    }
  }
}

bool Package::isOpened() const {
  return !filename.empty();
}
bool Package::needSaveAs() const {
  return filename.empty();
}
String Package::name() const {
  // wxFileName is too slow (profiled)
  //   wxFileName fn(filename);
  //   return fn.GetName();
  size_t ext   = filename.find_last_of(_('.'));
  size_t slash = filename.find_last_of(_("/\\:"), ext);
  if      (slash == String::npos && ext == String::npos) return filename;
  else if (slash == String::npos)                        return filename.substr(0,ext);
  else if (                         ext == String::npos) return filename.substr(slash+1);
  else                                                   return filename.substr(slash+1, ext-slash-1);
}
String Package::relativeFilename() const {
  size_t slash = filename.find_last_of(_("/\\:"));
  if (slash == String::npos) return filename;
  else                       return filename.substr(slash+1);
}
const String& Package::absoluteFilename() const {
  return filename;
}

void Package::open(const String& n, bool fast) {
  assert(!isOpened()); // not already opened
  PROFILER(_("open package"));
  // get absolute path
  wxFileName fn(n);
  fn.Normalize();
  filename = fn.GetFullPath();
  // get modified time
  if (!fn.FileExists() || !fn.GetTimes(0, &modified, 0)) {
    modified = wxDateTime(time_t(0)); // long time ago
  }
  // type of package
  if (wxDirExists(filename)) {
    openDirectory(fast);
  } else if (wxFileExists(filename)) {
    openZipfile();
  } else {
    throw PackageNotFoundError(_ERROR_1_("package not found", filename), filename);
  }
}

void Package::reopen() {
  if (wxDirExists(filename)) {
    // make sure we have no zip open
    zipStream.reset();
  } else {
    // reopen only needed for zipfile
    openZipfile();
  }
}


void Package::save(bool remove_unused) {
  assert(!needSaveAs());
  saveAs(filename, remove_unused);
}

void Package::saveAs(const String& name, bool remove_unused, bool as_directory) {
  if (Set* s = dynamic_cast<Set*>(this)) s->referenceActionStackFiles();
  // type of package
  if (wxDirExists(name) || as_directory) {
    saveToDirectory(name, remove_unused, false);
  } else {
    saveToZipfile  (name, remove_unused, false);
  }
  filename = name;
  removeTempFiles(remove_unused);
  reopen();
}

void Package::saveCopy(const String& name) {
  if (Set* s = dynamic_cast<Set*>(this)) s->referenceActionStackFiles();
  saveToZipfile(name, true, true);
  clearKeepFlag();
}

void Package::removeTempFiles(bool remove_unused) {
  // cleanup : remove temp files, remove deleted files from the list
  FileInfos::iterator it = files.begin();
  while (it != files.end()) {
    if (it->second.wasWritten()) {
      // remove corresponding temp file
      remove_file(it->second.tempName);
    }
    if (!it->second.keep && remove_unused && !isIgnoredOnSave(it->first)) {
      // also remove the record of deleted files
      FileInfos::iterator to_remove = it;
      ++it;
      files.erase(to_remove);
    } else {
      // free zip entry, we will reopen the file
      it->second.keep = false;
      it->second.tempName.clear();
      delete it->second.zipEntry; 
      it->second.zipEntry = 0;
      ++it;
    }
  }
}

void Package::clearKeepFlag() {
  FOR_EACH(f, files) {
    f.second.keep = false;
  }
}

// ----------------------------------------------------------------------------- : Streams

/// Class to use as a superclass
class FileInputStream_aux {
protected:
  wxFileInputStream file_stream;
  inline FileInputStream_aux(const String& filename)
    : file_stream(filename)
  {}
};

/// Class that is a wxZipInputStream over a wxFileInput stream
/** Note that wxFileInputStream is also a base class, because it must be constructed first
*/
class ZipFileInputStream : private FileInputStream_aux, public wxZipInputStream {
public:
  ZipFileInputStream(const String& filename)
    : FileInputStream_aux(filename)
    , wxZipInputStream(file_stream)
  {}
  ZipFileInputStream(const String& filename, wxZipEntry* entry)
    : ZipFileInputStream(filename)
  {
    OpenEntry(*entry);
  }
};

/// A buffered version of wxFileInputStream
/** 2007-08-24:
*    According to profiling this gives a significant speedup
*    Bringing the avarage run time of read_utf8_line from 186k to 54k (in cpu time units)
*/
class BufferedFileInputStream : private FileInputStream_aux, public wxBufferedInputStream {
public:
  inline BufferedFileInputStream(const String& filename)
    : FileInputStream_aux(filename)
    , wxBufferedInputStream(file_stream)
  {}
};

// ----------------------------------------------------------------------------- : Package : inside

bool Package::contains(const String& file) {
  FileInfos::iterator it = files.find(normalize_internal_filename(file));
  if (it == files.end()) {
    // does it look like a relative filename?
    if (filename.find(_(".mse-")) != String::npos) {
      return false;
    }
  }

  unique_ptr<wxInputStream> stream;
  if (it != files.end() && it->second.wasWritten()) {
    // written to this file, open the temp file
    retry_io([&]{ stream = make_unique<wxFileInputStream>(it->second.tempName); return stream->IsOk(); });
  }
  else if (wxFileExists(filename + _("/") + file)) {
    // a file in directory package
    retry_io([&]{ stream = make_unique<wxFileInputStream>(filename + _("/") + file); return stream->IsOk(); });
  }
  else if (wxFileExists(filename) && it != files.end() && it->second.zipEntry) {
    // a file in a zip archive
    retry_io([&]{ stream = make_unique<ZipFileInputStream>(filename, it->second.zipEntry); return stream->IsOk(); });
  }
  else {
    // shouldn't happen, packaged changed by someone else since opening it
    return false;
  }
  return stream && stream->IsOk();
}

unique_ptr<wxInputStream> Package::openIn(const String& file) {
  if (!file.empty() && file.GetChar(0) == _('/')) {
    // absolute path, open file from another package
    Packaged* p = dynamic_cast<Packaged*>(this);
    return package_manager.openFileFromPackage(p, file).first;
  }
  FileInfos::iterator it = files.find(normalize_internal_filename(file));
  if (it == files.end()) {
    // does it look like a relative filename?
    size_t pos = filename.find(_(".mse-"));
    if (pos != String::npos) {
      // check for nested folder
      pos = filename.find_last_of(_("/\\"));
      String nestedFilename = filename + filename.SubString(pos, filename.size()) + wxFileName::GetPathSeparator() + file;
      if (wxFileExists(nestedFilename)) {
        throw PackageError(_ERROR_1_("nested folder", filename));
      }
      else {
        throw PackageNotFoundError(_ERROR_2_("file not found package like", file, filename));
      }
    }
  }
  unique_ptr<wxInputStream> stream;
  if (it != files.end() && it->second.wasWritten()) {
    // written to this file, open the temp file
    retry_io([&]{ stream = make_unique<wxFileInputStream>(it->second.tempName); return stream->IsOk(); });
  } else if (wxFileExists(filename+_("/")+file)) {
    // a file in directory package
    retry_io([&]{ stream = make_unique<wxFileInputStream>(filename+_("/")+file); return stream->IsOk(); });
  } else if (wxFileExists(filename) && it != files.end() && it->second.zipEntry) {
    // a file in a zip archive
    retry_io([&]{ stream = make_unique<ZipFileInputStream>(filename, it->second.zipEntry); return stream->IsOk(); });
  } else {
    // shouldn't happen, packaged changed by someone else since opening it
    throw FileNotFoundError(file, filename);
  }
  if (!stream || !stream->IsOk()) {
    throw FileNotFoundError(file, filename);
  } else {
    return stream;
  }
}

unique_ptr<wxOutputStream> Package::openOut(const String& file) {
  return make_unique<wxFileOutputStream>(nameOut(file));
}

String Package::nameOut(const String& file) {
  assert(wxThread::IsMain()); // Writing should only be done from the main thread
  String name = normalize_internal_filename(file);
  if (isIgnoredOnSave(name)) {
    throw PackageError(_ERROR_1_("write to read only", name));
  }
  FileInfos::iterator it = files.find(name);
  if (it == files.end()) {
    // new file
    it = addFile(name);
    it->second.created = true;
  }

  // return stream
  if (it->second.wasWritten()) {
    return it->second.tempName;
  } else {
    // create temp file
    String name = wxFileName::CreateTempFileName(_("mse"));
    it->second.tempName = name;
    return name;
  }
}

LocalFileName Package::newFileName(const String& prefix, const String& suffix) {
  assert(wxThread::IsMain()); // Writing should only be done from the main thread
  String name;
  UInt infix = 0;
  while (true) {
    // create filename
    name = prefix;
    name << ++infix;
    name += suffix;
    name = normalize_internal_filename(name);
    // check if a file with that name exists
    FileInfos::iterator it = files.find(name);
    if (it == files.end()) {
      // name doesn't exist yet
      it = addFile(name);
      it->second.created = true;
      return name;
    }
  }
}

void Package::referenceFile(const String& file) {
  if (file.empty()) return;
  FileInfos::iterator it = files.find(file);
  if (it == files.end()) throw InternalError(_("Referencing a non-existent file: ") + file + _("\nin package: ") + relativeFilename());
  it->second.keep = true;
}

// ----------------------------------------------------------------------------- : LocalFileNames and absolute file references

String Package::absoluteName(const LocalFileName& file) {
  assert(wxThread::IsMain());
  FileInfos::iterator it = files.find(normalize_internal_filename(file.fn));
  if (it == files.end()) {
    throw FileNotFoundError(file.fn, filename);
  }
  if (it->second.wasWritten()) {
    // written to this file, return the temp file
    return it->second.tempName;
  } else if (wxFileExists(filename + _("/") + file.fn)) {
    // dir package
    return filename + _("/") + file.fn;
  } else {
    // assume zip package
    return filename + _("\1") + file.fn;
  }
}
// Open a file that is in some package
unique_ptr<wxInputStream> Package::openAbsoluteFile(const String& name) {
  size_t pos = name.find_first_of(_('\1'));
  if (pos == String::npos) {
    // temp or dir file
    unique_ptr<wxFileInputStream> stream;
    retry_io([&]{ stream = make_unique<wxFileInputStream>(name); return stream->IsOk(); });
    if (!stream->IsOk()) throw FileNotFoundError(_("<unknown>"), name);
    return stream;
  } else {
    // packaged file, always in zip format
    Package p;
    p.open(name.substr(0, pos));
    return p.openIn( name.substr(pos + 1));
  }
}

String LocalFileName::toStringForWriting() const {
  if (!fn.empty() && clipboard_package()) {
    // use absolute names on clipboard
    try {
      return clipboard_package()->absoluteName(fn);
    } catch (const Error&) {
      // ignore errors
      return String();
    }
  } else if (!fn.empty() && writing_package()) {
    writing_package()->referenceFile(fn);
    return fn;
  } else {
    return fn;
  }
}

LocalFileName LocalFileName::fromReadString(String const& fn, String const& prefix, String const& suffix) {
  if (!fn.empty() && clipboard_package()) {
    // copy file into current package
    try {
      LocalFileName local_name = clipboard_package()->newFileName(_("image"), _("")); // a new unique name in the package, assume it's an image
      auto out_stream = clipboard_package()->openOut(local_name);
      auto in_stream  = Package::openAbsoluteFile(fn);
      out_stream->Write(*in_stream); // copy
      return local_name;
    } catch (const Error&) {
      // ignore errors
      return LocalFileName();
    }
  } else {
    return LocalFileName(fn);
  }
}


// ----------------------------------------------------------------------------- : Package : private

Package::FileInfo::FileInfo()
  : keep(false), created(false), zipEntry(nullptr)
{}

Package::FileInfo::~FileInfo() {
  delete zipEntry;
}

void Package::loadZipStream() {
  files.clear();
  while (true) {
    wxZipEntry* entry = zipStream->GetNextEntry();
    if (!entry) break;
    String name = normalize_internal_filename(entry->GetName(wxPATH_UNIX));
    files[name].zipEntry = entry;
  }
  zipStream->CloseEntry();
}

void Package::openDirectory(bool fast) {
  if (!fast) openSubdir(_(""));
}

void Package::openSubdir(const String& name) {
  wxDir d(filename + _("/") + name);
  if (!d.IsOpened()) return; // ignore errors here
  // find files
  String f; // filename
  for(bool ok = d.GetFirst(&f, _(""), wxDIR_FILES | wxDIR_HIDDEN) ; ok ; ok = d.GetNext(&f)) {
    if (ignore_file(f)) continue;
    // add file to list of known files
    addFile(name + f);
    // get modified time
    wxDateTime file_time = file_modified_time(filename + _("/") + name + f);
    modified = max(modified,file_time);
  }
  // find subdirs
  for(bool ok = d.GetFirst(&f, _(""), wxDIR_DIRS | wxDIR_HIDDEN) ; ok ; ok = d.GetNext(&f)) {
    if (!f.empty() && f.GetChar(0) != _('.')) {
      // skip directories starting with '.', like ., .. and .svn
      openSubdir(name+f+_("/"));
    }
  }
}

void Package::openZipfile() {
  // open stream, retry a few times
  retry_io([&]{
    zipStream = make_unique<ZipFileInputStream>(filename);
    return zipStream->IsOk();
  });
  if (!zipStream->IsOk())  throw PackageError(_ERROR_1_("package not found", filename));
  // read zip entries
  loadZipStream();
}

void Package::saveToDirectory(const String& saveAs, bool remove_unused, bool is_copy) {
  // create directory?
  create_directory(saveAs);
  // write to a directory
  VCSP vcs = getVCS();
  FOR_EACH(f, files) {
    String f_out_path = saveAs + _("/") + f.first;
    if (!f.second.keep && remove_unused && !isIgnoredOnSave(f.first)) {
      // remove files that are not to be kept
      // ignore failure (new file that is not kept)
      vcs->removeFile(f_out_path);
      continue;
    }
    // TODO: create subdirectory?
    // write file
    if (f.second.wasWritten()) {
      // move files that were updated
      remove_file(f_out_path);
      if (!(is_copy ? copy_file        (f.second.tempName, f_out_path)
        : rename_file_or_dir(f.second.tempName, f_out_path))) {
        throw PackageError(_ERROR_("unable to store file"));
      }
      if (f.second.created) {
        vcs->addFile(f_out_path);
        f.second.created = false;
      }
    } else if (filename != saveAs) {
      // save as, copy old files
      if (isZipfile()) {
        auto in_stream  = openIn(f.first);
        unique_ptr<wxFileOutputStream> out;
        retry_io([&]{
          out = make_unique<wxFileOutputStream>(f_out_path);
          return out->IsOk();
        });
        if (out->IsOk()) {
          out->Write(*in_stream);
        }
        if (!out->IsOk() || (!in_stream->Eof() && in_stream->GetLastError() != wxSTREAM_NO_ERROR)) {
          throw PackageError(_ERROR_("unable to store file"));
        }
      } else {
        if (!copy_file(filename+_("/")+f.first, f_out_path)) {
          throw PackageError(_ERROR_("unable to store file"));
        }
      }
      vcs->addFile(f_out_path);
    } else {
      // old file, just keep it
    }
  }
}

void Package::saveToZipfile(const String& saveAs, bool remove_unused, bool is_copy) {
  // create temp file names
  // tempFile is deliberately NOT saveAs + ".tmp". CreateTempFileName guarantees
  // it will be in a local drive. To create it we need to do a lot of writes, so
  // we don't want to do that on a potential cloud-synced drive.
  String tempFile    = wxFileName::CreateTempFileName(_("mse-zip"));
  String bakFile     = saveAs + _(".bak");
  String bakTempFile = saveAs + _(".bak.tmp");
  // open temp zip file
  try {
    unique_ptr<wxFileOutputStream> newFile(new wxFileOutputStream(tempFile));
    if (!newFile->IsOk()) throw PackageError(_ERROR_("unable to open output file"));
    unique_ptr<wxZipOutputStream>  newZip(new wxZipOutputStream(*newFile));
    if (!newZip->IsOk())  throw PackageError(_ERROR_("unable to open output file"));
    // copy everything to a new zip file, unless it's updated or removed
    if (zipStream) newZip->CopyArchiveMetaData(*zipStream);
    FOR_EACH(f, files) {
      if (!f.second.keep && remove_unused && !isIgnoredOnSave(f.first)) {
        // to remove a file simply don't copy it
      } else if (!is_copy && f.second.zipEntry && !f.second.wasWritten()) {
        // old file, was also in zip, not changed
        // can't do this when saving a copy, since it destroys the zip entry
        zipStream->CloseEntry();
        if (!newZip->CopyEntry(f.second.zipEntry, *zipStream)) {
          throw PackageError(
            _("Unable to save: the file\n'") + f.first + _("'\n") +
            _("could not be read from the existing package file\n'") + filename + _("'.\n\n") +
            _("This often happens when a set is stored on a cloud-synced drive.\n") +
            _("Please make sure the file is fully available offline\n") +
            _("(for example by marking it 'Always keep on this device')\n") +
            _("then try saving again. Your changes have not been lost.")
          );
        }
        f.second.zipEntry = 0;
      } else {
        // changed file, or the old package was not a zipfile
        newZip->PutNextEntry(f.first);
        auto temp_stream = openIn(f.first);
        newZip->Write(*temp_stream);
        if (!newZip->IsOk() || (!temp_stream->Eof() && temp_stream->GetLastError() != wxSTREAM_NO_ERROR)) {
          throw PackageError(
            _("Unable to save: the file\n'") + f.first + _("'\n") +
            _("could not be written to the package.\n\n") +
            _("This often happens when a set is stored on a cloud-synced drive.\n") +
            _("Please make sure the file is fully available offline\n") +
            _("(for example by marking it 'Always keep on this device')\n") +
            _("then try saving again. Your changes have not been lost.")
          );
        }
      }
    }
    // close the old file
    if (!is_copy) {
      zipStream.reset();
    }
  } catch (Error const& e) {
    // when things go wrong delete the temp file
    remove_file(tempFile);
    // and release our read handle on the original file
    if (!is_copy) {
      zipStream.reset();
    }
    throw e;
  }
  // replace the old file with the new file, in effect commiting the changes
  {
    wxLogNull noLog;
    // move .bak to .bak.temp
    bool bak_temp_created = false;
    if (wxFileExists(bakFile)) {
      remove_file(bakTempFile); // clear out any stale leftover from an earlier crash
      bak_temp_created = rename_file_or_dir(bakFile, bakTempFile);
      if (!bak_temp_created) {
        // couldn't even move .bak, bail
        throw PackageError(
          _("Unable to save to\n'") + saveAs + _("'\n") +
          _("The existing backup file\n'") + bakFile + _("'\n") +
          _("could not be accessed, likely because it is in use by another program\n") +
          _("(for example another MSE instance, a virus scanner, cloud sync, etc...).\n\n") +
          _("Your changes have not been lost, but the save did not complete.\n") +
          _("Please try saving to a different file name (use 'Save As').\n\n") +
          _("System error: ") + wxSysErrorMsg(wxSysErrorCode())
        );
      }
    }
    if (wxFileExists(saveAs)) {
      // move old .mse-set to .bak
      if (!rename_file_or_dir(saveAs, bakFile)) {
        // failed, restore .bak.tmp to .bak, bail
        if (bak_temp_created) rename_file_or_dir(bakTempFile, bakFile);
        throw PackageError(
          _("Unable to save to\n'") + saveAs + _("'\n") +
          _("The existing file could not be replaced, likely because it is in use by another program\n") +
          _("(for example another MSE instance, a virus scanner, cloud sync, etc...).\n\n") +
          _("Your changes have not been lost, but the save did not complete.\n") +
          _("Please try saving to a different file name (use 'Save As').\n\n") +
          _("System error: ") + wxSysErrorMsg(wxSysErrorCode())
        );
      }
    }
    // move .tmp to .mse-set
    if (!rename_file_or_dir(tempFile, saveAs)) {
      // failed. try to move .bak to .mse-set, otherwise there would be no .mse-set file in the folder
      if (rename_file_or_dir(bakFile, saveAs)) {
        // success, try to also restore .bak.temp to .bak
        if (bak_temp_created) rename_file_or_dir(bakTempFile, bakFile);
        // bail
        throw PackageError(
          _("Unable to save to\n'") + saveAs + _("'\n") +
          _("The new file could not be put in place, likely because it is in use by another program\n") +
          _("(for example another MSE instance, a virus scanner, cloud sync, etc...).\n\n") +
          _("Your changes have not been lost, but the save did not complete.\n") +
          _("Please try saving to a different file name (use 'Save As').\n\n") +
          _("System error: ") + wxSysErrorMsg(wxSysErrorCode())
        );
      } else {
        // could not even restore .bak, there is now no .mse-set file
        // tell the user their data is in .tmp
        throw PackageError(
          _("Something went wrong while saving to\n'") + saveAs + _("'\n") +
          _("The destination path is unavailable, likely because it is in use by another program\n") +
          _("(for example another MSE instance, a virus scanner, cloud sync, etc...).\n\n") +
          _("The previous version of the file could not be restored either, so there is currently no file at that location.\n\n") +
          _("Your changes have NOT been lost: a complete, up to date copy has been saved to\n'") + tempFile + _("'\n") +
          _("You can rename that file to\n'") + saveAs + _("'\n") +
          _("manually yourself (outside of the program) to recover this save,\n") +
          _("or try saving to a different file name (use 'Save As').\n\n") +
          _("System error: ") + wxSysErrorMsg(wxSysErrorCode())
        );
      }
    }
    // save succeeded, delete .bak.temp
    if (bak_temp_created) remove_file(bakTempFile);
  }
  // re-open zip file
  filename = saveAs;
  openZipfile();
}


Package::FileInfos::iterator Package::addFile(const String& name) {
  return files.insert(make_pair(normalize_internal_filename(name), FileInfo())).first;
}

DateTime Package::modificationTime(const pair<String, FileInfo>& fi) const {
  if (fi.second.wasWritten()) {
    return wxFileName(fi.first).GetModificationTime();
  } else if (fi.second.zipEntry) {
    return fi.second.zipEntry->GetDateTime();
  } else if (wxFileExists(filename+_("/")+fi.first)) {
    return wxFileName(filename+_("/")+fi.first).GetModificationTime();
  } else {
    return DateTime((wxLongLong)0ul);
  }
}


// ----------------------------------------------------------------------------- : Packaged

template <> void Reader::handle(PackageDependency& dep) {
  if (!isCompound()) {
    handle(dep.package);
    size_t pos = dep.package.find_first_of(_(' '));
    if (pos != String::npos) {
      dep.version = Version::fromString(dep.package.substr(pos+1));
      dep.package = dep.package.substr(0,pos);
    }
  } else {
    handle(_("package"), dep.package);
    handle(_("suggests"), dep.suggests);
    handle(_("version"), dep.version);
  }
}
template <> void Writer::handle(const PackageDependency& dep) {
  if (dep.version != Version()) {
    handle(dep.package + _(" ") + dep.version.toString());
  } else {
    handle(dep.package);
  }
}

// note: reflection must be declared before it is used
IMPLEMENT_REFLECTION(Packaged) {
  REFLECT(short_name);
  REFLECT(full_name);
  REFLECT(folder_name);
  REFLECT_N("icon", icon_filename);
  REFLECT_N("dark_icon", dark_icon_filename);
  REFLECT_NO_SCRIPT(position_hint);
  REFLECT(installer_group);
  REFLECT(version);
  REFLECT(compatible_version);
  REFLECT_NO_SCRIPT_N("depends_ons", dependencies); // hack for singular_form
  REFLECT_NO_SCRIPT(read_only_files);
}

Packaged::Packaged()
  : position_hint(100000)
  , fully_loaded(true)
{}

bool Packaged::isIgnoredOnSave(const String& file) const {
  for (const String& pattern : read_only_files) {
    if (match_filename_wildcard(file, normalize_internal_filename(pattern))) return true;
  }
  return false;
}

unique_ptr<wxInputStream> Packaged::openIconFile() {
  String filename = icon_filename;
  if (settings.darkMode() && !dark_icon_filename.empty()) {
    filename = dark_icon_filename;
  }
  if (!filename.empty()) {
    return openIn(filename);
  } else {
    return unique_ptr<wxInputStream>();
  }
}

// proxy object, that reads just the package header WITHOUT using the overloaded behaviour
struct JustAsPackageProxy {
  JustAsPackageProxy(Packaged* that) : that(that) {}
  Packaged* that;
};
template <> void Reader::handle(JustAsPackageProxy& object) {
  object.that->Packaged::reflect_impl(*this);
}

void Packaged::open(const String& package, bool just_header) {
  Package::open(package);
  fully_loaded = false;
  PROFILER(just_header ? _("open package header") : _("open package fully"));
  if (just_header) {
    // Read just the header (the part common to all Packageds)
    auto stream = openIn(typeName());
    Reader reader(*stream, this, absoluteFilename() + _("/") + typeName(), true);
    try {
      JustAsPackageProxy proxy(this);
      reader.handle_greedy(proxy);
    } catch (const ParseError& err) {
      throw FileParseError(err.what(), absoluteFilename() + _("/") + typeName()); // more detailed message
    }
  } else {
    loadFully();
  }
}

void Packaged::loadFully() {
  if (fully_loaded) return;
  dependencies.clear();
  read_only_files.clear();
  auto stream = openIn(typeName());
  Reader reader(*stream, this, absoluteFilename() + _("/") + typeName());
  try {
    reader.handle_greedy(*this);
    fully_loaded = true; // only after loading and validating succeeded, be careful with recursion!
  } catch (const ParseError& err) {
    throw FileParseError(err.what(), absoluteFilename() + _("/") + typeName()); // more detailed message
  }
}

void Packaged::save(bool remove_unused) {
  WITH_DYNAMIC_ARG(writing_package, this);
  writeFile(typeName(), *this, fileVersion());
  referenceFile(typeName());
  Package::save(remove_unused);
}
void Packaged::saveAs(const String& package, bool remove_unused, bool as_directory) {
  WITH_DYNAMIC_ARG(writing_package, this);
  writeFile(typeName(), *this, fileVersion());
  referenceFile(typeName());
  Package::saveAs(package, remove_unused, as_directory);
}
void Packaged::saveCopy(const String& package) {
  WITH_DYNAMIC_ARG(writing_package, this);
  writeFile(typeName(), *this, fileVersion());
  referenceFile(typeName());
  Package::saveCopy(package);
}

void Packaged::validate(Version) {
  folder_name = name();
  // a default for the short name
  if (short_name.empty()) {
    if (!full_name.empty()) short_name = full_name;
    else short_name = folder_name;
  }
  // check dependencies
  FOR_EACH(dep, dependencies) {
    package_manager.checkDependency(*dep, true);
  }
}

void Packaged::requireDependency(Packaged* package) {
  if (package == this) return; // dependency on self
  if (this->relativeFilename().find("mse-set") != String::npos) return; // skip checks on sets
  String n = package->relativeFilename();
  if (n.find("mse-symbol-font") != String::npos) return; // skip checks for symbol-fonts
  FOR_EACH(dep, dependencies) {
    if (dep->package == n) {
      if (package->version < dep->version) {
        queue_message(MESSAGE_WARNING,_ERROR_3_("package out of date", n, package->version.toString(), dep->version.toString()));
      } else if (package->compatible_version > dep->version) {
        queue_message(MESSAGE_WARNING,_ERROR_4_("package too new",     n, package->version.toString(), dep->version.toString(), relativeFilename()));
      } else {
        return; // ok
      }
    }
  }
  // dependency not found
  queue_message(MESSAGE_WARNING,_ERROR_4_("dependency not given", name(), package->relativeFilename(), package->relativeFilename(), package->version.toString()));
}

// ----------------------------------------------------------------------------- : IncludePackage

String IncludePackage::typeName() const { return _("include"); }
Version IncludePackage::fileVersion() const { return file_version_script; }

IMPLEMENT_REFLECTION(IncludePackage) {
  REFLECT_BASE(Packaged);
}

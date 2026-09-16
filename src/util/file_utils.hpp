//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/settings.hpp>
class wxFileName;

// ----------------------------------------------------------------------------- : File names

/// Normalize a filename as much as possible, for real files.
String normalize_filename(const String& filename);

/// Normalize a filename as much as possible, for files in packages.
String normalize_internal_filename(const String& filename);

/// Should a file with the given name be ignored in packages?
/** true for hidden OS and version control files */
bool ignore_file(const String& name);

/// Glob-style match, supporting '*' (any run of characters) and '?' (any single character)
bool match_filename_wildcard(const String& text, const String& pattern);

/// Add an extension to a filename if it is not already present
/**  add_extension("test",".txt") == "test.txt"
 *   add_extension("test.txt",".txt") == "test.txt"
 *   add_extension("test.xyz",".txt") == "test.xyz.txt"
 */
String add_extension(const String& filename, String const& extension);

/// Make sure a string is safe to use as a filename.
String clean_filename(const String& name);

/// Change the filename fn if it already exists, in the way described by conflicts.
/** Returns true if the filename should be used, false if failed. */
bool resolve_filename_conflicts(wxFileName& fn, FilenameConflicts conflicts, set<String>& used);

// ----------------------------------------------------------------------------- : File info

/// Get the last modified time of a file
time_t file_modified_time(const String& name);

// ----------------------------------------------------------------------------- : Retrying flaky file operations

/// Because of this wonderful new technology called cloud storage, drives will now routinely fail
/// basic read/write operations, so we need to retry them multiple times with exponential backoff.
struct RetryOptions {
  int max_attempts;
  int initial_delay_ms;
  inline RetryOptions(int max_attempts = 4, int initial_delay_ms = 40)
    : max_attempts(max_attempts), initial_delay_ms(initial_delay_ms)
  {}
};

template <typename Op>
bool retry_io(Op op, RetryOptions const& opts = RetryOptions()) {
  int delay = opts.initial_delay_ms;
  for (int attempt = 1; attempt <= opts.max_attempts; ++attempt) {
    bool last_attempt = attempt == opts.max_attempts;
    try {
      if (op()) return true;
      if (last_attempt) return false;
    } catch (...) {
      if (last_attempt) throw;
    }
    wxMilliSleep(delay);
    delay *= 3;
  }
  return false; // unreachable
}

// ----------------------------------------------------------------------------- : Removing and renaming

bool create_directory(const String& path);

/// Ensure that the parent directories of the given filename exist.
bool create_parent_dirs(const String& file);

/// Remove the given file
/** This is identical to wxRemoveFile, except that it doesn't show an error message if the file doesn't exist
 *  (who thought that that was a good idea?), and it retries a few times before throwing.
 */
bool remove_file(const String& file);

/// Remove the given file or directory
/** It is not an error if the file doesn't exist.
 *  Removes all files in a directory.
 *  Returns true if the file is gone or was never there to begin with
 */
bool remove_file_or_dir(const String& file);

/// Copy a file, retrying a few times.
bool copy_file(const String& from, const String& to);

/// Rename (move) a file or directory, if it fails try to copy + delete, retrying a few times.
bool rename_file_or_dir(const String& old_name, const String& new_name);

/// Move files/dirs matching one of the given wildcard patterns from one directory to another
/** Used when updating a package: files the package declares in read_only_files
 *  (see Package::isIgnoredOnSave) are not managed by the package, so they need to be
 *  moved from the old installed directory into the freshly installed one by hand.
 */
void move_ignored_files(const String& from_dir, const String& to_dir, const vector<String>& ignore_patterns);


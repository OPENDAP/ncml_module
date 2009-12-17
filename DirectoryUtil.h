//////////////////////////////////////////////////////////////////////////////
// This file is part of the "NcML Module" project, a BES module designed
// to allow NcML files to be used to be used as a wrapper to add
// AIS to existing datasets of any format.
//
// Copyright (c) 2009 OPeNDAP, Inc.
// Author: Michael Johnson  <m.johnson@opendap.org>
//
// For more information, please also see the main website: http://opendap.org/
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// Please see the files COPYING and COPYRIGHT for more information on the GLPL.
//
// You can contact OPeNDAP, Inc. at PO Box 112, Saunderstown, RI. 02874-0112.
/////////////////////////////////////////////////////////////////////////////
#ifndef __AGG_UTIL__DIRECTORY_UTIL_H__
#define __AGG_UTIL__DIRECTORY_UTIL_H__

#include <iostream>
#include <string>
#include <vector>

namespace agg_util
{
  /** Class to hold info on files as we get them. */
  class FileInfo
  {
  public:
    /** strips any trailing "/" on path.  path.empty() assumed root "/" */
    FileInfo(const std::string& path, const std::string& basename, bool isDir);
    ~FileInfo();

    /** does not include trailing "/" */
    const std::string& path() const;
    const std::string& basename() const;
    bool isDir() const;

    /** Get the path and basename as path + "/" + basename
     * We cache this after first call to allow
     * for a const reference return and optimize qsort on this since
     * it will be called a lot. */
    const std::string& getFullPath() const;

    std::string toString() const;

    /** Comparator using string::operator< on the getFullPath() result.
     * Inlined since used for sort. */
    inline bool operator<(const FileInfo& rhs) const
    {
      return (getFullPath() < rhs.getFullPath());
    }

  private:
    std::string _path; // path portion with no trailing "/"
    std::string _basename; // just the basename
    mutable std::string _fullPath; // cache of the full pathname, path + "/" + basename
    bool _isDir; // true if a directory, else a file.
    // TODO add modification times, etc. in as well.
  };

  /** Helper classes for using dirent.h, dir.h, stat.h, etc. to make
   * directory listings.
   * */
   class DirectoryUtil
   {
   public:
     DirectoryUtil();
     ~DirectoryUtil();

     /** get the current root dir */
     const std::string& getRootDir() const;

     /** Makes sure the directory exists and is readable or throws an exception exception.
       Removes any trailing slashes unless the root is "/" (default).
       If !allowRelativePaths, then: Throws a BESForbiddenError if there is a "../" in the path somewhere.
       If !allowSymLinks and one is encountered, Throws a BESNotForbiddenError exception.
       If the root path cannot be found, throws BESNotFoundError.
       */
     void setRootDir(const std::string& rootDir, bool allowRelativePaths=false, bool allowSymLinks=false);

     /** Set the filter to be used for the nexy getListingForPath() call.
      * Only files that end in _suffix_ will be returned.
      * @param suffix  suffix string to filter returned files against
      */
     void setFilterSuffix(const std::string& suffix);

     /**
      * Get a listing of all the regular files and directories in the given path,
      *  which is assumed relative to getRootDir().
      *
      * Entries are placed into one of two vectors if not null.
      * This allows just regular files to be gotten, or just subdirectories, or both.
      * @note these can be the SAME pointer, with FileInfo.isDir specifying which are which.

      * @note  Any file starting with a dot ('.') is ignored!  This include ".." and "." dirs.
      * @note symbolic links in path ARE followed now (though the root dir may not contain them)
      * TODO consider adding a flag to filter out symlinks...
      *
      * @param path the directory to list, non-recursive.
      * @param pRegularFiles vector to add the regular (not directory) files to if not null.
      * @param pDirectories vector to add the directories found in path if not null.
      */
     void getListingForPath(const std::string& path,
         std::vector<FileInfo>* pRegularFiles,
         std::vector<FileInfo>* pDirectories);

     /**
      * Get the listing for the path recursing into every directory found
      * until it bottoms out.
      * NOTE: a symlink loop will cause an exception.
      * @see getListingForPath()
      * @param path top directory to begin recursive search
      * @param pRegularFiles if not null, filled with all regular files in filesystem subtree.
      * @param pDirectories if not null, filled in with all directories in filesystem subtree.
      */
     void getListingForPathRecursive(const std::string& path,
              std::vector<FileInfo>* pRegularFiles,
              std::vector<FileInfo>* pDirectories);

     /** Get recursive listing of all regular files in the directory subtree.
      * @see getListingForPathRecursive()
     * @param path top directory to search
     * @param rRegularFiles where to place all the files.
     */
     void getListingOfRegularFilesRecursive(const std::string& path,
         std::vector<FileInfo>& rRegularFiles);

     /** Is there a "../" in path? */
     static bool hasRelativePath(const std::string& path);

     /** mutate to remove all trailing "/" */
     static void removeTrailingSlashes(std::string& path);

     /** mutate to remove and preceding (in the front) "/" */
     static void removePrecedingSlashes(std::string& path);

     /** Print the list of files to the stream. */
     static void printFileInfoList(std::ostream& os, const std::vector<FileInfo>& listing);

     /** Just dump to the BESDebug channel _sDebugChannel for debugging */
     static void printFileInfoList(const std::vector<FileInfo>& listing);

     /** Gets the BES root directory by checking the bes.conf
      * settings for BES.
      * Returns: value for key == "BES.Catalog.catalog.RootDirectory"
      * if it exists, else the value for "BES.Data.RootDirectory"
      * if that doesn't exist either, simply returns the filesystem
      * root "/".
      * @return the root path
      */
     static std::string getBESRootDir();

     static bool matchesSuffix(const std::string filename, const std::string& suffix);

   private: // helper methods

     /** If opendir() fails, this uses errno to throw a BESForbiddenError,
      * BESNotFoundError, or BESInternalError with the problem.
      */
     void throwErrorForOpendirFail(const std::string& fullPath);

   private:

     // The search rootdir with no trailing slash
     // defaults to "/" if not set.
     std::string _rootDir;

     // if !empty(), files returned will end in this suffix.
     std::string _suffix;

     // Name to use in BESDEBUG channel for this class.
     static const std::string _sDebugChannel;
   };
}

#endif /* __AGG_UTIL__DIRECTORY_UTIL_H__ */

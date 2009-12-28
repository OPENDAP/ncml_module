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
#include "config.h"

#include "ScanElement.h"

#include "AggregationElement.h"
#include "DirectoryUtil.h" // agg_util
#include "NCMLDebug.h"
#include "NCMLParser.h"
#include "NetcdfElement.h"
#include "RCObject.h"
#include "SimpleTimeParser.h"
#include "XMLHelpers.h"

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <algorithm> // std::sort
#include <cstring>
#include <cerrno>
#include <iostream>
#include <sstream>

// libdap
#include "Error.h"


using agg_util::FileInfo;
using agg_util::DirectoryUtil;

namespace ncml_module
{
  const string ScanElement::_sTypeName = "scan";
  const vector<string> ScanElement::_sValidAttrs = getValidAttributes();

  ScanElement::ScanElement()
  : NCMLElement(0)
  , _location("")
  , _suffix("")
  , _regExp("")
  , _subdirs("")
  , _olderThan("")
  , _dateFormatMark("")
  {
  }

  ScanElement::ScanElement(const ScanElement& proto)
  : NCMLElement(0)
  , _location(proto._location)
  , _suffix(proto._suffix)
  , _regExp(proto._regExp)
  , _subdirs(proto._subdirs)
  , _olderThan(proto._olderThan)
  , _dateFormatMark(proto._dateFormatMark)
  {
  }

  ScanElement::~ScanElement()
  {
  }

  const string&
  ScanElement::getTypeName() const
  {
    return _sTypeName;
  }

  ScanElement*
  ScanElement::clone() const
  {
    return new ScanElement(*this);
  }

  void
  ScanElement::setAttributes(const XMLAttributeMap& attrs)
  {
    _location = attrs.getValueForLocalNameOrDefault("location", "");
    _suffix = attrs.getValueForLocalNameOrDefault("suffix", "");
    _regExp = attrs.getValueForLocalNameOrDefault("regExp", "");
    _subdirs = attrs.getValueForLocalNameOrDefault("subdirs", "true");
    _olderThan = attrs.getValueForLocalNameOrDefault("olderThan", "");
    _dateFormatMark = attrs.getValueForLocalNameOrDefault("dateFormatMark", "");
    _enhance = attrs.getValueForLocalNameOrDefault("enhance", "");

    // default is to print errors and throw which we want.
    validateAttributes(attrs, _sValidAttrs);

    // Until we implement them, we'll throw parse errors for those not yet implemented.
    throwOnUnhandledAttributes();
  }

  void
  ScanElement::handleBegin()
  {
    if (!_parser->isScopeAggregation())
      {
        THROW_NCML_PARSE_ERROR(line(),
            "ScanElement: " + toString() + " "
            "was not the direct child of an <aggregation> element as required!");
      }
  }

  void ScanElement::handleContent(const string& content)
  {
    // shouldn't be any, use the super impl to throw if not whitespace.
    NCMLElement::handleContent(content);
  }

  void ScanElement::handleEnd()
  {
    // Get to the our parent aggregation so we can add
    NetcdfElement* pCurrentDataset = _parser->getCurrentDataset();
    VALID_PTR(pCurrentDataset);
    AggregationElement* pParentAgg = pCurrentDataset->getChildAggregation();
    NCML_ASSERT_MSG(pParentAgg, "ScanElement::handleEnd(): Couldn't"
        " find the the child aggregation of the current dataset, which is "
        "supposed to be our parent!");
    pParentAgg->addScanElement(this);
  }

  string
  ScanElement::toString() const
  {
    return "<" + _sTypeName + " " +
        "location=\"" + _location + "\" " + // always print this one even in empty.
        printAttributeIfNotEmpty("suffix", _suffix) +
        printAttributeIfNotEmpty("regExp", _regExp) +
        printAttributeIfNotEmpty("subdirs", _subdirs) +
        printAttributeIfNotEmpty("olderThan", _olderThan) +
        printAttributeIfNotEmpty("dateFormatMark", _dateFormatMark) +
        ">";
  }

  bool
  ScanElement::shouldScanSubdirs() const
  {
    return (_subdirs == "true");
  }

  long
  ScanElement::getOlderThanAsSeconds() const
  {
    if (_olderThan.empty())
      {
        return 0L;
      }

    long secs = 0;
    bool success = agg_util::SimpleTimeParser::parseIntoSeconds(secs, _olderThan);
    if (!success)
      {
        THROW_NCML_PARSE_ERROR(line(),
            "Couldn't parse the olderThan attribute!  Expect a string of the form: "
            "\"%d %units\" where %d is a number and %units is a time unit string such as "
            " \"hours\" or \"s\".");
      }
    else
      {
        return secs;
      }
  }

  void
  ScanElement::getDatasetList(vector<NetcdfElement*>& datasets) const
  {
    // Use BES root as our root
    DirectoryUtil scanner;
    scanner.setRootDir(scanner.getBESRootDir());

    BESDEBUG("ncml", "Scan will be relative to the BES root data path = " <<
        scanner.getRootDir() << endl);

    setupFilters(scanner);

    vector<FileInfo> files;
    //vector<FileInfo> dirs;
    try // catch BES errors to give more context,,,,
    {
      // Call the right version depending on setting of subtree recursion.
      if (shouldScanSubdirs())
        {
          scanner.getListingOfRegularFilesRecursive(_location, files);
        }
      else
        {
          scanner.getListingForPath(_location, &files, 0);
        }
    }
    catch (BESNotFoundError& ex)
    {
      ostringstream oss;
      oss << "In processing " << toString() << " we got a BESNotFoundError with msg=";
      ex.dump(oss);
      oss << " Perhaps a path is incorrect?" << endl;
      THROW_NCML_PARSE_ERROR(line(), oss.str());
    }
    // Let the others percolate...  Internal errors
    // and Forbidden are pretty clear and likely not a typo
    // in the NCML like NotFound could be.

    // Sort the filenames here (based on full path) since FileInfo::less uses that.
    // Multiple scan elements sequentially or explicit data impose their own partial ordering,
    std::sort(files.begin(), files.end());

    BESDEBUG("ncml", "Scan " << toString() << " returned matching regular files (sorted on fullPath): " << endl);
    DirectoryUtil::printFileInfoList(files);
    //BESDEBUG("ncml", "Scan of location=" << _location << " returned directories: " << endl);
    //DirectoryUtil::printFileInfoList(dirs);

    // Turn the file list into NetcdfElements
    // created from the parser's factory so they
    // get added to its memory pool
    XMLAttributeMap attrs;
    for (vector<FileInfo>::const_iterator it = files.begin();
        it != files.end();
        ++it)
      {
        // start fresh
        attrs.clear();

        // The path to the file, relative to the BES root as needed.
        attrs.addAttribute(XMLAttribute("location", it->getFullPath()));

        // Make the dataset using the parser so it's in the parser memory pool.
        RCPtr<NCMLElement> dataset = _parser->_elementFactory.makeElement("netcdf", attrs, *_parser);

        // Up the ref count (since it's in an RCPtr) and add to the result vector
        datasets.push_back(static_cast<NetcdfElement*>(dataset.refAndGet()));
      }
  }

  void
  ScanElement::setupFilters(agg_util::DirectoryUtil& scanner) const
  {
    // If we have a suffix, set the filter.
    if (!_suffix.empty())
      {
        BESDEBUG("ncml", "Scan will filter against suffix=\"" << _suffix << "\"" << endl);
          scanner.setFilterSuffix(_suffix);
      }

    if (!_regExp.empty())
      {
        BESDEBUG("ncml", "Scan will filter against the regExp=\"" << _regExp << "\"" << endl);

        // If there's a problem compiling it, we'll know now.
        // So catch it and wrap it as a parse error, which tecnically it is.
        try
        {
          scanner.setFilterRegExp(_regExp);
        }
        catch (libdap::Error& err)
        {
          THROW_NCML_PARSE_ERROR(line(),
              "There was a problem compiling the regExp=\"" + _regExp +
              "\"  : "
              + err.get_error_message());
        }
      }

    if (!_olderThan.empty())
      {
        long secs = getOlderThanAsSeconds();
        struct timeval tvNow;
        gettimeofday(&tvNow, 0);
        long cutoffTime = tvNow.tv_sec - secs;
        scanner.setFilterModTimeOlderThan(static_cast<time_t>(cutoffTime));
        BESDEBUG("ncml", "Setting scan filter modification time using duration: "
            << secs << " from the olderThan attribute=\"" << _olderThan << "\""
            " The cutoff modification time based on now is: " <<
            getTimeAsString(cutoffTime) << endl);
      }
  }

  vector<string>
  ScanElement::getValidAttributes()
  {
    vector<string> attrs;
    attrs.push_back("location");
    attrs.push_back("suffix");
    attrs.push_back("regExp");
    attrs.push_back("subdirs");
    attrs.push_back("olderThan");
    attrs.push_back("dateFormatMark");
    attrs.push_back("enhance"); // it's in the schema, but we don't plan to support it
    return attrs;
  }

  void
  ScanElement::throwOnUnhandledAttributes()
  {
    if (!_dateFormatMark.empty())
      {
        THROW_NCML_PARSE_ERROR(line(), "ScanElement: Sorry, dateFormatMark attribute is not yet supported.");
      }
    if (!_enhance.empty())
      {
        THROW_NCML_PARSE_ERROR(line(), "ScanElement: Sorry, enhance attribute is not yet supported.");
      }
  }

  std::string
  ScanElement::getTimeAsString(time_t theTime)
  {
    struct tm* pTM = gmtime(&theTime);
    char buf[128];
    // this should be "Year-Month-Day Hour:Minute:Second"
    strftime(buf, 128, "%F %T", pTM);
    return string(buf);
  }

}

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
#ifndef __NCML_MODULE__SCAN_ELEMENT_H__
#define __NCML_MODULE__SCAN_ELEMENT_H__

#include "NCMLElement.h"
#include "AggMemberDataset.h"

namespace ncml_module
{
  // FDecls
  class NetcdfElement;

  /**
   * Implementation of the <scan> element used to scan directories
   * to create the set of files for an aggregation.
   */
  class ScanElement : public NCMLElement
  {
  public: // class vars
    // Name of the element
    static const string _sTypeName;

      // All possible attributes for this element.
    static const vector<string> _sValidAttrs;

  private:
      ScanElement& operator=(const ScanElement& rhs); // disallow

  public:
    ScanElement();
    ScanElement(const ScanElement& proto);
    virtual ~ScanElement();

    virtual const string& getTypeName() const;
    virtual ScanElement* clone() const; // override clone with more specific subclass
    virtual void setAttributes(const XMLAttributeMap& attrs);
    virtual void handleBegin();
    virtual void handleContent(const string& content);
    virtual void handleEnd();
    virtual string toString() const;

    /** is the subdirs attribute true? */
    bool shouldScanSubdirs() const;

    /**
     * Actually perform the filesystem scan based
     * on the specified attributes (suffix, subdirs, etc).
     *
     * Fill in the vector with matching datasets, sorted
     * by the filename.
     *
     * NOTE: The members added to this vector will be ref()'d
     * so the caller needs to make sure to deref them!
     *
     * @param datasets The vector to add the datasets to.
     */
    void getDatasetList(vector<NetcdfElement*>& datasets) const;

  private: // internal methods

    static vector<string> getValidAttributes();

    /** throw a parse error for non-empty attributes we don't handle yet */
    void throwOnUnhandledAttributes();

  private: // data rep
    string _location;
    string _suffix;
    string _regExp;
    string _subdirs;
    string _olderThan;
    string _dateFormatMark;
    // string _enhance; // we're not implementing this one now.
  };

}

#endif /* __NCML_MODULE__SCAN_ELEMENT_H__ */

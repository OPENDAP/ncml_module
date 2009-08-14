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
#ifndef __NCML_MODULE__DIMENSION_ELEMENT_H__
#define __NCML_MODULE__DIMENSION_ELEMENT_H__

#include "NCMLElement.h"
#include <string>
using std::string;

namespace ncml_module
{
  class NCMLParser;

  /**
   * @brief Class for parsing and representing <dimension> elements.
   *
   * In the initial version of the module, we will not be supporting
   * many of the attributes.  In particular, we currently support:
   *
   * dimension@name:    the name of the dimension, which must be unique in
   *                    the current containing netcdf element (we don't
   *                    handle group namespaces yet).
   *
   * dimension@length:  the size of this dimension as an unsigned int.
   *
   * The other attributes, namely one of { orgName, isUnlimited,
   * isVariableLength, isShared } will not be supported in this version
   * of the module.
   */
  class DimensionElement : public ncml_module::NCMLElement
  {
  public:
    static const string _sTypeName;

    DimensionElement();
    DimensionElement(const DimensionElement& proto);
    virtual ~DimensionElement();
    virtual const string& getTypeName() const;
    virtual DimensionElement* clone() const; // override clone with more specific subclass
    virtual void setAttributes(const AttributeMap& attrs);
    virtual void handleBegin(NCMLParser& p);
    virtual void handleContent(NCMLParser& p, const string& content);
    virtual void handleEnd(NCMLParser& p);
    virtual string toString() const;

    const string& name() const { return _name; }
    const string& length() const { return _length; }

    /** Parsed version of length() */
    unsigned int getLengthNumeric() const { return _size; }

  private:

    /** Fill in _size from the string in _length.
     * @exception Throws BESSyntaxUserError if the token in _length
     *            cannot be successfully parsed as an unsigned int.
     */
    void parseAndCacheSize();

    /** Make sure they didn't set any attributes we can't handle.
     * @exception Throw BESSyntaxUserError if there's an attribute we can't handle.
     * */
    void validateOrThrow();

  private:
    string _name;
    string _length;
    string _orgName; // unused
    string _isUnlimited; // unused
    string _isShared; // unused
    string _isVariableLength; // unused

    unsigned int _size; // parsed version of _length, cached.
  };

}

#endif /* __NCML_MODULE__DIMENSION_ELEMENT_H__ */
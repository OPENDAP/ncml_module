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
#include "DimensionElement.h"
#include "NCMLDebug.h"
#include "NCMLParser.h"
#include "NCMLUtil.h"
#include <sstream>

using std::string;
using std::stringstream;

namespace ncml_module
{
  // the parse name of the element
  const string DimensionElement::_sTypeName = "dimension";


  DimensionElement::DimensionElement()
  : NCMLElement()
  , _name("")
  , _length("0")
  , _orgName("")
  , _isUnlimited("")
  , _isShared("")
  , _isVariableLength("")
  , _size(0)
  {
  }

  DimensionElement::DimensionElement(const DimensionElement& proto)
  : NCMLElement(proto)
  {
    _name = proto._name;
    _length = proto._length;
    _orgName = proto._orgName;
    _isUnlimited = proto._isUnlimited;
    _isShared = proto._isShared;
    _isVariableLength = proto._isVariableLength;
    _size = proto._size;
  }

  DimensionElement::~DimensionElement()
  {
  }

  const string& DimensionElement::getTypeName() const
  {
    return _sTypeName;
  }

  DimensionElement*
  DimensionElement::clone() const
  {
    return new DimensionElement(*this);
  }

  void
  DimensionElement::setAttributes(const AttributeMap& attrs)
  {
    _name = NCMLUtil::findAttrValue(attrs, "name");
    _length = NCMLUtil::findAttrValue(attrs, "length");
    _orgName = NCMLUtil::findAttrValue(attrs, "orgName");
    _isUnlimited = NCMLUtil::findAttrValue(attrs, "isUnlimited");;
    _isShared = NCMLUtil::findAttrValue(attrs, "isShared");;
    _isVariableLength = NCMLUtil::findAttrValue(attrs, "isVariableLength");

    parseAndCacheSize();
    validateOrThrow();
  }

  void DimensionElement::handleBegin(NCMLParser& p)
  {
    BESDEBUG("ncml", "DimensionElement::handleBegin called...");

    // Make sure we're placed at a valid parse location.
    // Direct child of <netcdf> only now since we dont handle <group>
    if (!p.isScopeNetcdf())
      {
        THROW_NCML_PARSE_ERROR("Got dimension element = " + toString() +
            " at an invalid parse location.  Expected it as a direct child of <netcdf> element only." +
            " scope=" + p.getScopeString());
      }

    // Make sure the name is unique at this parse level or exception.
    const DimensionElement* pExistingDim = p.getDimension(_name);
    if (pExistingDim)
      {
        THROW_NCML_PARSE_ERROR("Tried at add dimension " + toString() +
            " but a dimension with name=" + _name +
            " already exists in this scope=" + p.getScopeString());
      }

    // Add a copy in since the parser cleans up this itself after handleEnd()
    p.addDimension(this->clone());
  }

  void
  DimensionElement::handleContent(NCMLParser& /* p */, const string& content)
  {
    // BESDEBUG("ncml", "DimensionElement::handleContent called...");
    if (!NCMLUtil::isAllWhitespace(content))
      {
        THROW_NCML_PARSE_ERROR("Got illegal (non-whitespace) content in element " + toString());
      }
  }

  void
  DimensionElement::handleEnd(NCMLParser& /* p */)
  {
    BESDEBUG("ncml", "DimensionElement::handleEnd called...");
  }

  string
  DimensionElement::toString() const
  {
    string ret = "<" + _sTypeName + " ";
    ret += NCMLElement::printAttributeIfNotEmpty("name", _name);
    ret += NCMLElement::printAttributeIfNotEmpty("length", _length);
    ret += NCMLElement::printAttributeIfNotEmpty("isShared", _isShared);
    ret += NCMLElement::printAttributeIfNotEmpty("isVariableLength", _isVariableLength);
    ret += NCMLElement::printAttributeIfNotEmpty("isUnlimited", _isUnlimited);
    ret += NCMLElement::printAttributeIfNotEmpty("orgName", _orgName);
    ret += " >";
    return ret;
  }

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////// PRIVATE IMPL

  void
  DimensionElement::parseAndCacheSize()
  {
    stringstream sis;
    sis.str(_length);
    sis >> _size;
    if (sis.fail())
      {
        THROW_NCML_PARSE_ERROR("Element " + toString() + " failed to parse the length attribute into a proper unsigned int!");
      }
  }

  void
  DimensionElement::validateOrThrow()
  {
    // Perhaps we want to warn in BESDEBUG rather than error, but I'd rather be explicit for now.
    if (!_isShared.empty() ||
        !_isUnlimited.empty() ||
        !_isVariableLength.empty() ||
        !_orgName.empty() )
      {
        THROW_NCML_PARSE_ERROR("Dimension element " + toString() + " has unexpected unimplemented attributes. "
                               "This version of the module only handles name and length.");
      }
  }

}

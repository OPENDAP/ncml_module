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
#include "NetcdfElement.h"
#include <sstream>

using std::string;
using std::stringstream;

namespace ncml_module
{
  // the parse name of the element
  const string DimensionElement::_sTypeName = "dimension";
  const vector<string> DimensionElement::_sValidAttributes = getValidAttributes();

  DimensionElement::DimensionElement()
  : NCMLElement()
  , _length("0")
  , _orgName("")
  , _isUnlimited("")
  , _isShared("")
  , _isVariableLength("")
  , _dim()
  {
  }

  DimensionElement::DimensionElement(const DimensionElement& proto)
  : NCMLElement(proto)
  {
    _length = proto._length;
    _orgName = proto._orgName;
    _isUnlimited = proto._isUnlimited;
    _isShared = proto._isShared;
    _isVariableLength = proto._isVariableLength;
    _dim = proto._dim;
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
    _dim.name = NCMLUtil::findAttrValue(attrs, "name");
    _length = NCMLUtil::findAttrValue(attrs, "length");
    _orgName = NCMLUtil::findAttrValue(attrs, "orgName");
    _isUnlimited = NCMLUtil::findAttrValue(attrs, "isUnlimited");;
    _isShared = NCMLUtil::findAttrValue(attrs, "isShared");;
    _isVariableLength = NCMLUtil::findAttrValue(attrs, "isVariableLength");

    // First check that we didn't get any typos...
    validateAttributes(attrs, _sValidAttributes);

    // Parse the size etc
    parseAndCacheDimension();

    // Final validation for things we implemented
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

    // This will be the scope we're to be added...
    NetcdfElement* dataset = p.getCurrentDataset();
    VALID_PTR(dataset);

    // Make sure the name is unique at this parse level or exception.
    const DimensionElement* pExistingDim = dataset->getDimensionInLocalScope(name());
    if (pExistingDim)
      {
        THROW_NCML_PARSE_ERROR("Tried at add dimension " + toString() +
            " but a dimension with name=" + name() +
            " already exists in this scope=" + p.getScopeString());
      }

    // The dataset will maintain a strong reference to us while we're needed.
    dataset->addDimension(this);
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
    ret += NCMLElement::printAttributeIfNotEmpty("name", name());
    ret += NCMLElement::printAttributeIfNotEmpty("length", _length);
    ret += NCMLElement::printAttributeIfNotEmpty("isShared", _isShared);
    ret += NCMLElement::printAttributeIfNotEmpty("isVariableLength", _isVariableLength);
    ret += NCMLElement::printAttributeIfNotEmpty("isUnlimited", _isUnlimited);
    ret += NCMLElement::printAttributeIfNotEmpty("orgName", _orgName);
    ret += " >";
    return ret;
  }

  bool
  DimensionElement::checkDimensionsMatch(const DimensionElement& rhs) const
  {
    return ( (this->name() == rhs.name()) &&
             (this->getSize() == rhs.getSize()) );
  }

  const string&
  DimensionElement::name() const
  {
    return _dim.name;
  }

  unsigned int
  DimensionElement::getLengthNumeric() const
  {
    return _dim.size;
  }

  unsigned int
  DimensionElement::getSize() const
  {
    return getLengthNumeric();
  }


  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////// PRIVATE IMPL

  void
  DimensionElement::parseAndCacheDimension()
  {
    stringstream sis;
    sis.str(_length);
    sis >> _dim.size;
    if (sis.fail())
      {
        THROW_NCML_PARSE_ERROR("Element " + toString() + " failed to parse the length attribute into a proper unsigned int!");
      }

    // @TODO set the _dim.isSizeConstant from the isVariableLength, etc once we know how to use them for aggs
    _dim.isSizeConstant = true;

    if (_isShared == "true")
      {
        _dim.isShared = true;
      }
    else if (_isShared == "false")
      {
        _dim.isShared = false;
      }
    else if (!_isShared.empty())
      {
        THROW_NCML_PARSE_ERROR("dimension@isShared did not have value in {true,false}.");
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

  vector<string>
  DimensionElement::getValidAttributes()
  {
    vector<string> validAttrs;
    validAttrs.reserve(10);
    validAttrs.push_back("name");
    validAttrs.push_back("length");
    validAttrs.push_back("isUnlimited");
    validAttrs.push_back("isVariableLength");
    validAttrs.push_back("isShared");
    validAttrs.push_back("orgName");
    return validAttrs;
  }
}

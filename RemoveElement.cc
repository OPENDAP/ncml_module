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
#include "RemoveElement.h"
#include "NCMLDebug.h"
#include "NCMLParser.h"
#include "NCMLUtil.h"

namespace ncml_module
{
  // The element name
  const string RemoveElement::_sTypeName = "remove";

  RemoveElement::RemoveElement()
  : _name("")
  , _type("")
  {
  }

  RemoveElement::RemoveElement(const RemoveElement& proto)
  : NCMLElement()
  {
    _name = proto._name;
    _type = proto._type;
  }

  RemoveElement::~RemoveElement()
  {
  }

  const string&
  RemoveElement::getTypeName() const
  {
    return _sTypeName;
  }

  RemoveElement*
  RemoveElement::clone() const
  {
    // We rely on the copy ctor here, so make sure it's valid
    RemoveElement* newElt = new RemoveElement(*this);
    return newElt;
  }

  void
  RemoveElement::setAttributes(const AttributeMap& attrs)
  {
    _name = NCMLUtil::findAttrValue(attrs, "name");
    _type = NCMLUtil::findAttrValue(attrs, "type");
  }

  void
  RemoveElement::handleBegin(NCMLParser& p)
  {
    processRemove(p);
  }

  void
  RemoveElement::handleContent(NCMLParser& /* p */, const string& content)
  {
    if (!NCMLUtil::isAllWhitespace(content))
      {
        THROW_NCML_PARSE_ERROR("Got non-whitespace for element content and didn't expect it.  Element=" + toString() + " content=\"" +
            content + "\"");
      }
  }

  void
  RemoveElement::handleEnd(NCMLParser&  /* p */)
  {
  }

  string
  RemoveElement::toString() const
  {
    return "<" + _sTypeName + " " + "name=\"" + _name + "\" type=\"" + _type + "\" >";
  }

  /////////////////////////////////////////////////////////////////////////////////////////////
  /// Non Public Implementation

  void
  RemoveElement::processRemove(NCMLParser& p)
  {
    if ( !(_type.empty() || _type == "attribute") )
      {
        THROW_NCML_PARSE_ERROR("Illegal type in remove element: type=" + _type +
            "  This version of the parser can only remove type=attribute");
      }

    AttrTable::Attr_iter it;
    bool gotIt = p.findAttribute(_name, it);
    if (!gotIt)
      {
        THROW_NCML_PARSE_ERROR("In remove element, could not find attribute to remove name=" + _name +
            " at the current scope=" + p.getScopeString());
      }

    // Nuke it.  This call works with containers too, and will recursively delete the children.
    BESDEBUG("ncml", "Removing attribute name=" << _name << " at scope=" << p.getScopeString() << endl);
    AttrTable* pTab = p.getCurrentAttrTable();
    VALID_PTR(pTab);
    pTab->del_attr(_name);
  }

} // ncml_module

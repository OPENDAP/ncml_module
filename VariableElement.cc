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
#include "VariableElement.h"

#include "BaseType.h"
#include "NCMLDebug.h"
#include "NCMLParser.h"
#include "NCMLUtil.h"

using namespace libdap;

namespace ncml_module
{

  const string VariableElement::_sTypeName = "variable";

  VariableElement::VariableElement()
  : _name("")
  , _type("")
  {
  }

  VariableElement::VariableElement(const VariableElement& proto)
  : NCMLElement()
  {
    _name = proto._name;
    _type = proto._type;
  }

  VariableElement::~VariableElement()
  {
  }

  const string&
  VariableElement::getTypeName() const
  {
    return _sTypeName;
  }

  VariableElement*
  VariableElement::clone() const
  {
    return new VariableElement(*this);
  }

  void
  VariableElement::setAttributes(const AttributeMap& attrs)
  {
    _name = NCMLUtil::findAttrValue(attrs, "name");
    _type = NCMLUtil::findAttrValue(attrs,"type");
  }

  void
  VariableElement::handleBegin(NCMLParser& p)
  {
    processBegin(p);
  }

  void
  VariableElement::handleContent(NCMLParser& /* p */, const string& content)
  {
    if (!NCMLUtil::isAllWhitespace(content))
      {
        THROW_NCML_PARSE_ERROR("Got non-whitespace for element content and didn't expect it.  Element=" + toString() + " content=\"" +
            content + "\"");
      }
  }

  void
  VariableElement::handleEnd(NCMLParser& p)
  {
    processEnd(p);
  }

  string
  VariableElement::toString() const
  {
    return "<" + _sTypeName + " name=\"" + _name + "\" type=\"" + _type + "\" >";
  }


  ////////////////// NON PUBLIC IMPLEMENTATION

  void
  VariableElement::processBegin(NCMLParser& p)
  {
    BESDEBUG("ncml", "VariableElement::handleBegin called for " << toString() << endl);

    if (!p.withinLocation())
      {
        THROW_NCML_PARSE_ERROR("Got element " + toString() + " while not in <netcdf> node!");
      }

    // Can only specify variable globally or within a composite variable now.
    if (!(p.isScopeGlobal() || p.isScopeCompositeVariable()))
      {
        THROW_NCML_PARSE_ERROR("Got <variable> element while not within a <netcdf> or within variable container.  scope=" +
            p.getScopeString());
      }

    p.processMetadataDirectiveIfNeeded();

    // Lookup the variable in whatever the parser's current variable container is
    // this could be the DDS top level or a constructor variable.
    BaseType* pVar = p.getVariableInCurrentVariableContainer(_name);
    if (!pVar)
      {
        THROW_NCML_PARSE_ERROR( "Could not find variable=" + _name + " in scope=" + p.getScopeString() +
            " and new variables cannot be added in this version.");
      }

    // Make sure the type matches.  NOTE:
    // We use "Structure" to mean Grid, Structure, Sequence!
    // Also type="" will match ANY type.
    // TODO This fails on Array as well due to NcML making arrays be a basic type with a non-zero dimension.
    // We're gonna ignore that until we allow addition of variables, but let's leave this check here for now
    if (!_type.empty() && !p.typeCheckDAPVariable(*pVar, p.convertNcmlTypeToCanonicalType(_type)))
      {
        THROW_NCML_PARSE_ERROR("Type Mismatch in variable element with name=" + _name +
            " at scope=" + p.getScopeString() +
            " Expected type=" + _type +
            " but found variable with type=" + pVar->type_name() +
            "  To match a variable of any type, please do not specify variable@type.");
      }

    // this sets the _pCurrentTable to the variable's table.
    p.setCurrentVariable(pVar);

    // Add the proper variable scope to the stack
    if (pVar->is_constructor_type())
      {
        p.enterScope(_name, ScopeStack::VARIABLE_CONSTRUCTOR);
      }
    else
      {
        p.enterScope(_name, ScopeStack::VARIABLE_ATOMIC);
      }
  }

  void
  VariableElement::processEnd(NCMLParser& p)
  {
    BESDEBUG("ncml", "VariableElement::handleEnd called at scope:" << p.getScopeString() << endl);
    if (!p.isScopeVariable())
      {
        THROW_NCML_PARSE_ERROR("VariableElement::handleEnd called when not parsing a variable element!  Scope=" + p.getTypedScopeString());
      }

    NCML_ASSERT_MSG(p.getCurrentVariable(), "Error: VariableElement::handleEnd(): Expected non-null parser.getCurrentVariable()!");

    // Set the new variable container to the parent of the current.
    // This could be NULL if we're a top level variable, making the DDS the variable container.
    p.setCurrentVariable(p.getCurrentVariable()->get_parent());
    p.exitScope();
    p.printScope();

    BaseType* pVar = p.getCurrentVariable();
    BESDEBUG("ncml", "Variable scope now with name: " << ((pVar)?(pVar->name()):("<NULL>")) << endl);
  }

}

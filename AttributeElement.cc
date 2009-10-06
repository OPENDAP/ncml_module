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
#include "AttributeElement.h"
#include "NCMLDebug.h"
#include "NCMLParser.h"
#include "NCMLUtil.h"
#include "OtherXMLParser.h"

namespace ncml_module
{
  const string AttributeElement::_sTypeName = "attribute";
  const vector<string> AttributeElement::_sValidAttributes = getValidAttributes();

  AttributeElement::AttributeElement()
  : NCMLElement(0)
  , _name("")
  , _type("")
  , _value("")
  , _separator(NCMLUtil::WHITESPACE)
  , _orgName("")
  , _tokens()
  , _pOtherXMLParser(0)
  {
    _tokens.reserve(256); // not sure what a good number is, but better than resizing all the time.
  }

  AttributeElement::AttributeElement(const AttributeElement& proto)
  : NCMLElement(proto)
  {
    _name = proto._name;
    _type = proto._type;
    _value = proto._value;
    _separator = proto._separator;
    _orgName = proto._orgName;
    _pOtherXMLParser = 0;
  }

  AttributeElement::~AttributeElement()
  {
    delete _pOtherXMLParser;
  }

  const string&
  AttributeElement::getTypeName() const
  {
    return _sTypeName;
  }

  AttributeElement*
  AttributeElement::clone() const
  {
    return new AttributeElement(*this);
  }

  void
  AttributeElement::setAttributes(const AttributeMap& attrs )
  {
    _name = NCMLUtil::findAttrValue(attrs, "name");
    _type = NCMLUtil::findAttrValue(attrs,"type");
    _value = NCMLUtil::findAttrValue(attrs, "value");
    _separator = NCMLUtil::findAttrValue(attrs, "separator", ""); // empty is unspecified...  whitespace if not string
    _orgName = NCMLUtil::findAttrValue(attrs, "orgName");

    validateAttributes(attrs, _sValidAttributes);
  }

  void
  AttributeElement::handleBegin()
  {
    processAttribute(*_parser);
  }

  void
  AttributeElement::handleContent(const string& content)
  {
    // We should know if it's valid here, but doublecheck with parser.
     if (_parser->isScopeAtomicAttribute())
       {
         BESDEBUG("ncml", "Adding attribute values as characters content for atomic attribute=" << _name <<
             " value=\"" << content << "\"" << endl);
         _value = content; // save the content unless we end the element, then we'll set it.
       }
     // Otherwise, it better be whitespace
     else if (!NCMLUtil::isAllWhitespace(content))
       {
         THROW_NCML_PARSE_ERROR("Got characters content for a non-atomic attribute!"
             " attribute@value is not allowed for attribute@type=Structure!");
       }
  }

  void
  AttributeElement::handleEnd()
  {
    processEndAttribute(*_parser);
  }

  string
  AttributeElement::toString() const
  {
    string ret = "<" + _sTypeName + " ";

    ret += "name=\"" + _name + "\"";

    if (!_type.empty())
      {
        ret += " type=\"" + _type + "\" ";
      }

    if (_separator != NCMLUtil::WHITESPACE)
      {
        ret += " separator=\"" + _separator + "\" ";
      }

    if (!_orgName.empty())
      {
        ret += " orgName=\"" + _orgName + "\" ";
      }

    if (!_value.empty())
      {
        ret += " value=\"" + _value + "\" ";
      }

    ret += ">";
    return ret;
  }


  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /// Non Public Implementation

  void
  AttributeElement::processAttribute(NCMLParser& p)
  {
    BESDEBUG("ncml", "handleBeginAttribute called for attribute name=" << _name << endl);

    // Make sure we're in a netcdf and then process the attribute at the current table scope,
    // which could be anywhere including glboal attributes, nested attributes, or some level down a variable tree.
    if (!p.withinNetcdf())
      {
        THROW_NCML_PARSE_ERROR("Got <attribute> element while not within a <netcdf> node!");
      }

    if (p.isScopeAtomicAttribute())
      {
        THROW_NCML_PARSE_ERROR("Got new <attribute> while in a leaf <attribute> at scope=" + p.getScopeString() +
            " Hierarchies of attributes are only allowed for attribute containers with type=Structure");
      }

    // Convert the NCML type to a canonical type here.
    // "Structure" will remain as "Structure" for specialized processing.
    string internalType = p.convertNcmlTypeToCanonicalType(_type);
    if (internalType.empty())
      {
        THROW_NCML_PARSE_ERROR("Unknown NCML type=" + _type + " for attribute name=" + _name + " at scope=" + p.getScopeString());
      }

    p.printScope();

    // First, if the type is a Structure, we are dealing with nested attributes and need to handle it separately.
    if (_type == NCMLParser::STRUCTURE_TYPE)
      {
        BESDEBUG("ncml", "Processing an attribute element with type Structure." << endl);
        processAttributeContainerAtCurrentScope(p);
      }
    else // It's atomic, so look it up in the current attr table and add a new one or mutate an existing one.
      {
        processAtomicAttributeAtCurrentScope(p);
      }
  }

  void
  AttributeElement::processAtomicAttributeAtCurrentScope(NCMLParser& p)
  {

    // If no orgName, just process with name.
     if (_orgName.empty())
       {
         if (p.attributeExistsAtCurrentScope(_name))
           {
             BESDEBUG("ncml", "Found existing attribute named: " << _name << " with type=" << _type << " at scope=" <<
                 p.getScopeString() << endl);
             // We set this when the element closes now!
             // mutateAttributeAtCurrentScope(p, _name, _type, _value);
           }
         else
           {
             BESDEBUG("ncml", "Didn't find attribute: " << _name << " so adding it with type=" << _type << " and value=" << _value << endl );
             addNewAttribute(p);
           }
       }

     else // if orgName then we want to rename an existing attribute, handle that separately
       {
         renameAtomicAttribute(p);
       }

     // If it's of type OtherXML, we need to set a proxy parser.
     if (_type == "OtherXML")
       {
         startOtherXMLParse(p);
       }

     // In all cases, also push the scope on the stack in case we get values as content.
     p.enterScope(_name, ScopeStack::ATTRIBUTE_ATOMIC);
  }

  void
  AttributeElement::processAttributeContainerAtCurrentScope(NCMLParser& p)
  {
    NCML_ASSERT_MSG(_type == NCMLParser::STRUCTURE_TYPE, "Logic error: processAttributeContainerAtCurrentScope called with non Structure type.");
    BESDEBUG("ncml", "Processing attribute container with name:" << _name << endl);

    // Technically it's an error to have a value for a container, so just check and warn.
    if (!_value.empty())
      {
        THROW_NCML_PARSE_ERROR("Found non empty() value attribute for attribute container at scope=" + p.getTypedScopeString());
      }

    // Make sure we're in a valid context.
    VALID_PTR(p.getCurrentAttrTable());

     AttrTable* pAT = 0;
     // If we're supposed to rename.
     if (!_orgName.empty())
       {
         pAT = renameAttributeContainer(p);
         VALID_PTR(pAT); // this should never be null.  We throw exceptions for parse errors.
       }
     else // Not renaming, either new one or just a scope specification.
       {
         AttrTable* pCurrentTable = p.getCurrentAttrTable();

         // See if the attribute container already exists in current scope.
         pAT = pCurrentTable->find_container(_name);
         if (!pAT) // doesn't already exist
           {
             // So create a new one
             pAT = pCurrentTable->append_container(_name);
             BESDEBUG("ncml", "Attribute container was not found, creating new one name=" << _name << " at scope=" << p.getScopeString() << endl);
           }
         else
           {
             BESDEBUG("ncml", "Found an attribute container name=" << _name << " at scope=" << p.getScopeString() << endl);
           }
       }

     // No matter how we get here, pAT is now the new scope, so push it under it's name
     VALID_PTR(pAT);
     p.setCurrentAttrTable(pAT);
     p.enterScope(pAT->get_name(), ScopeStack::ATTRIBUTE_CONTAINER);
  }

  string
  AttributeElement::getInternalType() const
  {
    return NCMLParser::convertNcmlTypeToCanonicalType(_type);
  }

  void
  AttributeElement::addNewAttribute(NCMLParser& p)
  {
    VALID_PTR(p.getCurrentAttrTable());

    string internalType = getInternalType();

    // OtherXML cannot be vector, only scalar, so enforce that.
    if (internalType != "OtherXML")
      {
        // Split the value string properly if the type is one that can be a vector.
        p.tokenizeAttrValues(_tokens, _value, internalType, _separator);
        p.getCurrentAttrTable()->append_attr(_name, internalType, &(_tokens));
      }
    else // if we are OtherXML
      {
        // At this point, we expect the value to be null.  It will show up in content...
        BESDEBUG("ncml", "Addinng new attribute of type OtherXML data." << endl);
        if (!_value.empty())
          {
            THROW_NCML_PARSE_ERROR("Adding new Attribute of type=OtherXML:  Cannot specify"
                " an attribute@value for OtherXML --- it must be set in the content!  Scope was: "
                + p.getScopeString() );
          }
        p.getCurrentAttrTable()->append_attr(_name, internalType, _value);
      }
  }

  void
  AttributeElement::mutateAttributeAtCurrentScope(NCMLParser& p, const string& name, const string& type, const string& value)
  {
    AttrTable* pTable = p.getCurrentAttrTable();
    VALID_PTR(pTable);
    NCML_ASSERT_MSG(p.attributeExistsAtCurrentScope(name),
        "Logic error. mutateAttributeAtCurrentScope called when attribute name=" + name + " didn't exist at scope=" + p.getTypedScopeString());

    // First, pull out the existing attribute's type if unspecified.
    string actualType = type;
    if (type.empty())
      {
        actualType = pTable->get_type(name);
      }

    // Make sure to turn it into internal DAP type for tokenize and storage
    actualType = p.convertNcmlTypeToCanonicalType(actualType);

    // Can't mutate, so just delete and reenter it.  This move change the ordering... Do we care?
    pTable->del_attr(name);

    // Split the values if needed, again avoiding OtherXML being tokenized since it's a scalar by definition.
    if (actualType == "OtherXML")
      {
        BESDEBUG("ncml", "Setting OtherXML data to: " << endl << _value << endl);
        pTable->append_attr(name, actualType, _value);
      }
    else
      {
        p.tokenizeAttrValues(_tokens, value, actualType, _separator);
        pTable->append_attr(name, actualType, &(_tokens));
      }
  }

  void
  AttributeElement::renameAtomicAttribute(NCMLParser& p)
  {
    AttrTable* pTable = p.getCurrentAttrTable();
    VALID_PTR(pTable);

    // Check for user errors
    if (!p.attributeExistsAtCurrentScope(_orgName))
      {
        THROW_NCML_PARSE_ERROR("Failed to change name of non-existent attribute with orgName=" + _orgName +
                               " and new name=" + _name + " at the current scope=" + p.getScopeString());
      }

    // If the name we're renaming to already exists, we'll assume that's an error as well, since the user probably
    // wants to know that.
    if (p.attributeExistsAtCurrentScope(_name))
      {
        THROW_NCML_PARSE_ERROR("Failed to change name of existing attribute orgName=" + _orgName +
                               " because an attribute with the new name=" + _name +
                               " already exists at the current scope=" + p.getScopeString());
      }

    AttrTable::Attr_iter it;
    bool gotIt = p.findAttribute(_orgName, it);
    NCML_ASSERT(gotIt);  // logic bug check, we check above

    // Just to be safe... we shouldn't get down here if it is, but we can't proceed otherwise.
    NCML_ASSERT_MSG( !pTable->is_container(it),
        "LOGIC ERROR: renameAtomicAttribute() got an attribute container where it expected an atomic attribute!");

    // Copy the entire vector explicitly here!
    vector<string>* pAttrVec = pTable->get_attr_vector(it);
    NCML_ASSERT_MSG(pAttrVec, "Unexpected NULL from get_attr_vector()");
    // Copy it!
    vector<string> orgData = *pAttrVec;
    AttrType orgType = pTable->get_attr_type(it);

    // Delete the old one
    pTable->del_attr(_orgName);

    // Hmm, what to do if the types are different?  I'd say use the new one....
    string typeToUse = AttrType_to_String(orgType);
    if (!_type.empty() && _type != typeToUse)
    {
       BESDEBUG("ncml", "Warning: renameAtomicAttribute().  New type did not match old type, using new type." << endl);
       typeToUse = _type;
    }

    // We'll record the type for the rename as well, for setting the data in the end element.
    _type = typeToUse;

    pTable->append_attr(_name, typeToUse, &orgData);

    // If value was specified, let's go call mutate on the thing we just made to change the data.  Seems
    // odd a user would do this, but it's allowable I think.
    if (!_value.empty())
      {
        mutateAttributeAtCurrentScope(p, _name, typeToUse, _value);
      }
  }

  AttrTable*
  AttributeElement::renameAttributeContainer(NCMLParser& p)
  {
    AttrTable* pTable = p.getCurrentAttrTable();
    VALID_PTR(pTable);
    AttrTable* pAT = pTable->find_container(_orgName);
    if (!pAT)
      {
        THROW_NCML_PARSE_ERROR("renameAttributeContainer: Failed to find attribute container with orgName=" + _orgName +
            " at scope=" + p.getScopeString());
      }

    if (p.attributeExistsAtCurrentScope(_name))
      {
        THROW_NCML_PARSE_ERROR("Renaming attribute container with orgName=" + _orgName +
            " to new name=" + _name +
            " failed since an attribute already exists with that name at scope=" + p.getScopeString());
      }

    BESDEBUG("ncml", "Renaming attribute container orgName=" << _orgName << " to name=" << _name << " at scope="
        << p.getTypedScopeString() << endl);

    // Just changing the name doesn't work because of how AttrTable stores names, so we need to remove and readd it under new name.
    AttrTable::Attr_iter it;
    bool gotIt = p.findAttribute(_orgName, it);
    NCML_ASSERT_MSG(gotIt, "Logic error.  renameAttributeContainer expected to find attribute but didn't.");

    // We now own pAT.
    pTable->del_attr_table(it);

    // Shove it back in with the new name.
    pAT->set_name(_name);
    pTable->append_container(pAT, _name);

    // Return it as the new current scope.
    return pAT;
  }

  void
  AttributeElement::processEndAttribute(NCMLParser& p)
  {

    BESDEBUG("ncml", "AttributeElement::handleEnd called at scope:" << p.getScopeString() << endl);

    if (p.isScopeAtomicAttribute())
      {
        // If it was an OtherXML, then set the _value from the proxy parser.
        if (_type == "OtherXML")
          {
            VALID_PTR(_pOtherXMLParser);
            _value = _pOtherXMLParser->getString();
            SAFE_DELETE(_pOtherXMLParser);
          }

        // Set the values that we have gotten if we're not a rename, or if we ARE a rename but have a new _value
        if (_orgName.empty() ||
            (!_orgName.empty() && !_value.empty()) )
          {
            mutateAttributeAtCurrentScope(*_parser, _name, _type, _value);
          }
        // And pop the attr table
        p.exitScope();
      }
    else if (p.isScopeAttributeContainer())
      {
        p.exitScope();
        p.setCurrentAttrTable(p.getCurrentAttrTable()->get_parent());
        // This better be valid or something is really broken!
        NCML_ASSERT_MSG(p.getCurrentAttrTable(), "ERROR: Null p.getCurrentAttrTable() unexpected while leaving scope of attribute container!");
      }
    else // Can't close an attribute if we're not in one!
      {
        THROW_NCML_PARSE_ERROR("Got end of attribute element while not parsing an attribute!");
      }
  }

  void
  AttributeElement::startOtherXMLParse(NCMLParser& p)
  {
    // this owns the memory.
    _pOtherXMLParser = new OtherXMLParser(p);
    p.enterOtherXMLParsingState(_pOtherXMLParser);
  }

  vector<string>
  AttributeElement::getValidAttributes()
  {
    vector<string> attrs;
    attrs.reserve(10);
    attrs.push_back("name");
    attrs.push_back("type");
    attrs.push_back("value");
    attrs.push_back("orgName");
    attrs.push_back("separator");
    return attrs;
  }


}




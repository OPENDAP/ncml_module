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
#include "NCMLParser.h"
#include <map>

#include "AttrTable.h"
#include "BaseType.h"
#include "BESConstraintFuncs.h"
#include "BESDDSResponse.h"
#include "BESDebug.h"
#include "cgi_util.h"
#include "DAS.h"
#include "DDS.h"
#include "DDSLoader.h"
#include "NcmlDebug.h"
#include "NcmlUtil.h"
#include "SaxParserWrapper.h"

// Turn on for more debug spew.
#define DEBUG_NCML_PARSER_INTERNALS 1

namespace ncml_module {

// Consider filling this with a compilation flag.
/* static */ bool NCMLParser::sThrowExceptionOnUnknownElements = true;

// An attribute or variable with type "Structure" will match this string.
const string NCMLParser::STRUCTURE_TYPE("Structure");

bool
NCMLParser::isScopeAtomicAttribute() const
{
  return (!_scope.empty()) && (_scope.topType() == ScopeStack::ATTRIBUTE_ATOMIC);
}

bool
NCMLParser::isScopeAttributeContainer() const
{
  return (!_scope.empty()) && (_scope.topType() == ScopeStack::ATTRIBUTE_CONTAINER);
}

bool
NCMLParser::isScopeSimpleVariable() const
{
  return (!_scope.empty()) && (_scope.topType() == ScopeStack::VARIABLE_ATOMIC);
}

bool
NCMLParser::isScopeCompositeVariable() const
{
  return (!_scope.empty()) && (_scope.topType() == ScopeStack::VARIABLE_CONSTRUCTOR);
}

bool
NCMLParser::isScopeGlobal() const
{
  return withinLocation() && _scope.empty();
}

DDS*
NCMLParser::getDDS() const
{
  NCML_ASSERT_MSG(_ddsResponse.get(), "getDDS() called when we're not processing a <netcdf> location and _ddsResponse is null!");
  return _ddsResponse->get_dds();
}

void
NCMLParser::resetParseState()
{
  _filename = "";
  _parsingLocation = false;
  _metadataDirective = PERFORM_DEFAULT;
  _pVar = 0;
  _pCurrentTable = 0;

  _scope.clear();

  // _ddsResponse takes care of itself.

  // just in case
  _loader.cleanup();
}

BaseType*
NCMLParser::getVariableInContainer(const string& varName, BaseType* pContainer)
{
  BaseType::btp_stack varContext;
  if (pContainer)
  {
    return pContainer->var(varName, varContext);
  }
  else
    {
      return getVariableInDDS(varName);
    }
}

// Not that this should take a fully qualified one too, but without a scoping operator (.) it will
// just search the top level variables.
BaseType*
NCMLParser::getVariableInDDS(const string& varName)
{
  BaseType::btp_stack varContext;
 return  getDDS()->var(varName, varContext);
}

void
NCMLParser::setCurrentVariable(BaseType* pVar)
{
   _pVar = pVar;
   if (pVar) // got a variable
     {
        _pCurrentTable = &(pVar->get_attr_table());
     }
   else if (getDDS()) // null pvar but we have a dds, use global table
     {
       DDS* dds = getDDS();
       _pCurrentTable = &(dds->get_attr_table());
     }
   else // just clear it out, no context
     {
       _pCurrentTable = 0;
     }
}

bool
NCMLParser::typeCheckDAPVariable(const BaseType& var, const string& expectedType)
{
  // Match all types.
  if (expectedType.empty())
    {
      return true;
    }
  else
    {
      // If the type specifies a Structure, it better be a Constructor type.
      if (expectedType == STRUCTURE_TYPE)
        {
          // Calls like is_constructor_type really should be const...
          BaseType& varSemanticConst = const_cast<BaseType&>(var);
          return varSemanticConst.is_constructor_type();
        }
      else
        {
          return (var.type_name() == expectedType);
        }
    }
}

void
NCMLParser::processAttributeAtCurrentScope(const string& name, const string& ncmlType, const string& value, const string& orgName)
{
  // Convert the NCML type to a canonical type here.
  // "Structure" will remain as "Structure" for specialized processing.
  string type = convertNcmlTypeToCanonicalType(ncmlType);
  if (type.empty())
    {
      THROW_NCML_PARSE_ERROR("Unknown NCML type=" + ncmlType + " for attribute name=" + name + " at scope=" + _scope.getScopeString());
    }

  printScope();

  // First, if the type is a Structure, we are dealing with nested attributes and need to handle it separately.
  if (type == STRUCTURE_TYPE)
    {
      BESDEBUG("ncml", "Processing an attribute element with type Structure." << endl);
      processAttributeContainerAtCurrentScope(name, type, value, orgName);
    }
  else // It's atomic, so look it up in the pTable and add a new one or mutate an existing one.
    {
      processAtomicAttributeAtCurrentScope(name, type, value, orgName);
    }
}

void
NCMLParser::processAtomicAttributeAtCurrentScope(const string& name, const string& type, const string& value, const string& orgName)
{
  // If no orgName, just process with name.
  if (orgName.empty())
    {
      if (attributeExistsAtCurrentScope(name))
        {
          BESDEBUG("ncml", "Found existing attribute named: " << name << " and changing to type=" << type << " and value=" << value << endl);
          mutateAttributeAtCurrentScope(name, type, value);
        }
      else
        {
          BESDEBUG("ncml", "Didn't find attribute: " << name << " so adding it with type=" << type << " and value=" << value << endl );
          addNewAttribute(name, type, value);
        }
    }

  else // if orgName then we want to rename an existing attribute, handle that separately
    {
      renameAtomicAttribute(name, type, value, orgName);
    }

  // In all cases, also push the scope on the stack in case we get values as content.
  enterScope(name, ScopeStack::ATTRIBUTE_ATOMIC);
}


void
NCMLParser::processAttributeContainerAtCurrentScope(const string& name, const string& type, const string& value, const string& orgName)
{
  NCML_ASSERT_MSG(type == STRUCTURE_TYPE, "Logic error: processAttributeContainerAtCurrentScope called with non Structure type.");
  BESDEBUG("ncml", "Processing attribute container with name:" << name << endl);

  // Technically it's an error to have a value for a container, so just check and warn.
  if (!value.empty())
    {
      THROW_NCML_PARSE_ERROR("Found non empty() value attribute for attribute container at scope=" + _scope.getTypedScopeString());
    }

  AttrTable* pAT = 0;
  // If we're supposed to rename.
  if (!orgName.empty())
    {
      pAT = renameAttributeContainer(name, orgName);
      VALID_PTR(pAT); // this should never be null.  We throw exceptions for parse errors.
    }
  else // Not renaming, either new one or just a scope specification.
    {

      // See if the attribute container already exists in pTable scope.
      pAT = _pCurrentTable->find_container(name);
      if (!pAT) // doesn't already exist
        {
          // So create a new one
          pAT = _pCurrentTable->append_container(name);
          BESDEBUG("ncml", "Attribute container was not found, creating new one name=" << name << " at scope=" << _scope.getScopeString() << endl);
        }
      else
        {
          BESDEBUG("ncml", "Found an attribute container name=" << name << " at scope=" << _scope.getScopeString() << endl);
        }
    }

  // No matter how we get here, pAT is now the new scope, so push it under it's name
  VALID_PTR(pAT);
  _pCurrentTable = pAT;
  enterScope(pAT->get_name(), ScopeStack::ATTRIBUTE_CONTAINER);
}

AttrTable*
NCMLParser::getGlobalAttrTable()
{
  return &(getDDS()->get_attr_table());
}

bool
NCMLParser::attributeExistsAtCurrentScope(const string& name)
{
  // Lookup the given attribute in the current table.
  AttrTable::Attr_iter attr;
  bool foundIt = findAttribute(name, attr);
  return foundIt;
}

bool
NCMLParser::findAttribute(const string& name, AttrTable::Attr_iter& attr) const
{
  AttrTable* pContainer = 0;

  if (_pCurrentTable)
    {
      _pCurrentTable->find(name, &pContainer, &attr);
      return (pContainer != 0);
    }
  else
    {
      return false;
    }
 }

void
NCMLParser::addNewAttribute(const string& name, const string& type, const string& value)
{
  VALID_PTR(_pCurrentTable);

  // Split the value string properly if the type is one that can be a vector.
  tokenizeValues(value, type);

  _pCurrentTable->append_attr(name, type, &(_tokens));
}

void
NCMLParser::mutateAttributeAtCurrentScope(const string& name, const string& type, const string& value)
{
  VALID_PTR(_pCurrentTable);
  NCML_ASSERT_MSG(attributeExistsAtCurrentScope(name),
      "Logic error. mutateAttributeAtCurrentScope called when attribute name=" + name + " didn't exist at scope=" + _scope.getTypedScopeString());

  // First, pull out the existing attribute's type if unspecified.
  string actualType = type;
  if (type.empty())
    {
      actualType = _pCurrentTable->get_type(name);
    }

  // Split the values if needed.  Also uses current value of _separators in case an atomic attribute set it.
  tokenizeValues(value, actualType);

  // Can't mutate, so just delete and reenter it.  This move change the ordering... Do we care?
  _pCurrentTable->del_attr(name);
  _pCurrentTable->append_attr(name, actualType, &(_tokens));
}

void
NCMLParser::renameAtomicAttribute(const string& newName, const string& newType, const string& newValue, const string& orgName)
{
  VALID_PTR(_pCurrentTable);

  // Check for user errors
  if (!attributeExistsAtCurrentScope(orgName))
    {
      THROW_NCML_PARSE_ERROR("Failed to change name of non-existent attribute with orgName=" + orgName +
                             " and new name=" + newName + " at the current scope=" + _scope.getScopeString());
    }

  // If the name we're renaming to already exists, we'll assume that's an error as well, since the user probably
  // wants to know that.
  if (attributeExistsAtCurrentScope(newName))
    {
      THROW_NCML_PARSE_ERROR("Failed to change name of existing attribute orgName=" + orgName +
                             " because an attribute with the new name=" + newName +
                             " already exists at the current scope=" + _scope.getScopeString());
    }

  // OK, if it's there, let's rename it...  We'll need to:
  // 1) Pull out the existing data from orgName
  // 2) Delete the old entry for orgName
  // 3) Add the new entry with the saved data under the new name
  // 4) * (consider this) If value is !empty(), then mutate the entry or error?
  AttrTable::Attr_iter it;
  bool gotIt = findAttribute(orgName, it);
  NCML_ASSERT(gotIt);  // logic bug check, we check above

  // Just to be safe... we shouldn't get down here if it is, but we can't proceed otherwise.
  NCML_ASSERT_MSG( !_pCurrentTable->is_container(it),
      "LOGIC ERROR: renameAtomicAttribute() got an attribute container where it expected an atomic attribute!");

  // 1) Copy the entire vector explicitly here!
  vector<string>* pAttrVec = _pCurrentTable->get_attr_vector(it);
  NCML_ASSERT_MSG(pAttrVec, "Unexpected NULL from get_attr_vector()");
  // Copy it!
  vector<string> orgData = *pAttrVec;
  AttrType orgType = _pCurrentTable->get_attr_type(it);

  // 2) Out with the old
  _pCurrentTable->del_attr(orgName);

  // Hmm, what to do if the types are different?  I'd say use the new one....
  string typeToUse = AttrType_to_String(orgType);
  if (!newType.empty() && newType != typeToUse)
  {
     BESDEBUG("ncml", "Warning: renameAtomicAttribute().  New type did not match old type, using new type." << endl);
     typeToUse = newType;
  }

  // 3) In with the new entry for the old data.
  _pCurrentTable->append_attr(newName, typeToUse, &orgData);


  // 4) If value was specified, let's go call mutate on the thing we just made to change the data.  Seems
  // odd a user would do this, but it's allowable I think.
  if (!newValue.empty())
    {
      mutateAttributeAtCurrentScope(newName, newType, newValue);
    }
}

AttrTable*
NCMLParser::renameAttributeContainer(const string& newName, const string& orgName)
{
  VALID_PTR(_pCurrentTable);
  AttrTable* pAT = _pCurrentTable->find_container(orgName);
  if (!pAT)
    {
      THROW_NCML_PARSE_ERROR("renameAttributeContainer: Failed to find attribute container with orgName=" + orgName +
          " at scope=" + _scope.getScopeString());
    }

    // Check that the new name doesn't exist either.
    if (attributeExistsAtCurrentScope(newName))
      {
        THROW_NCML_PARSE_ERROR("Renaming attribute container with orgName=" + orgName +
            " to new name=" + newName +
            " failed since an attribute already exists with that name at scope=" + _scope.getScopeString());
      }

    // Otherwise, we're good, let the curious know.
    BESDEBUG("ncml", "Renaming attribute container orgName=" << orgName << " to name=" << newName << " at scope="
          << _scope.getTypedScopeString() << endl);

    // Just changing the name doesn't work because of how AttrTable stores names, so we need to remove and readdit under new na,e.
    AttrTable::Attr_iter it;
    bool gotIt = findAttribute(orgName, it);
    NCML_ASSERT_MSG(gotIt, "Logic error.  renameAttributeContainer expected to find attribute but didn't.");
    // Erase the table at the iterator.  This leaves the memory alone, so pAT still valid.
    // BUG TODO This call actually leaks the struct entry memory.  Submitted a ticket.
    // Until it's fixed, we also leak.
    // IMPORTANT: make sure this doesn't delete the actual table!
    // We want it to remove it but hand the memory to me, or else we need to copy it!
    _pCurrentTable->del_attr_table(it);

    // Shove it back in with the new name
    pAT->set_name(newName);
    _pCurrentTable->append_container(pAT, newName);

    // Return it as the new current scope.
    return pAT;
}

int
NCMLParser::tokenizeValues(const string& values, const string& dapTypeName)
{
  // Convert the type string into a DAP AttrType to be sure
  AttrType dapType = String_to_AttrType(dapTypeName);
  if (dapType == Attr_unknown)
    {
      THROW_NCML_PARSE_ERROR("Attempting to tokenize attribute value failed since we found an unknown internal DAP type=" + dapTypeName +
           " for the current fully qualified attribute=" + _scope.getScopeString());
    }

  // If we're valid type, tokenize us according to type.
  int numTokens = tokenizeValuesForDAPType(values, dapType);

  // TODO Do we want to validate the data we are pulling out of the NcML?  I.e. check that it is
  // Of the type specified and split properly?  For example, we'd check each entry could be atof for floats, etc.

#if DEBUG_NCML_PARSER_INTERNALS

  if (_separators != NcmlUtil::WHITESPACE)
    {
       BESDEBUG("ncml", "Got non-default separators for tokenize.  _separators=\"" << _separators << "\"" << endl);
    }

  string msg = "";
  for (int i=0; i<numTokens; i++)
    {
      if (i > 0)
        {
          msg += ",";
        }
      msg += "\"";
      msg += _tokens[i];
      msg += "\"";
    }
  BESDEBUG("ncml", "Tokenize got " << numTokens << " tokens:\n" << msg << endl);

#endif // DEBUG_NCML_PARSER_INTERNALS

  return numTokens;
}

int
NCMLParser::tokenizeValuesForDAPType(const string& values, AttrType dapType)
{
  _tokens.resize(0);  // Start empty.

  // For URL and String, just push it onto the end, one token.
  if (dapType ==  Attr_string || dapType == Attr_url)
    {
       _tokens.push_back(values);
       return 1;
    }
  else if (dapType == Attr_unknown)
    {
      // Do out best to recover....
      BESDEBUG("ncml", "Warning: tokenizeValuesForDAPType() got unknown DAP type!  Attempting to continue..." << endl);
      _tokens.push_back(values);
      return 1;
    }
  else if (dapType == Attr_container)
    {
      // Not supposed to have values, just push empty string....
      BESDEBUG("ncml", "Warning: tokenizeValuesForDAPType() got container type, we should not have values!" << endl);
      _tokens.push_back("");
      return 1;
    }
  else // For all other atomic types, do a split.
    {
      return NcmlUtil::tokenize(values, _tokens, _separators);
    }
}

////////////////////////////////////// Class Methods (Statics)

// Used below to convert NcML data type to a DAP data type.
// TODO consider making this case-insensitive...
typedef std::map<string, string> TypeConverter;

/* Ncml DataType:
  <xsd:enumeration value="char"/>
  <xsd:enumeration value="byte"/>
  <xsd:enumeration value="short"/>
  <xsd:enumeration value="int"/>
  <xsd:enumeration value="long"/>
  <xsd:enumeration value="float"/>
  <xsd:enumeration value="double"/>
  <xsd:enumeration value="String"/>
  <xsd:enumeration value="string"/>
  <xsd:enumeration value="Structure"/>
 */
static TypeConverter* makeTypeConverter()
{
  TypeConverter* ptc = new TypeConverter();
  TypeConverter& tc = *ptc;
  // NcML to DAP conversions
  tc["char"] = "Byte";
  tc["byte"] = "Byte";
  tc["short"] = "Int16";
  tc["int"] = "Int32";
  tc["long"] = "Int32"; // not sure of this one
  tc["float"] = "Float32";
  tc["double"] = "Float64";
  tc["string"] = "String"; // allow lower case.
  tc["String"] = "String";
  tc["Structure"] = "Structure";
  tc["structure"] = "Structure"; // allow lower case for this as well
  return ptc;
}

// Singleton
static const TypeConverter& getTypeConverter()
{
  static TypeConverter* singleton = 0;
  if (!singleton)
    {
      singleton = makeTypeConverter();
    }
  return *singleton;
}

// Is the given type a DAP type?
static bool isDAPType(const string& type)
{
  return (String_to_AttrType(type) != Attr_unknown);
}

// Whether we want to pass through DAP attribute types as well as NcML.
static const bool ALLOW_DAP_ATTRIBUTE_TYPES = true;

/* static */
string
NCMLParser::convertNcmlTypeToCanonicalType(const string& ncmlType)
{
  NCML_ASSERT_MSG(!ncmlType.empty(), "Logic error: convertNcmlTypeToCanonicalType disallows empty() input.");

  // if we allow DAP types to be specified as an attribute type
  // then just pass it through
  if (ALLOW_DAP_ATTRIBUTE_TYPES && isDAPType(ncmlType))
    {
       return ncmlType;
    }

  const TypeConverter& tc = getTypeConverter();
  TypeConverter::const_iterator it = tc.find(ncmlType);
  if (it == tc.end())
    {
      return ""; // error condition
    }
  else
    {
      return it->second;
    }
}

void
NCMLParser::changeMetadataDirective(SourceMetadataDirective newVal)
{
  // Only go back to default if we're not processing a <netcdf> node.
  if (!_parsingLocation && newVal != PERFORM_DEFAULT)
    {
      THROW_NCML_PARSE_ERROR("<readMetadata/> or <explicit/> element found outside <netcdf> tree.");
    }

  // If it's already been set by the file (ie not default) can't change it (unless to PROCESSED).
  if (_parsingLocation && _metadataDirective != PERFORM_DEFAULT && newVal != PROCESSED)
    {
      THROW_NCML_PARSE_ERROR("NcML file must contain one of <readMetadata/> or <explicit/>");
    }

  // Also an error to unprocess during a location parse if we already explicitly set it.
  if (_parsingLocation && _metadataDirective != PERFORM_DEFAULT && newVal != PROCESSED)
    {
      THROW_NCML_INTERNAL_ERROR("Logic error: can't unprocess a metadata directive if we already processed it.");
    }

  // Otherwise, we can set it.
  _metadataDirective = newVal;
}

void
NCMLParser::processMetadataDirectiveIfNeeded()
{
  // only do it once.
  if (_metadataDirective == PROCESSED)
    {
      return;
    }

  // If explicit, clear the DDS metadata
  if (_metadataDirective == EXPLICIT)
    {
        BESDEBUG("ncml", "<explicit/> was used, so clearing all source metadata!" << endl);
        clearAllAttrTables();
    }

  // In all cases, only do this once per location.
  changeMetadataDirective(PROCESSED);
}

void
NCMLParser::clearAllAttrTables()
{
  DDS* dds = getDDS();
  if (!dds)
  {
      return;
  }

  // Blow away the global attribute table.
  dds->get_attr_table().erase();

  // Hit all variables, recursing on containers.
  for (DDS::Vars_iter it = dds->var_begin(); it != dds->var_end(); ++it)
    {
      // this will clear not only *it's table, but it's children if it's composite.
        clearVariableMetadataRecursively(*it);
    }
}

void
NCMLParser::clearVariableMetadataRecursively(BaseType* var)
{
  VALID_PTR(var);
  // clear the table
  var->get_attr_table().erase();

  if (var->is_constructor_type())
    {
        Constructor *compositeVar = dynamic_cast<Constructor*>(var);
        if (!compositeVar)
          {
           THROW_NCML_INTERNAL_ERROR("clearVariableMetadataRecursively: Unexpected cast error on dynamic_cast<Constructor*>");
          }
        for (Constructor::Vars_iter it = compositeVar->var_begin(); it != compositeVar->var_end(); ++it)
          {
             clearVariableMetadataRecursively(*it);
          }
    }
}


void
NCMLParser::enterScope(const string& name, ScopeStack::ScopeType type)
{
  _scope.push(name, type);
  BESDEBUG("ncml", "Entering scope: " << _scope.top().getTypedName() << endl);
  BESDEBUG("ncml", "New scope=\"" << _scope.getScopeString() << "\"" << endl);
}

void
NCMLParser::exitScope()
{
  NCML_ASSERT_MSG(!_scope.empty(), "Logic Error: Scope Stack Underflow!");
  BESDEBUG("ncml", "Exiting scope " << _scope.top().getTypedName() << endl);
  _scope.pop();
  BESDEBUG("ncml", "New scope=\"" << _scope.getScopeString() << "\"" << endl);
}

void
NCMLParser::printScope() const
{
  BESDEBUG("ncml", "Scope=\"" << _scope.getScopeString() << "\"" << endl);
}

void
NCMLParser::cleanup()
{
  // The only memory we own is the _ddsResponse, which is in an auto_ptr so will
  // either be returned to caller in parse() and cleared, or else
  // delete'd by our dtor via auto_ptr

  // All other objects point into _ddsResponse temporarily, so nothing to destroy there.

  // Just for completeness.
  resetParseState();
}

////////////////////////////////////////
////// Public

NCMLParser::NCMLParser(DDSLoader& loader)
: _filename("")
, _parsingLocation(false)
, _loader(loader)
, _ddsResponse(0)
, _metadataDirective(PERFORM_DEFAULT)
, _pVar(0)
, _pCurrentTable(0)
, _scope()
, _tokens()
, _separators(NcmlUtil::WHITESPACE)
{
  BESDEBUG("ncml", "Created NCMLHelper." << endl);
 _tokens.reserve(256); // not sure what a good number is, but better than resizing all the time.
}

NCMLParser::~NCMLParser()
{
  cleanup();
  // if _ddsResponse isn't relinquished, auto_ptr will nuke it here.
}

auto_ptr<BESDDSResponse>
NCMLParser::parse(const string& filename)
{
  if (parsing())
    {
      THROW_NCML_INTERNAL_ERROR("Illegal Operation: NCMLParser::parse called while already parsing!");
    }

  BESDEBUG("ncml", "Beginning NcML parse of file=" << filename << endl);

  // In case we care.
  _filename = filename;

  // Make a SAX parser wrapper to set up the C callbacks.
  // It will call us back through SaxParser interface.
  // The parse call can throw exceptions, but
  // we use RAII pattern to clean up if needed.
  SaxParserWrapper parser(*this);
  parser.parse(filename);

  // Prepare for a new parse, making sure it's all cleaned up (with the exception of the _ddsResponse
  // which where's about to send off)
  resetParseState();

  // Relinquish ownership to the caller.
  return _ddsResponse;
}

void
NCMLParser::handleBeginLocation(const string& location)
{
  BESDEBUG("ncml", "handleBeginLocation on:" << location << endl);
  if (_parsingLocation)
    {
      THROW_NCML_PARSE_ERROR("Got a new <netcdf> location while already parsing one!");
    }
  _parsingLocation = true;

  // If we get another one of these while we still have one, it's currently an error.
  // We can only process on location right now.
  if (_ddsResponse.get())
    {
      // Dtor will clean it up
     THROW_NCML_PARSE_ERROR("Got another <netcdf> node while already processing one!  We only support one <netcdf> per file in this version.");
    }

  // Use the loader to load the location specified in the <netcdf> element.
  // If not found, this call will throw an exception and we'll just unwind out.
  // TODO We probably want to see what the error was and throw the correct reponse upward or something.
  _ddsResponse = _loader.load(location); // gain control of response object

  // Force the attribute table to be the global one for the DDS.
  if (getDDS())
    {
      _pCurrentTable = getGlobalAttrTable();
      VALID_PTR(_pCurrentTable); // it really has to be there.
    }
}

void
NCMLParser::handleEndLocation()
{
  BESDEBUG("ncml", "handleEndLocation called!" << endl);

  if (!withinLocation())
    {
      THROW_NCML_PARSE_ERROR("Got close of <netcdf> node while not within one!");
    }

  // If the only entry was the metadata directive, we'd better make sure it gets done!
  processMetadataDirectiveIfNeeded();

  // For now, this will be the end of our parse, until aggregation.
  _parsingLocation = false;
  // We leave the rest of the state alone for the main parse to clean up and return the DDX response correctly.
}

void
NCMLParser::handleReadMetadata()
{
  if (!withinLocation())
    {
      THROW_NCML_PARSE_ERROR("Got <readMetadata/> while not within <netcdf>");
    }
  changeMetadataDirective(READ_METADATA);
}

void
NCMLParser::handleExplicit()
{
  if (!withinLocation())
    {
      THROW_NCML_PARSE_ERROR("Got <explicit/> while not within <netcdf>");
    }
  changeMetadataDirective(EXPLICIT);;
}

void
NCMLParser::handleBeginVariable(const string& varName, const string& type)
{
  BESDEBUG("ncml", "handleBeginVariable called with varName=" << varName << endl);

  if (!withinLocation())
    {
      THROW_NCML_PARSE_ERROR("Got <variable> element while not in <netcdf> node!");
    }

  // Can only specify variable globally or within a composite variable now.
  if (!(isScopeGlobal() || isScopeCompositeVariable()))
    {
      THROW_NCML_PARSE_ERROR("Got <variable> element while not within a <netcdf> or within variable container.  scope=" +
          _scope.getScopeString());
    }

  // Handle the metadata directive if this is the first call.
  processMetadataDirectiveIfNeeded();

  // Lookup the variable at this level.  If !_pVar, we assume we look on the DDS.
  // Otherwise, assume pVar is a container and look within it.
  BaseType* pVar = getVariableInContainer(varName, _pVar);
  if (!pVar)
    {
      THROW_NCML_PARSE_ERROR( "Could not find variable=" + varName + " in scope=" + _scope.getScopeString() +
                              " and new variables cannot be added in this version.");
    }

  // Make sure the type matches.  NOTE:
  // We use "Structure" to mean Grid, Structure, Sequence!
  // Also type="" will match ANY type.
  // TODO This fails on Array as well due to NcML referring making arrays be a basic type with a non-zero dimension.
  // We're gonna ignore that until we allow addition of variables, but let's leave this check here for now
  if (!type.empty() && !typeCheckDAPVariable(*pVar, convertNcmlTypeToCanonicalType(type)))
    {
      THROW_NCML_PARSE_ERROR("Type Mismatch in variable element with name=" + varName +
          " at scope=" + _scope.getScopeString() +
          " Expected type=" + type +
          " but found variable with type=" + pVar->type_name() +
          "  To match a variable of any type, please do not specify variable@type.");
    }

  // this sets the _pCurrentTable to the variable's table.
  setCurrentVariable(pVar);

  // Add the proper variable scope to the stack
  if (pVar->is_constructor_type())
    {
      enterScope(varName, ScopeStack::VARIABLE_CONSTRUCTOR);
    }
  else
    {
      enterScope(varName, ScopeStack::VARIABLE_ATOMIC);
    }
}

void
NCMLParser::handleEndVariable()
{
  // pop the attr table back upwards to the previous one
  // I think we can just use the parent of the current...
  BESDEBUG("ncml", "handleEndVariable called at scope:" << _scope.getScopeString() << endl);
  if (!isScopeVariable())
    {
      THROW_NCML_PARSE_ERROR("handleEndVariable called when not parsing a variable element!");
    }

  NCML_ASSERT_MSG(_pVar, "Error: Unexpected NULL _pVar in handleEndVariable!");

  // New context is the parent, which could be NULL for top level DDS vars.
  setCurrentVariable(_pVar->get_parent());
  exitScope(); // pop the stack
  printScope();

  BESDEBUG("ncml", "Variable scope now with name: " << ((_pVar)?(_pVar->name()):("<NULL>")) << endl);
}

void
NCMLParser::handleBeginAttribute(const string& name, const string& type, const string& value, const string& separators, const string& orgName)
{
  BESDEBUG("ncml", "handleBeginAttribute called for attribute name=" << name << endl);

  // Make sure we're in a netcdf and then process the attribute at the current table scope,
  // which could be anywhere including glboal attributes, nested attributes, or some level down a variable tree.
  if (!withinLocation())
    {
      THROW_NCML_PARSE_ERROR("Got <attribute> element while not within a <netcdf> node!");
    }

  // Can't be within a leaf attribute for this call either!
  if (isScopeAtomicAttribute())
    {
      THROW_NCML_PARSE_ERROR("Got new <attribute> while in a leaf <attribute> at scope=" + _scope.getScopeString() +
          " Hierarchies of attributes are only allowed for attribute containers with type=Structure");
    }

  // Handle metadata directive if this is the first entry.
  processMetadataDirectiveIfNeeded();

  // If this was specified, then we want to store it for the parsing of this attribute only.
  // It will be set back to default in the next handleEndAttribute() call.
  if (!separators.empty())
    {
      _separators = separators;

#if DEBUG_NCML_PARSER_INTERNALS
      BESDEBUG("ncml", "Got attribute@separator=\"" << separators << "\"  and using for the next attribute value." << endl);
#endif // #if DEBUG_NCML_PARSER_INTERNALS
    }

  processAttributeAtCurrentScope(name, type, value, orgName);
}

void
NCMLParser::handleEndAttribute()
{
  BESDEBUG("ncml", "handleEndAttribute called at scope:" << _scope.getScopeString() << endl);

  // if it wasn't a container, just clear out this state
  if (isScopeAtomicAttribute())
    {
      // Nothing to do but pop it off scope stack, we're still within the containing AttrTable.
      exitScope();
    }
  else if (isScopeAttributeContainer()) // we are parsing a container
    {
      exitScope();

      // Walk up the to the container's parent as we leave this scope.
      _pCurrentTable = _pCurrentTable->get_parent();
      // This better be valid or something is really broken!
      NCML_ASSERT_MSG(_pCurrentTable, "ERROR: Null _pCurrentTable unexpected while leaving scope of attribute container!");
    }
  else // Can't close an attribute if we're not in one!
    {
      THROW_NCML_PARSE_ERROR("Got end of attribute element while not parsing an attribute!");
    }
}


void
NCMLParser::onStartDocument()
{
  BESDEBUG("ncml", "onStartDocument." << endl);
}

void
NCMLParser::onEndDocument()
{
  BESDEBUG("ncml", "onEndDocument." << endl);
}

void
NCMLParser::onStartElement(const std::string& name, const AttrMap& attrs)
{
  // TODO Lots of magic string constants in here...  They're not likely to change,
  // But it might be better to pull them into classes for each node
  // which has the named attributes it supports.  Easier to check for
  // misspelled attribute that way, etc.

  // Dispatch to the right place, else ignore
 if (name == "netcdf")
   {
     handleBeginLocation(SaxParser::findAttrValue(attrs, "location"));
   }
 else if (name == "explicit")
   {
     handleExplicit();
   }
 else if (name == "readMetadata")
   {
     handleReadMetadata();
   }
 else if (name == "attribute")
   {
     const string& attrName = SaxParser::findAttrValue(attrs, "name");
     const string& type = SaxParser::findAttrValue(attrs,"type");
     const string& value = SaxParser::findAttrValue(attrs, "value");
     const string& separators = SaxParser::findAttrValue(attrs, "separator");
     const string& orgName = SaxParser::findAttrValue(attrs, "orgName");

     //Process the attribute depending on what it is.
     // If orgName is !empty(), we'll rename it.
     handleBeginAttribute(attrName, type, value, separators, orgName);
   }
 else if (name == "variable")
   {
     const string& varName = SaxParser::findAttrValue(attrs, "name");
     const string& type = SaxParser::findAttrValue(attrs,"type");
     handleBeginVariable(varName, type);
   }
 else // Unknown element...  Ugh, we might want to parse error here to let the caller know we don't support the element?  I'll defer it for now.
   {
     if (sThrowExceptionOnUnknownElements)
       {
         THROW_NCML_PARSE_ERROR("Unknown element type=" + name + " found in NcML parse with scope=" + _scope.getScopeString());
       }
     else
       {
         BESDEBUG("ncml", "Start of <" << name << "> element.  Element unsupported, ignoring." << endl);
       }
   }
}

void
NCMLParser::onEndElement(const std::string& name)
{
  // BESDEBUG("ncml", "onEndElement got: " << name << endl);
  // Dispatch to the right place, else ignore
  if (name == "netcdf")
    {
      handleEndLocation();
    }
  else if (name == "attribute")
    {
      handleEndAttribute();
      // go back to our default as we leave an attribute that may have changed this!
      _separators = NcmlUtil::WHITESPACE;
    }
  else if (name == "variable")
    {
      handleEndVariable();
    }
  else
    {
      BESDEBUG("ncml", "End of <" << name << "> element unsupported currently, ignoring." << endl);
    }
}

static bool isAllWhitespace(const string& str)
{
  return (str.find_first_not_of(" \t\n") == string::npos);
}

void
NCMLParser::onCharacters(const std::string& content)
{
  // Getting non-whitespace characters is valid in an leaf element to specify the value.
  if (isScopeAtomicAttribute())
    {
      const string& name = _scope.topName();
      BESDEBUG("ncml", "Adding attribute values as characters content for atomic attribute=" << name << " value=\"" << content << "\"" << endl);
      mutateAttributeAtCurrentScope(name, "", content); // empty type means leave the same.
    }
  else // other elements don't have mixed values now, so this better be filler space!
    {
      // If this isn't just random whitespace then bad ncml.
      if (!isAllWhitespace(content))
        {
          THROW_NCML_PARSE_ERROR("Got unexpected non-whitespace characters at scope=" +
              _scope.getTypedScopeString() +
              " with characters=\"" + content + "\"");
        }
    }
}

void
NCMLParser::onParseWarning(std::string msg)
{
  // TODO  We may want to make a flag for considering warnings errors as well.
  BESDEBUG("ncml", "PARSE WARNING: LibXML msg={" << msg << "}.  Attempting to continue parse." << endl);
}

// Pretty much have to give up on malformed XML.
void
NCMLParser::onParseError(std::string msg)
{
  THROW_NCML_PARSE_ERROR("libxml SAX2 parser error! msg={" + msg + "} Terminating parse!");
}

} // namespace ncml_module




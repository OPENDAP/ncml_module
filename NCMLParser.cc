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
#include "NcmlUtil.h"
#include "SaxParserWrapper.h"
// #include "util.h" // for downcase()

// Turn on for more debug spew.
#define DEBUG_NCML_PARSER_INTERNALS 1

using namespace ncml_module;

// An attribute or variable with type "Structure" will match this string.
const string NCMLParser::STRUCTURE_TYPE("Structure");

bool NCMLParser::isScopeSimpleAttribute() const
{
  return _scope.topType() == ScopeStack::ATTRIBUTE_ATOMIC;
}

bool
NCMLParser::isScopeAttributeContainer() const
{
  return _scope.topType() == ScopeStack::ATTRIBUTE_CONTAINER;
}

bool
NCMLParser::isScopeSimpleVariable() const
{
  return _scope.topType() == ScopeStack::VARIABLE_ATOMIC;
}

bool
NCMLParser::isScopeCompositeVariable() const
{
  return _scope.topType() == ScopeStack::VARIABLE_CONSTRUCTOR;
}

bool
NCMLParser::isScopeGlobal() const
{
  return _scope.empty();
}

DDS*
NCMLParser::getDDS() const
{
  assert(_ddsResponse); // find logic errors in debug build.
  if (!_ddsResponse)
    {
      BESDEBUG("ncml", "Error: getDDS called when we're not processing a <netcdf> location and _ddsResponse is null!" << endl);
      throw BESInternalError("Null pointer", __FILE__, __LINE__);
    }
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

 // while (!_attrContainerStack.empty())
  //  {
  //    _attrContainerStack.pop();
  //  }

  _scope.clear();

  // if non-null, we still own it.
  delete _ddsResponse;
  _ddsResponse = 0;

  // just in case
  _loader.cleanup();
}

BaseType*
NCMLParser::getVariableInContainer(const string& varName, BaseType* pContainer)
{
  BaseType::btp_stack varContext;
  if (pContainer)
  {
    // TODO consider checking the context.  I think this actually recurses, so we want to make sure
    // we get the var at current level, not recursed.  Probably want to do the iteration ourselves at this level.
    return pContainer->var(varName, varContext);
  }
  else
    {
      return getVariableInDDS(varName);
    }
}

// Not that this should take a fully qualified one too, but without a scoping oeprator (.) it will
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

// TODO we'll need a version of this that takes a vector<string>* as well for handling arrays.
void
NCMLParser::processAttributeAtCurrentScope(const string& name, const string& ncmlType, const string& value)
{
  // Convert the NCML type to a DAP type here.
  // "Structure" will remain as "Structure" for specialized processing.
  string type = convertNcmlTypeToDapType(ncmlType);
  if (type.empty())
    {
      string msg = "Unknown NCML type: " + ncmlType;
      BESDEBUG("ncml", msg << endl);
      throw BESInternalError(msg, __FILE__, __LINE__);
    }

  printScope();

  // First, if the type is a Structure, we are dealing with nested attributes and need to handle it separately.
  if (type == STRUCTURE_TYPE)
    {
      BESDEBUG("ncml", "Processing an attribute element with type Structure." << endl);
      processAttributeContainerAtCurrentScope(name, type, value);
    }
  else // It's atomic, so look it up in the pTable and add a new one or mutate an existing one.
    {
      if (attributeExistsAtCurrentScope(name))
        {
          BESDEBUG("ncml", "Found existing attribute named: " << name << " and changing to type=" << type << " and value=" << value << endl);
          mutateAttributeAtCurrentScope(name, type, value);
        }
      else
        {
          BESDEBUG("ncml", "Didn't find attribute: " << name << " so adding it with type=" << type << " and value=" << value << endl );
          addNewAttribute(_pCurrentTable, name, type, value);
        }

      // Also push the scope on the stack in case we get values as content.
      enterScope(name, ScopeStack::ATTRIBUTE_ATOMIC);
    }
}


void
NCMLParser::processAttributeContainerAtCurrentScope(const string& name, const string& type, const string& value)
{
  assert(type == STRUCTURE_TYPE);
  BESDEBUG("ncml", "Processing attribute container with name:" << name << endl);

  // Technically it's an error to have a value for a container, so just check and warn.
  if (!value.empty())
    {
      BESDEBUG("ncml", "WARNING: processAttributeContainerForTable got a non-empty value for an attribute container.  Ignoring it.");
    }

  // See if the attribute container already exists in pTable scope.
  AttrTable* pAT = _pCurrentTable->find_container(name);
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

  // Either way, pAT is now the new scope, so push it
  _pCurrentTable = pAT;
  enterScope(name, ScopeStack::ATTRIBUTE_CONTAINER);
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
NCMLParser::addNewAttribute(AttrTable* pTable, const string& name, const string& type, const string& value)
{
  // Split the value string properly if the type is one that can be a vector.
  tokenizeValues(value, type);

  pTable->append_attr(name, type, &(_tokens));
}

void
NCMLParser::mutateAttributeAtCurrentScope(const string& name, const string& type, const string& value)
{
  assert(attributeExistsAtCurrentScope(name)); // logic error to call this otherwise.

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

int
NCMLParser::tokenizeValues(const string& values, const string& dapTypeName)
{
  // Convert the type string into a DAP AttrType to be sure
  AttrType dapType = String_to_AttrType(dapTypeName);
  if (dapType == Attr_unknown)
    {
      string msg = "Unknown DAP type found for: " + dapTypeName;
      BESDEBUG("ncml", msg << endl);
      throw BESInternalError(msg, __FILE__, __LINE__);
    }

  // If we're valid type, tokenize us according to type.
  int numTokens = tokenizeValuesForDAPType(values, dapType);

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
  tc["string"] = "String";
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
NCMLParser::convertNcmlTypeToDapType(const string& ncmlType)
{
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
      throw BESInternalError("<readMetadata/> or <explicit/> element found outside <netcdf> tree.", __FILE__, __LINE__);
    }

  // If it's already been set by the file (ie not default) can't change it (unless to PROCESSED).
  if (_parsingLocation && _metadataDirective != PERFORM_DEFAULT && newVal != PROCESSED)
    {
      throw BESInternalError("NcML file must contain one of <readMetadata/> or <explicit/>", __FILE__, __LINE__);
    }

  // Also an error to unprocess during a location parse if we already explicitly set it.
  if (_parsingLocation && _metadataDirective != PERFORM_DEFAULT && newVal != PROCESSED)
    {
      throw BESInternalError("Logic error.", __FILE__, __LINE__);
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
  assert(dds);
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
  // clear the table
  var->get_attr_table().erase();

  if (var->is_constructor_type())
    {
        Constructor *compositeVar = dynamic_cast<Constructor*>(var);
        if (!compositeVar)
          {
            throw BESInternalError("Cast error", __FILE__, __LINE__);
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
  // we don't own any of the memory, unless _ddsResponse is non-null at the point
  // this object is destructed (ie we have not finished a successful parse)
  // On successfull parse, we hand this to caller and relinquish ownership (null it).
  delete _ddsResponse;
  _ddsResponse = 0;
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
}

BESDDSResponse*
NCMLParser::parse(const string& filename)
{
  if (parsing())
    {
      string msg = "Error: NCMLParser::parse called while already parsing!";
      BESDEBUG("ncml", msg << endl);
      throw BESInternalError(msg, __FILE__, __LINE__);
    }

  BESDEBUG("ncml", "Beginning NcML parse of file=" << filename << endl);

  // In case we care.
  _filename = filename;

  // Make a SAX parser wrapper to set up the C callbacks.
  // It will call us back through SaxParser interface.
  SaxParserWrapper parser(*this);
  parser.parse(filename);

  // Relinquish ownership to the caller.
  BESDDSResponse* ret = _ddsResponse;
  _ddsResponse = 0;

  // Prepare for a new parse, making sure it's all cleaned up.
  resetParseState();

  return ret;
}

void
NCMLParser::handleBeginLocation(const string& location)
{
  BESDEBUG("ncml", "handleBeginLocation on:" << location << endl);
  if (_parsingLocation)
    {
      string msg = "Parse Error: Got a new <netcdf> location while already parsing one!";
       BESDEBUG("ncml", msg << endl);
       throw BESInternalError(msg, __FILE__, __LINE__);
    }
  _parsingLocation = true;

  // If we get another one of these while we still have one, it's currently an error.
  // We can only process on location right now.
  if (_ddsResponse)
    {
      cleanup(); // get rid of the storage, then exception
      string msg = "Parse Error: Got another <netcdf> node while already processing one!";
      BESDEBUG("ncml", msg << endl);
      throw BESInternalError(msg, __FILE__, __LINE__);
    }

  // Use the loader to load the location specified in the <netcdf> element.
  // If not found, we'll throw an exception and just unwind out.
  _ddsResponse = _loader.load(location);

  // Force the attribute table to be the global one for the DDS.
  if (getDDS())
    {
      _pCurrentTable = getGlobalAttrTable();
    }
}

void
NCMLParser::handleEndLocation()
{
  BESDEBUG("ncml", "handleEndLocation called!" << endl);

  // For now, this will be the end of our parse, until aggregation.
  _parsingLocation = false;
  // We leave the rest of the state alone for the main parse to clean up and return correctly.
}

void
NCMLParser::handleReadMetadata()
{
   changeMetadataDirective(READ_METADATA);
}

void
NCMLParser::handleExplicit()
{
  changeMetadataDirective(EXPLICIT);;
}

// TODO this will have to take the other attributes, such as type.  We want to double check
// that type is "Structure" for nested variable calls, probably.
// TODO Probably want to typecheck type if it is not null.
void
NCMLParser::handleBeginVariable(const string& varName, const string& type /* = "" */)
{
  BESDEBUG("ncml", "handleBeginVariable called with varName=" << varName << endl);

  processMetadataDirectiveIfNeeded();

  // Lookup the variable at this level.  If !_pVar, we assume we look on the DDS.
  // Otherwise, assume pVar is a container and look within it.
  // TODO Note that if pVar, pVar had better be a container variable!
  BaseType* pVar = getVariableInContainer(varName, _pVar);
  if (!pVar)
    {
      const string msg = "PARSE ERROR: handleBeginVariable could not find variable=" + varName + " in scope=" + _scope.getScopeString() +
        " and new variables cannot be added in this version.";
      BESDEBUG("ncml", msg << endl);
      throw BESInternalError(msg, __FILE__, __LINE__);
    }

  // TODO Check the type matches if they specified it...

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
  if (!withinVariable())
    {
      throw BESInternalError("handleEndVariable called when not parsing a variable element", __FILE__, __LINE__);
    }

  // New context is the parent, which could be NULL for top level DDS vars.
  // We might want to use an explicit local stack to help debugging, TODO
  setCurrentVariable(_pVar->get_parent());
  exitScope(); // pop the stack
  printScope();

  BESDEBUG("ncml", "Variable scope now with name: " << ((_pVar)?(_pVar->name()):("<NULL>")) << endl);
}

void
NCMLParser::handleBeginAttribute(const string& name, const string& type, const string& value)
{
  // TODO might want to print our current container, be it global or a variable...
  BESDEBUG("ncml", "handleBeginAttribute called for attribute name=" << name << endl);

  // Make sure we're in a netcdf and then process the attribute at the current table scope,
  // which could be anywhere including glboal attributes, nested attributes, or some level down a variable tree.
  if (!withinLocation())
    {
      BESDEBUG("ncml", "Parse error: Got handleBeginAttribute when not within a <netcdf> node!");
      throw BESInternalError("Parse error.", __FILE__, __LINE__);
    }

  processMetadataDirectiveIfNeeded();

  // If we are processing a simple attribute, we should not get a new elemnent before closing the old!  Parse error!
  if (isScopeSimpleAttribute())
    {
      const string msg = "Parse error: handleBeginAttribute called before a an atomic attribute was closed!";
      BESDEBUG("ncml", msg << " Scope=" << _scope.getTypedScopeString() << endl);
      throw BESInternalError(msg, __FILE__, __LINE__);
    }

  processAttributeAtCurrentScope(name, type, value);
}

void
NCMLParser::handleEndAttribute()
{
  BESDEBUG("ncml", "handleEndAttribute called at scope:" << _scope.getScopeString() << endl);

  // if it wasn't a container, just clear out this state
  if (isScopeSimpleAttribute())
    {
      // Nothing to do but pop it off scope stack, we're still within the containing AttrTable.
      exitScope();
    }
  else if (isScopeAttributeContainer()) // we are parsing a container
    {
      exitScope();

      // Walk up the to the container's parent as we leave this scope.
      _pCurrentTable = _pCurrentTable->get_parent();
      assert(_pCurrentTable); // This better be valid or something is really broken!
      if (!_pCurrentTable)
        {
          const string msg = "ERROR: Null _pCurrentTable unexpected.";
          BESDEBUG("ncml", msg << endl);
          throw BESInternalError(msg, __FILE__, __LINE__);
        }
    }
  else
    {
      const string msg = "PARSE ERROR: got handleEndAttribute when not in attribute scope.  Scope=" + _scope.getTypedScopeString();
      BESDEBUG("ncml",  msg << endl);
      throw BESInternalError(msg, __FILE__, __LINE__);
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
  // Dispatch to the right place, else ignore
 if (name == "netcdf")
   {
     handleBeginLocation(SaxParser::findAttrValue(attrs, "location"));
   }
 else if (name == "explicit")
   {
     handleExplicit();
   }
 else if (name == "readMetaData")
   {
     handleReadMetadata();
   }
 else if (name == "attribute")
   {
     const string& attrName = SaxParser::findAttrValue(attrs, "name");
     const string& type = SaxParser::findAttrValue(attrs,"type");
     const string& value = SaxParser::findAttrValue(attrs, "value");
     const string& separators = SaxParser::findAttrValue(attrs, "separator");
     // If this was specified, then we want to store it for the parsing of this attrribute only
     if (!separators.empty())
       {
#if DEBUG_NCML_PARSER_INTERNALS
         BESDEBUG("ncml", "Got attribute@separator=\"" << separators << "\"  and using for the next attribute value." << endl);
#endif // #if DEBUG_NCML_PARSER_INTERNALS
         _separators = separators;
       }
     handleBeginAttribute(attrName, type, value);
   }
 else if (name == "variable")
   {
     const string& varName = SaxParser::findAttrValue(attrs, "name");
     const string& type = SaxParser::findAttrValue(attrs,"type");
     handleBeginVariable(varName, type);
   }
 else
   {
     BESDEBUG("ncml", "Start of <" << name << "> element unsupported currently, ignoring." << endl);
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
      // go back to our default as we leave an attribute.
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
  if (isScopeSimpleAttribute())
    {
      const string& name = _scope.topName();
      BESDEBUG("ncml", "Adding attribute calues as characters content for atomic attribute=" << name << " value=\"" << content << "\"" << endl);
      mutateAttributeAtCurrentScope(name, "", content); // empty type means leave the same.
    }
  else // other elements don't have mixed values now!
    {
      // If this isn't just random whitespace then bad ncml.
      if (!isAllWhitespace(content))
        {
            const string msg = "PARSE ERROR: Got non-whitespace characters in unexpected element at scope=\"" +
              _scope.getTypedScopeString() +
              "\" with characters=\"" + content + "\"";
            BESDEBUG("ncml", msg << endl);
            throw BESInternalError(msg, __FILE__, __LINE__);
        }
    }
}

void
NCMLParser::onParseWarning(std::string msg)
{
  BESDEBUG("ncml", "PARSE WARNING: LibXML msg={" << msg << "}.  Attempting to continue parse." << endl);
}

// Pretty much have to give up on malformed XML.
void
NCMLParser::onParseError(std::string msg)
{
  string bigMsg = "PARSE ERROR: LibXML msg={" + msg + "} Terminating parse!";
  BESDEBUG("ncml", bigMsg << endl);
  throw BESInternalError(bigMsg, __FILE__, __LINE__);
}




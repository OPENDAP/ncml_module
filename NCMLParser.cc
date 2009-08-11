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

#include "AttrTable.h"
#include "BaseType.h"
#include "BESConstraintFuncs.h"
#include "BESDataDDSResponse.h"
#include "BESDDSResponse.h"
#include "BESDebug.h"
#include "cgi_util.h"
#include "DAS.h"
#include "DDS.h"
#include "DDSLoader.h"
#include "DimensionElement.h"
#include <map>
#include <memory>
#include "NCMLCommonTypes.h"
#include "NCMLDebug.h"
#include "NCMLElement.h"
#include "NCMLUtil.h"
#include "NetcdfElement.h"
#include "parser.h" // for the type checking...
#include "SaxParserWrapper.h"
#include <sstream>
#include "Structure.h"

// For extra debug spew for now.
#define DEBUG_NCML_PARSER_INTERNALS 1

namespace ncml_module {

  // From the DAP 2 guide....
  static const unsigned int MAX_DAP_STRING_SIZE = 32767;

// Consider filling this with a compilation flag.
/* static */ bool NCMLParser::sThrowExceptionOnUnknownElements = true;

// An attribute or variable with type "Structure" will match this string.
const string NCMLParser::STRUCTURE_TYPE("Structure");


/////////////////////////////////////////////////////////////////////////////////////////////
////// Public

NCMLParser::NCMLParser(DDSLoader& loader)
: _filename("")
, _parsingNetcdf(false)
, _loader(loader)
, _responseType(DDSLoader::eRT_RequestDDX)
, _response(0)
, _metadataDirective(PERFORM_DEFAULT)
, _pVar(0)
, _pCurrentTable(0)
, _elementStack()
, _scope()
, _dimensions()
{
  BESDEBUG("ncml", "Created NCMLParser." << endl);
}

NCMLParser::~NCMLParser()
{
  // clean other stuff up
  cleanup();
}

auto_ptr<BESDapResponse>
NCMLParser::parse(const string& ncmlFilename,  DDSLoader::ResponseType responseType)
{
  // Parse into a newly created object.
  auto_ptr<BESDapResponse> response = DDSLoader::makeResponseForType(responseType);

  // Parse into the response.  We still got it in the auto_ptr in this scope, so we're safe
  // on exception since the auto_ptr in this func will cleanup the memory.
  parseInto(ncmlFilename, responseType, response.get());

  // Relinquish it to the caller
  return response;
}

void
NCMLParser::parseInto(const string& ncmlFilename, DDSLoader::ResponseType responseType, BESDapResponse* response)
{
  VALID_PTR(response);
  NCML_ASSERT_MSG(DDSLoader::checkResponseIsValidType(responseType, response),
        "NCMLParser::parseInto: got wrong response object for given type.");

  _responseType = responseType;
  _response = response;

  if (parsing())
    {
      THROW_NCML_INTERNAL_ERROR("Illegal Operation: NCMLParser::parse called while already parsing!");
    }

  BESDEBUG("ncml", "Beginning NcML parse of file=" << ncmlFilename << endl);

  // In case we care.
  _filename = ncmlFilename;

  // Invoke the libxml sax parser
  SaxParserWrapper parser(*this);
  parser.parse(ncmlFilename);

  // Prepare for a new parse, making sure it's all cleaned up (with the exception of the _ddsResponse
  // which where's about to send off)
  resetParseState();

  // we're done with it.
  _response = 0;
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
NCMLParser::onStartElement(const std::string& name, const AttributeMap& attrs)
{
  // the auto_ptr will clean up memory on any exception in handleBegin().
  // if we succeed, we release and store the element in a stack which the dtor must clean up.
  std::auto_ptr<NCMLElement> elt = NCMLElement::Factory::getTheFactory().makeElement(name, attrs);

  // If we found an element of the given type name
  if (elt.get())
    {
      elt->handleBegin(*this);
      // Must release the auto_ptr for storing in a container.
      pushElement(elt.release());
    }
 else // Unknown element...
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
  NCMLElement* elt = getCurrentElement();
  VALID_PTR(elt);

  // If it matches the one on the top of the stack, then process and pop.
  if (elt->getTypeName() == name)
    {
      elt->handleEnd(*this);
      popElement(); // handles delete
      elt = 0;
    }
  else // the names don't match, so just ignore it.
    {
      BESDEBUG("ncml", "End of <" << name << "> element unsupported currently, ignoring." << endl);
    }
}

void
NCMLParser::onCharacters(const std::string& content)
{
  // If we got an element on the stack, hand it off.  Otherwise, do nothing.
  NCMLElement* elt = getCurrentElement();
  if (elt)
    {
      elt->handleContent(*this, content);
    }
}

void
NCMLParser::onParseWarning(std::string msg)
{
  // TODO  We may want to make a flag for considering warnings errors as well.
  BESDEBUG("ncml", "PARSE WARNING: LibXML msg={" << msg << "}.  Attempting to continue parse." << endl);
}

void
NCMLParser::onParseError(std::string msg)
{
  // Pretty much have to give up on malformed XML.
  THROW_NCML_PARSE_ERROR("libxml SAX2 parser error! msg={" + msg + "} Terminating parse!");
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Non-public Implemenation

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
  return withinNetcdf() && _scope.empty();
}

bool
NCMLParser::isScopeNetcdf() const
{
  // see if the last thing parsed was <netcdf>
  return (!_elementStack.empty() && dynamic_cast<NetcdfElement*>(_elementStack.back()));
}

DDS*
NCMLParser::getDDS() const
{
  NCML_ASSERT_MSG(_response, "getDDS() called when we're not processing a <netcdf> location and _response is null!");
  return NCMLUtil::getDDSFromEitherResponse(_response);
}

bool
NCMLParser::parsingDataRequest() const
{
  const BESDataDDSResponse* const pDataDDSResponse = dynamic_cast<const BESDataDDSResponse* const>(_response);
  return (pDataDDSResponse);
}

void
NCMLParser::resetParseState()
{
  _filename = "";
  _parsingNetcdf = false;
  _metadataDirective = PERFORM_DEFAULT;
  _pVar = 0;
  _pCurrentTable = 0;

  _scope.clear();

  // Cleanup any memory in the _elementStack
  deleteElementStack();

  // Not that this matters...
  _responseType = DDSLoader::eRT_RequestDDX;

  // We never own the memory in this, so just clear it.
  _response = 0;

  // just in case
  _loader.cleanup();

  deleteDimensions();
}

void
NCMLParser::loadLocation(const std::string& location)
{
  // We better have one!  This gets created up front now.  It will be an empty DDS
  VALID_PTR(_response);

  // Use the loader to load the location
  // If not found, this call will throw an exception and we'll just unwind out.
   _loader.loadInto(location, _responseType, _response);
}

BaseType*
NCMLParser::getVariableInCurrentVariableContainer(const string& name)
{
  return getVariableInContainer(name, _pVar);
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
NCMLParser::addCopyOfVariableAtCurrentScope(BaseType& varTemplate)
{
  // make sure we can do it...
  BaseType* pExisting = getVariableInCurrentVariableContainer(varTemplate.name());
  if (pExisting)
    {
      // at this point, it's an internal error.  the caller should have checked.
      THROW_NCML_INTERNAL_ERROR("NCMLParser::addNewVariableAtCurrentScope:"
          " Cannot add variable since one with the same name exists at current scope."
          " Name= " + varTemplate.name());
    }

  // Also an internal error if the caller tries it.
  if (!(isScopeCompositeVariable() || isScopeGlobal()))
    {
      THROW_NCML_INTERNAL_ERROR("NCMLParser::addNewVariableAtCurrentScope: current scope not valid for adding variable.  Scope=" +
          getTypedScopeString());
    }

  // OK, we know we can add it now.  But to what?
  if (_pVar) // Constructor variable
    {
      NCML_ASSERT_MSG(_pVar->is_constructor_type(), "Expected _pVar is a container type!");
      _pVar->add_var(&varTemplate);
    }
  else // Top level DDS
    {
      BESDEBUG("ncml", "Adding new variable to DDS top level.  Variable name=" << varTemplate.name() <<
          " and typename=" << varTemplate.type_name() << endl);
      DDS* pDDS = getDDS();
      pDDS->add_var(&varTemplate);
    }
}

void
NCMLParser::deleteVariableAtCurrentScope(const string& name)
{
  if (! (isScopeCompositeVariable() || isScopeGlobal()) )
    {
      THROW_NCML_INTERNAL_ERROR("NCMLParser::deleteVariableAtCurrentScope called when we do not have a variable container at current scope!");
    }

  if (_pVar) // In container?
    {
      // Given interfaces, unfortunately it needs to be a Structure or we can't do this operation.
      Structure* pVarContainer = dynamic_cast<Structure*>(_pVar);
      if (!pVarContainer)
        {
          THROW_NCML_PARSE_ERROR( "NCMLParser::deleteVariableAtCurrentScope called with _pVar not a "
                                  "Structure class variable!  "
                                  "We can only delete variables from top DDS or within a Structure now.  scope=" +
                                  getTypedScopeString());
        }
      // First, make sure it exists so we can warn if not.  The call fails silently.
      BaseType* pToBeNuked = pVarContainer->var(name);
      if (!pToBeNuked)
        {
          THROW_NCML_PARSE_ERROR( "Tried to remove variable from a Structure, but couldn't find the variable with name=" + name +
                                  "at scope=" + getScopeString());
        }
      // Silently fails, so assume it worked.
      pVarContainer->del_var(name);
    }
  else // Global
    {
      // we better have a DDS if we get here!
      DDS* pDDS = getDDS();
      VALID_PTR(pDDS);
      pDDS->del_var(name);
    }
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

AttrTable*
NCMLParser::getGlobalAttrTable()
{
  return &(getDDS()->get_attr_table());
}

void
NCMLParser::setCurrentAttrTable(AttrTable* table)
{
  _pCurrentTable = table;
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

int
NCMLParser::tokenizeAttrValues(vector<string>& tokens, const string& values, const string& dapAttrTypeName, const string& separator)
{
  // Convert the type string into a DAP AttrType to be sure
  AttrType dapType = String_to_AttrType(dapAttrTypeName);
  if (dapType == Attr_unknown)
    {
      THROW_NCML_PARSE_ERROR("Attempting to tokenize attribute value failed since we found an unknown internal DAP type=" + dapAttrTypeName +
           " for the current fully qualified attribute=" + _scope.getScopeString());
    }

  // If we're valid type, tokenize us according to type.
  int numTokens = tokenizeValuesForDAPType(tokens, values, dapType, separator);

  // Now type check the tokens are valid strings for the type.
  NCMLParser::checkDataIsValidForCanonicalTypeOrThrow(dapAttrTypeName, tokens);

#if DEBUG_NCML_PARSER_INTERNALS

  if (separator != NCMLUtil::WHITESPACE)
    {
       BESDEBUG("ncml", "Got non-default separators for tokenize.  separator=\"" << separator << "\"" << endl);
    }

  string msg = "";
  for (int i=0; i<numTokens; i++)
    {
      if (i > 0)
        {
          msg += ",";
        }
      msg += "\"";
      msg += tokens[i];
      msg += "\"";
    }
  BESDEBUG("ncml", "Tokenize got " << numTokens << " tokens:\n" << msg << endl);

#endif // DEBUG_NCML_PARSER_INTERNALS

  return numTokens;
}

int
NCMLParser::tokenizeValuesForDAPType(vector<string>& tokens, const string& values, AttrType dapType, const string& separator)
{
  tokens.resize(0);  // Start empty.

  // For URL and String, just push it onto the end, one token.
  if (dapType ==  Attr_string || dapType == Attr_url)
    {
       tokens.push_back(values);
       return 1;
    }
  else if (dapType == Attr_unknown)
    {
      // Do out best to recover....
      BESDEBUG("ncml", "Warning: tokenizeValuesForDAPType() got unknown DAP type!  Attempting to continue..." << endl);
      tokens.push_back(values);
      return 1;
    }
  else if (dapType == Attr_container)
    {
      // Not supposed to have values, just push empty string....
      BESDEBUG("ncml", "Warning: tokenizeValuesForDAPType() got container type, we should not have values!" << endl);
      tokens.push_back("");
      return 1;
    }
  else // For all other atomic types, do a split.
    {
      return NCMLUtil::tokenize(values, tokens, separator);
    }
}


////////////////////////////////////// Class Methods (Statics)

// Used below to convert NcML data type to a DAP data type.
typedef std::map<string, string> TypeConverter;

// If true, we allow the specification of a DAP scalar type
// in a location expecting an NcML type.
static const bool ALLOW_DAP_TYPES_AS_NCML_TYPES = true;

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
  tc["char"] = "Byte"; // char is a C char, let's use a Byte and special parse it as a char not numeric
  tc["byte"] = "Int16"; // Since NcML byte's can be signed, we must promote them to not lose the sign bit.
  tc["short"] = "Int16";
  tc["int"] = "Int32";
  tc["long"] = "Int32"; // not sure of this one
  tc["float"] = "Float32";
  tc["double"] = "Float64";
  tc["string"] = "String"; // allow lower case.
  tc["String"] = "String";
  tc["Structure"] = "Structure";
  tc["structure"] = "Structure"; // allow lower case for this as well

  // If we allow DAP types to be specified directly,
  // then make them be passthroughs in the converter...
  if (ALLOW_DAP_TYPES_AS_NCML_TYPES)
    {
      tc["Byte"] = "Byte"; // DAP Byte can fit in Byte tho, unlike NcML "byte"!
      tc["Int16"] = "Int16";
      tc["UInt16"] = "UInt16";
      tc["Int32"] = "Int32";
      tc["UInt32"] = "UInt32";
      tc["Float32"] = "Float32";
      tc["Float64"] = "Float64";
      // allow both url cases due to old bug where "Url" is returned in dds rather then DAP2 spec "URL"
      tc["Url"] = "URL";
      tc["URL"] = "URL";
    }

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


/* static */
string
NCMLParser::convertNcmlTypeToCanonicalType(const string& ncmlType)
{
  NCML_ASSERT_MSG(!ncmlType.empty(), "Logic error: convertNcmlTypeToCanonicalType disallows empty() input.");

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
NCMLParser::checkDataIsValidForCanonicalTypeOrThrow(const string& type, const vector<string>& tokens) const
{
/*  Byte
  Int16
  UInt16
  Int32
  UInt32
  Float32
  Float64
  String
  URL
*/
  bool valid = true;
  vector<string>::const_iterator it;
  vector<string>::const_iterator endIt = tokens.end();
  for (it = tokens.begin(); it != endIt; ++it)
    {
      if (type == "Byte")
        {
          valid &= check_byte(it->c_str());
        }
      else if (type == "Int16")
        {
          valid &= check_int16(it->c_str());
        }
      else if (type == "UInt16")
        {
          valid &= check_uint16(it->c_str());
        }
      else if (type == "Int32")
        {
          valid &= check_int32(it->c_str());
        }
      else if (type == "UInt32")
        {
          valid &= check_uint32(it->c_str());
        }
      else if (type == "Float32")
        {
          valid &= check_float32(it->c_str());
        }
      else if (type == "Float64")
        {
          valid &= check_float64(it->c_str());
        }
      // Doh!  The DAP2 specifies case as "URL" but internally libdap uses "Url"  Allow both...
      else if (type == "URL" || type == "Url" || type == "String")
        {
          // TODO the DAP call check_url is currently a noop.  do we want to check for well-formed URL?
          // This isn't an NcML type now, so straight up NcML users might enter URL as String anyway.
          valid &= (it->size() <= MAX_DAP_STRING_SIZE);
          if (!valid)
            {
              std::stringstream msg;
              msg << "Invalid Value: The "<< type << " attribute value (not shown) exceeded max string length of " << MAX_DAP_STRING_SIZE <<
              " at scope=" << _scope.getScopeString() << endl;
              THROW_NCML_PARSE_ERROR(msg.str());
            }

          valid &= NCMLUtil::isAscii(*it);
          if (!valid)
            {
              THROW_NCML_PARSE_ERROR("Invalid Value: The " + type + " attribute value (not shown) has an invalid non-ascii character.");
            }
        }

      else
        {
          // We probably shouldn't get here, but...
          THROW_NCML_INTERNAL_ERROR("checkDataIsValidForCanonicalType() got unknown data type=" + type);
        }

      // Early throw so we know which token it was.
      if (!valid)
        {
          THROW_NCML_PARSE_ERROR("Invalid Value given for type=" + type + " with value=" + (*it) + " was invalidly formed or out of range" +
               _scope.getScopeString());
        }
    }
  // All is good if we get here.
}

void
NCMLParser::changeMetadataDirective(SourceMetadataDirective newVal)
{
  // Only go back to default if we're not processing a <netcdf> node.
  if (!_parsingNetcdf && newVal != PERFORM_DEFAULT)
    {
      THROW_NCML_PARSE_ERROR("<readMetadata/> or <explicit/> element found outside <netcdf> tree.");
    }

  // If it's already been set by the file (ie not default) can't change it (unless to PROCESSED).
  if (_parsingNetcdf && _metadataDirective != PERFORM_DEFAULT && newVal != PROCESSED)
    {
      THROW_NCML_PARSE_ERROR("NcML file must contain one of <readMetadata/> or <explicit/>");
    }

  // Also an error to unprocess during a location parse if we already explicitly set it.
  if (_parsingNetcdf && _metadataDirective != PERFORM_DEFAULT && newVal != PROCESSED)
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

string
NCMLParser::getScopeString() const
{
  return _scope.getScopeString();
}

string
NCMLParser::getTypedScopeString() const
{
  return _scope.getTypedScopeString();
}

void
NCMLParser::pushElement(NCMLElement* elt)
{
  VALID_PTR(elt);
  _elementStack.push_back(elt);
}

void
NCMLParser::popElement()
{
  NCMLElement* elt = _elementStack.back();
  _elementStack.pop_back();
  delete elt;
}

NCMLElement*
NCMLParser::getCurrentElement() const
{
  if (_elementStack.empty())
    {
      return 0;
    }
  else
    {
      return _elementStack.back();
    }
}

void
NCMLParser::deleteElementStack()
{
  while (!_elementStack.empty())
    {
      NCMLElement* elt = _elementStack.back();
      _elementStack.pop_back();
      delete elt;
    }
}

const DimensionElement*
NCMLParser::getDimension(const std::string& name) const
{
  const DimensionElement* ret = 0;
  vector<DimensionElement*>::const_iterator endIt = _dimensions.end();
  for (vector<DimensionElement*>::const_iterator it = _dimensions.begin(); it != endIt; ++it)
    {
      const DimensionElement* pElt = *it;
      VALID_PTR(pElt);
      if (pElt->name() == name)
        {
          ret = pElt;
          break;
        }
    }
  return ret;
}

void
NCMLParser::addDimension(DimensionElement* pCopyToAdd)
{
  VALID_PTR(pCopyToAdd);
  if (getDimension(pCopyToAdd->name()))
    {
      THROW_NCML_INTERNAL_ERROR("NCMLParser::addDimension(): already found dimension with name while adding " + pCopyToAdd->toString());
    }

  _dimensions.push_back(pCopyToAdd);

  BESDEBUG("ncml", "Added dimension to table.  Dimension Table is now: " << printDimensions() << endl);
}

string
NCMLParser::printDimensions() const
{
  string ret ="Dimensions = {\n";
  vector<DimensionElement*>::const_iterator endIt = _dimensions.end();
  vector<DimensionElement*>::const_iterator it;
  for (it = _dimensions.begin(); it != endIt; ++it)
    {
      ret += (*it)->toString() + "\n";
    }
  ret += "}";
  return ret;
}

void
NCMLParser::deleteDimensions()
{
  while(!_dimensions.empty())
    {
      DimensionElement* pElt = _dimensions.back();
      _dimensions.pop_back();
      delete pElt;
    }
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



} // namespace ncml_module




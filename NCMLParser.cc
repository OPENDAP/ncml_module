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
//#include "BESDASResponse.h"
#include "BESDDSResponse.h"
//#include "BESDapError.h"
//#include "BESDataDDSResponse.h"
#include "BESDebug.h"
#include "BESInfo.h"
#include "BESContainer.h"
#include "BESVersionInfo.h"
#include "BESDataNames.h"
//#include "BESResponseHandler.h"
//#include "BESResponseNames.h"
//#include "BESVersionInfo.h"
//#include "BESTextInfo.h"
#include "BESDataHandlerInterface.h"
#include "DAS.h"
#include "DDS.h"
#include "DDSLoader.h"
#include "InternalErr.h"
#include "SaxParserWrapper.h"

using namespace ncml_module;

// An attribute or variable with type "Structure" will match this string.
const string NCMLParser::STRUCTURE_TYPE("Structure");

DDS*
NCMLParser::getDDS() const
{
  assert(_ddsResponse);
  if (!_ddsResponse)
    {
      BESDEBUG("ncml", "Error: getDDS called when we're not processing a <netcdf> location and _ddsResponse is null!" << endl);
      throw new BESInternalError("Null pointer", __FILE__, __LINE__);
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

  while (!_attrContainerStack.empty())
    {
      _attrContainerStack.pop();
    }

  _processingSimpleAttribute = false;
  _currentScope.clear();

  // if non-null, we still own it.
  delete _ddsResponse;
  _ddsResponse = 0;
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
NCMLParser::processAttributeAtCurrentScope(const string& name, const string& type, const string& value)
{
  printScope();

  // First, if the type is a Structure, we are dealing with nested attributes and need to handle it separately.
  if (type == STRUCTURE_TYPE)
    {
      BESDEBUG("ncml", "Processing an attribute element with type Structure." << endl);
      processAttributeContainerAtCurrentScope(name, type, value);
    }
  else // Just look it up in the pTable and add a new one or mutate an existing one.
    {
      // We know it's not a structure, so expect it to close immediately.
      _processingSimpleAttribute = true;

      // Lookup the given attribute in the current table.
      // If it exists, we want to mutate it
      // If not, we want to add a new one.
      AttrTable::Attr_iter attr;
      bool foundIt = findAttribute(_pCurrentTable, name, attr);
      if (foundIt)
      {
          // Mutate the values.
        BESDEBUG("ncml", "Found existing attribute named: " << name << " and changing to type=" << type << " and value=" << value << endl);

        // I can't see any easy way to mutate an existing attribute, so  I think I will just delete and readd it with the new values.  Not this
        // will reorder the attributes!!  Is order supposed to be preserved?
        // TODO We probably want to make sure this isn't a container as well.
        _pCurrentTable->del_attr(name);
        _pCurrentTable->append_attr(name, type, value);
      }
      else
        {
          BESDEBUG("ncml", "Didn't find attribute: " << name << " so adding it with type=" << type << " and value=" << value << endl );
          addNewAttribute(_pCurrentTable, name, type, value);
        }
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
      BESDEBUG("ncml", "Attribute container was not found, creating new one name=" << name << " at scope=" << getScopeString() << endl);
    }
  else
    {
      BESDEBUG("ncml", "Found an attribute container name=" << name << " at scope=" << getScopeString() << endl);
    }

  // Either way, pAT is now the new scope, so push it
  _pCurrentTable = pAT;
  _attrContainerStack.push(pAT);
  enterScope(ScopeEntry(ATTRIBUTE_CONTAINER, name));
  printScope();

}

AttrTable*
NCMLParser::getGlobalAttrTable()
{
  return &(getDDS()->get_attr_table());
}

bool
NCMLParser::findAttribute(AttrTable* pTable, const string& name, AttrTable::Attr_iter& attr) const
{
  AttrTable::Attr_iter endIt = pTable->attr_end();

  for (attr = pTable->attr_begin(); attr != endIt; ++attr)
  {
    if (name == (*attr)->name)
    {
      break;
    }
  }
  // return whether we found it or not.
  return attr != endIt;
}

void
NCMLParser::addNewAttribute(AttrTable* pTable, const string& name, const string& type, const string& value)
{
  pTable->append_attr(name, type, value);
}


/** Recursion helper:
 *  Recurse on the members of composite variable consVar and recursively add their AttrTables
 *  to the given dasTable for the container.
 */
static void populateAttrTableForContainerVariableRecursive(AttrTable* dasTable, Constructor* consVar)
{
  BESDEBUG("ncml", "Recursively adding attribute tables for children of composite variable " << consVar->name() << "..." << endl);
  Constructor::Vars_iter endIt = consVar->var_end();
  for (Constructor::Vars_iter it = consVar->var_begin(); it != endIt; ++it)
    {
       BaseType* var = *it;
       assert(var);
       BESDEBUG("ncml", "Adding attribute table for var: " << var->name() << endl);
       // Make a new table for the child variable
       AttrTable* newTable = new AttrTable(var->get_attr_table());
       // Add it to the DAS's attribute table for the consVar scope.
       dasTable->append_container(newTable, var->name());

       // If it's a container type, we need to recurse.
       if (var->is_constructor_type())
         {
           Constructor* child = dynamic_cast<Constructor*>(var);
           assert(child);
           if (!child) throw new BESInternalError("Type cast error", __FILE__, __LINE__);

           BESDEBUG("ncml", "Var " << child->name() << " is composite, so recursively adding attribute tables" << endl);
           populateAttrTableForContainerVariableRecursive(newTable, child);
         }
    }
}


// This is basically the opposite of transfer_attributes.
void
NCMLParser::populateDASFromDDS(DAS* das, const DDS& dds_const)
{
  BESDEBUG("ncml", "Populating a DAS from a DDS...." << endl);

  // Make sure the DAS is empty tostart.
  das->erase();

  // dds is semantically const in this function, but the calls to it aren't...
  DDS& dds = const_cast<DDS&>(dds_const);

  // First, make sure we don't have a container at top level since we're assuming for now
  // that we only have one dataset per call (right?)
  if (dds.container())
    {
      BESDEBUG("ncml", "populateDASFromDDS got unexpected container " << dds.container_name() << " and is failing." << endl);
      throw new BESInternalError("Unexpected Container Error creating DAS from DDS in NCMLHandler", __FILE__, __LINE__);
    }

  // Copy over the global attributes table
  //BESDEBUG("ncml", "Coping global attribute tables from DDS to DAS..." << endl);
  *(das->get_top_level_attributes()) = dds.get_attr_table();

  // For each variable in the DDS, make a table in the DAS.
  //  If the variable in composite, then recurse
 // BESDEBUG("ncml", "Adding attribute tables for all DDS variables into DAS recursively..." << endl);
  DDS::Vars_iter endIt = dds.var_end();
  for (DDS::Vars_iter it = dds.var_begin(); it != endIt; ++it)
    {
      // For each BaseType*, copy its table and add to DAS under its name.
      BaseType* var = *it;
      assert(var);

     // BESDEBUG("ncml", "Adding attribute table for variable: " << var->name() << endl);
      AttrTable* clonedVarTable = new AttrTable(var->get_attr_table());
      das->add_table(var->name(), clonedVarTable);

      // If it's a container type, we need to recurse.
      if (var->is_constructor_type())
        {
          Constructor* consVar = dynamic_cast<Constructor*>(var);
          assert(consVar);
          if (!consVar) throw new BESInternalError("Type cast error", __FILE__, __LINE__);

          populateAttrTableForContainerVariableRecursive(clonedVarTable, consVar);
        }
    }
}

// This function was added since DDS::operator= had some bugs we need to fix.
// At that point, we can just use that function, probably.
void
NCMLParser::copyVariablesAndAttributesInto(DDS* dds_out, const DDS& dds_in)
{
  // Avoid obvious bugs
  if (dds_out == &dds_in)
    {
      return;
    }

  // handle semantic constness
  DDS& dds = const_cast<DDS&>(dds_in);

  // Copy the global attribute table
  dds_out->get_attr_table() = dds.get_attr_table();

  // copy the things pointed to by the variable list, not just the pointers
  // add_var is designed to deepcopy *i, so this should get all the children
  // as well.
  for (DDS::Vars_iter i = dds.var_begin(); i != dds.var_end(); ++i)
    {
       dds_out->add_var(*i); // add_var() dups the BaseType.
    }

  // for safety, make sure the factory is 0.  If it isn't we might have a double delete.
  assert(!dds_out->get_factory());
}

void
NCMLParser::changeMetadataDirective(SourceMetadataDirective newVal)
{
  // Only go back to default if we're not processing a <netcdf> node.
  if (!_parsingLocation && newVal != PERFORM_DEFAULT)
    {
      throw new BESInternalError("<readMetadata/> or <explicit/> element found outside <netcdf> tree.", __FILE__, __LINE__);
    }

  // If it's already been set by the file (ie not default) can't change it (unless to PROCESSED).
  if (_parsingLocation && _metadataDirective != PERFORM_DEFAULT && newVal != PROCESSED)
    {
      throw new BESInternalError("NcML file must contain one of <readMetadata/> or <explicit/>", __FILE__, __LINE__);
    }

  // Also an error to unprocess during a location parse if we already explicitly set it.
  if (_parsingLocation && _metadataDirective != PERFORM_DEFAULT && newVal != PROCESSED)
    {
      throw new BESInternalError("Logic error.", __FILE__, __LINE__);
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
        clearAllMetadata();
    }

  // In all cases, only do this once per location.
  changeMetadataDirective(PROCESSED);
}

void
NCMLParser::clearAllMetadata()
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
        if (!compositeVar) throw new BESInternalError("Cast error", __FILE__, __LINE__);
        for (Constructor::Vars_iter it = compositeVar->var_begin(); it != compositeVar->var_end(); ++it)
          {
             clearVariableMetadataRecursively(*it);
          }
    }
}


void
NCMLParser::enterScope(const ScopeEntry& entry)
{
  _currentScope.push_back(entry);
}

void
NCMLParser::exitScope()
{
  _currentScope.pop_back();
}

string
NCMLParser::getScopeString() const
{
  string scope("");
  for (unsigned int i=0; i<_currentScope.size(); ++i)
    {
      if (i > 0)
        {
          scope.append(".");  // append scoping operator if not first entry
        }
      scope.append(_currentScope[i].name);
    }
  return scope;
}

void
NCMLParser::printScope() const
{
  BESDEBUG("ncml", "Current fully qualified scope is: " << getScopeString() << endl);
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
,  _parsingLocation(false)
, _loader(loader)
, _ddsResponse(0)
, _metadataDirective(PERFORM_DEFAULT)
, _pVar(0)
, _pCurrentTable(0)
, _attrContainerStack()
, _processingSimpleAttribute(false)
, _currentScope()
{
  BESDEBUG("ncml", "Created NCMLHelper." << endl);
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
       throw new BESInternalError(msg, __FILE__, __LINE__);
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
      BESDEBUG("ncml", "handleBeginVariable could not find AttrTable for variable=" << varName <<
                " and cannot create new variables in this version." << endl);
      throw BESInternalError("handleBeginVariable failed to locate variable.", __FILE__, __LINE__);
    }
  // this sets the _pCurrentTable as well.
  setCurrentVariable(pVar);
  // Add the new scope to the debugging stack
  enterScope(ScopeEntry(VARIABLE, varName));
  printScope();

  // BESDEBUG("ncml", "Found variable named:" << pVar->name() << endl);
}

void
NCMLParser::handleEndVariable()
{
  // pop the attr table back upwards to the previous one
  // I think we can just use the parent of the current...
  BESDEBUG("ncml", "handleEndVariable called at scope:" << getScopeString() << endl);
  if (!withinVariable())
    {
      throw BESInternalError("handleEndVariable called when not parsing a variable element", __FILE__, __LINE__);
    }

  // New context is the parent, which could be NULL for top level DDS vars.
  // We might want to use an explicit local stack to help debugging, TODO
  setCurrentVariable(_pVar->get_parent());
  exitScope(); // pop the stack
  printScope();

  BESDEBUG("ncml", "Var context now: " << ((_pVar)?(_pVar->name()):("NULL")) << endl);
}

void
NCMLParser::handleBeginAttribute(const string& name, const string& type, const string& value)
{
  // TODO might want to print our current container, be it global or a variable...
  BESDEBUG("ncml", "handleBeginAttribute called for attribute name=" << name << endl);

  processMetadataDirectiveIfNeeded();

  // If we are processing a simple attribute, we should not get a new elemnent before closing the old!  Parse error!
  if (_processingSimpleAttribute)
    {
      const string msg = "Parse error: handleBeginAttribute called before a an atomic attribute was closed!";
      BESDEBUG("ncml", msg << " Scope=" << getScopeString() << endl);
      throw new BESInternalError(msg, __FILE__, __LINE__);
    }

  // Make sure we're in a netcdf and then process the attribute at the current table scope,
  // which could be anywhere including glboal attributes, nested attributes, or some level down a variable tree.
  if (withinLocation())
    {
      printScope();
      processAttributeAtCurrentScope(name, type, value);
    }
  else
    {
      BESDEBUG("ncml", "Parse error: Got handleBeginAttribute when not within a <netcdf> node!");
      throw new BESInternalError("Parse error.", __FILE__, __LINE__);
    }

}

void
NCMLParser::handleEndAttribute()
{
  BESDEBUG("ncml", "handleEndAttribute called at scope:" << getScopeString() << endl);

  // if it wasn't a container, just clear out this state
  if (_processingSimpleAttribute)
    {
      _processingSimpleAttribute = false;
    }
  else // it was a container!
    {
      assert(!_attrContainerStack.empty());  // It better be there or we got a logic error!

      // Walk the scope up a level.
      // First pop off the container stack
      AttrTable* pAT = _attrContainerStack.top();
      _attrContainerStack.pop();

      // Set the current table to the parent of the one we just popped off.
      // That SHOULD be the correct location, no matter where we are.
      // TODO technically I probably don't need a full stack of these due to back pointers, do i?
      // Well, at least it lets me know if I am in a attribute structure or not.
      _pCurrentTable = pAT->get_parent();
      assert(_pCurrentTable); // This better be valid or something is really broken!
      exitScope();
      printScope();
    }
}


void
NCMLParser::onStartDocument()
{
}

void
NCMLParser::onEndDocument()
{
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

void
NCMLParser::onCharacters(const std::string& content)
{
  // TODO Once the scope stack is refactored and handles atomic attributes,
  // implement this!!
  if (!_processingSimpleAttribute)
    {
      // This is likely whitespace in the stream.  TODO make sure it's whitespace or else throw an error.
    }
  else
    {
       string msg = "onCharacters() currently unsupported!  Please add <attribute> values as <attribute value=\"...\".  Content was: " + content;
       BESDEBUG("ncml", msg << endl);
       throw BESInternalError(msg, __FILE__, __LINE__);
    }
}

void
NCMLParser::onParseWarning(std::string msg)
{
  BESDEBUG("ncml", "PARSE WARNING: " << msg << endl);
}

// Pretty much have to give up on malformed XML.
void
NCMLParser::onParseError(std::string msg)
{
  BESDEBUG("ncml", "PARSE ERROR: " << msg << ".  Terminating parse!" << endl);
  throw new BESInternalError(msg, __FILE__, __LINE__);
}




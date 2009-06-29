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

#ifndef NCMLHELPER_H_
#define NCMLHELPER_H_

#include "config.h"

#include <stack>
#include <string>
#include <vector>

#include "SaxParser.h" // interface superclass
#include "AttrTable.h" // needed due to parameter with AttrTable::Attr_iter

//FDecls
namespace libdap
{
  class BaseType;
  class DAS;
  class DDS;
};
class BESDDSResponse;


using namespace libdap;
using namespace std;

namespace ncml_module
{
  class DDSLoader;
}

/**
 *  @brief INCOMPLETE NcML Parser for adding AIS to existing local datasets.
 *
 *  Core engine for parsing an NcML structure and modifying the DDS of a single dataset by adding new metadata to it.
 *  (http://docs.opendap.org/index.php/AIS_Using_NcML).
 *
 *  Limitations Expected for the initial version:
 *
 *  o We only handle local (same BES) datasets with this version (hopefully we can relax this to allow remote dataset augmentation
 *      as a bes.conf option or something.
 *
 *  o We can only handle a single <netcdf> node, i.e. we can only augment one location with metadata.  Future versions will allow aggregation, etc.
 *
 *  For the purposes of this class "scope" will mean the attribute table at some place in the DDX (populated DDS object), be it at
 *  the global attribute table, inside a nested attribute container, inside a variable, or inside a nested variable (Structure, e.g.).
 *  This is basically the same as a "fully qualified name" in DAP 2 (ESE-RFC-004.1.1)
 *
 *  This parser can load a given DDX for a single netcdf@location and then modify it by:
 *
 *  0) Clearing all attributes if <explicit/> tag is given.
 *  1) Adding or modifying existing atomic attributes at any scope (global, variable, nested variable, nested attribute container)
 *  2) Adding (or traversing the scope of existing) attribute containers using <attribute type="Structure"> to refer to the container.
 *  3) Traversing a hierarchy of variable scopes to set the scope for 1) or 2)
 *
 *  We maintain a pointer to the currently active AttrTable as we get SAX parser calls.  As we enter/exit attribute containers or
 *  Constructor variables we keep track of this on a scope stack which allows us to know the fully qualifed name of the current scope.
 *  (We need a minor refactor to fix this for attribute containers, see below).
 *
 *  TODO Refactor the scope stack to handle more specific scope types for better error checking in the parse.  Included in this is
 *  rolling the attribute container stack and simple_attribute flag into the ScopeEntry/ScopeStack pattern to avoid the duplicated state
 *  and simplify everything.  Also will allow us to know the scope of atomic attribute for values in the element content not attribute.
 *
 *  TODO @BUG We do not split atomic attribute values into a vector<string> on whitespace (or attribute@separator)
 *
 *  TODO @BUG Add ability to handle <attribute>some values></attribute> as well as <attribute value="some values"/>  NcML allows for both.
 *
 *  TODO @BUG Fails to handle Constraints at all now.  This needs to be thought through more.
 *
 *  TODO We don't handle attribute@separator or attribute@orgName now (will we?)
 *
 *  @author mjohnson <m.johnson@opendap.org>
 */
namespace ncml_module
{

class NCMLParser : public SaxParser
{
private:

  // The current scope is either global attribute table, within a variable's attribute table,
  // or within an attribute container. (TODO add more specific contexts to this)
  enum ScopeSource { GLOBAL=0, VARIABLE, ATTRIBUTE_CONTAINER};

  // Each time we traverse down into containers, we'll
  // push one of these on a stack to keep track of where we
  // are in a parse for debugging.
  // TODO refactor this class and the actual vector<ScopeEntry> into
  // its own helper class rather than have it in place in NCMLParser.
  struct ScopeEntry
  {
    ScopeEntry(ScopeSource the_type, string the_name)
    : type(the_type)
    , name(the_name)
    {}

    ScopeSource type;
    string name;
  };

  // Default means we've seen no tag for thie lcoation yet, READ_METADATA says we saw <readMetadata/> and EXPLICIT means we saw <explicit/>
  // Processed means the directive was already processed for the current location.
  enum SourceMetadataDirective { PERFORM_DEFAULT=0, READ_METADATA, EXPLICIT, PROCESSED };

private: // data rep
  string _filename; // name of the ncml file we need to parse

  bool _parsingLocation; // true if we have entered a location element and not closed it yet. (todo consider making this a string with loc name)

  // Handed in at creation, this is a helper to load a given DDS.
  DDSLoader& _loader;

  // The response object containing the DDS for the <netcdf> node we are processing.
  BESDDSResponse* _ddsResponse;

  SourceMetadataDirective _metadataDirective; // what to do with existing metadata after it's read in from parent.

  // pointer to currently processed variable, or NULL if none (ie we're at global level).
  BaseType* _pVar;

  // the current attribute table for the scope we are in.  Global table if not within a variable,
  // table for the variable if we are in a variable.
  // Also, if we have a nested attribute table (ie attribute table contains a container) this will be
  // the table for the scope at the current parse point.
  AttrTable* _pCurrentTable;

  // As we handle nested attribute containers (nested attributes),
  // we push them onto this stack to be sure we update state
  // properly as we close the elements and exit scope.
  // This will be empty unless we are in nested attribute.
  stack<AttrTable*> _attrContainerStack;

  // This will be true between a handleBeginAttribute and handleEndAttribute call for
  // a simple type attribute, but not for a container.  Since these will be leaves,
  // we shouldn't get any other calls except for the content string.
  bool _processingSimpleAttribute;

  // As we parse, we'll use this as a stack for keeping track of the current
  // scope we're in.  In other words, this stack will refer to the container where _pCurrTable is in the DDS.
  // if empty() then we're in global dataset scope (or no scope if not parsing location yet).
  vector<ScopeEntry> _currentScope;

private: //methods

  // Helper to get the dds from _ddsResponse
  // Error to call _ddsResponse is null.
  DDS* getDDS() const;

  // Clear any volatile parse state (basically after each netcdf node).
  void resetParseState();

  // Look up the variable called varName in pContainer.  If !pContainer, look in _dds top level.
 BaseType* getVariableInContainer(const string& varName, BaseType* pContainer);

 BaseType* getVariableInDDS(const string& varName);

 /**
  *  Set the current scope to the variable pVar and update the _pCurrentTable to reflect this variables attributetable.
  *  If pVar is null and there is a valid dds, then set _pCurrentTable to the global attribute table.
  */
 void setCurrentVariable(BaseType* pVar);

  /**
   *   At current scope (_pCurrentTable), either add the attribute given by type, name, value if it doesn't exist
   *   or change the value of the attribute if it does exist.
   *   If type=="Structure", we assume the attribute is a container and will handle it differently.
   *   @see processAttributeContainerForTable
   */
  void processAttributeAtCurrentScope(const string& name, const string& type, const string& value);

  /**
   * Given an attribute with type==Structure at current scope _pCurrentTable, either add a new container to if
   * one does not exist with /c name, otherwise assume we're just specifying a new scope for a later child attribute.
   * In either case, push the scope of the container into our instance's scope for future calls.
   */
  void processAttributeContainerAtCurrentScope(const string& name, const string& type, const string& value);

  // Pulls global table out of the current DDS...  TODO figure out what to do about this when we aggregate
  AttrTable* getGlobalAttrTable();

  // Try and find an attribute with name in pTable without recursing.
  // If found, attr will point to it, else pTable->attr_end().
  // Return whether it was found or not
  bool findAttribute(AttrTable* pTable, const string& name, AttrTable::Attr_iter& attr) const;

  // Create a new attribute and put it into pTable
  void addNewAttribute(AttrTable* pTable, const string& name, const string& type, const string& value);

  // Return whether we are inside a location element <netcdf> at this point of the parse.
  bool withinLocation() const { return _parsingLocation; }

  // Returns whether we are inside a variable element at current parse point.
  // Note we could be nested down within multiple variables or attribute containers,
  // but this will be true if anywhere in current scope we're within a variable.
  bool withinVariable() const { return _parsingLocation && _pVar; }

  // Return whether the current scope is within an attribute container, ie a nested attribute.
  // Since it's illegal to have variables within nested attribute containers,
  // this refers only to the immediate scope, unlike /c withinVariable and /c withinLocation
  // @see withinVariable
  // @see withinLocation
  bool isCurrentScopeAnAttributeContainer() const { return !_attrContainerStack.empty(); }

  // Change the <explicit> vs <readMetadata> functionality, but doing error checking for parse errors.
  void changeMetadataDirective(SourceMetadataDirective newVal);

  // If not already PROCESSED, will perform the metadata directive, effectively
  // clearing the DDS attributes if if was set to explicit.
  void processMetadataDirectiveIfNeeded();

  // If we got an explicit tag, we need to clear out all metadata before processing.
  // This clears out the current DDS's metadata recursively.
  void clearAllMetadata();

  // Clear the attribute table for \c var .  If composite, recurse.
  void clearVariableMetadataRecursively(BaseType* var);

  // Push the new scope
  void enterScope(const ScopeEntry& entry);

  // Pop off the last scope to move up a level.
  void exitScope();

  // Turn the current scope into a fully qualified name
  string getScopeString() const;

  // Print getScopeString using BESDEBUG
  void printScope() const;

public: // Class Helpers  TODO These should get refactored somewhere else.

  /** The string describing the type "Structure" */
  static const string STRUCTURE_TYPE;

  /** Given we have a valid attribute tree inside of the DDS, recreate it in the DAS.
       @param das the das to clear and populate
       @param dds_const the source dds
       */
    static void populateDASFromDDS(DAS* das, const DDS& dds_const);

    /** Make a deep copy of the global attributes and variables within dds_in
     * into *dds_out.
     * Doesn't affect any other top-level data.
     * @param dds_out place to copy global attribute table and variables into
     * @param dds_in source DDS
     */
    static void copyVariablesAndAttributesInto(DDS* dds_out, const DDS& dds_in);

public:
  /**
   * @brief Create a structure that can parse an NCML filename and returned a transformed DDX response.
   *
   * @param loader helper for loading a DDX for locations referred to in the ncml.
   */
  NCMLParser(DDSLoader& loader);

  /**
   * Destroys our allocated BESDDSResponse if we have not returned it in a successful parse.
   * This will work if an exception leads to the destruction of the parser in a call to parse().
   */
  virtual ~NCMLParser();

private:
  /** illegal */
  NCMLParser(const NCMLParser& from);

  /** illegal */
  NCMLParser& operator=(const NCMLParser& from);

public:

  /** @brief Parse the NcML filename, returning a newly allocated DDS response containing the underlying dataset
   *  transformed by the NcML.  The caller owns the returned memory.
   *
   *  On a parse error, an BESInternalError will be thrown.  TODO perhaps make my own exception for catching separately upstairs.
   *
   *  @return a new response object with the transformed DDS in it.  The caller assumes ownership of the returned object.
  */
  BESDDSResponse* parse(const string& ncmlFilename);

  bool parsing() const { return !_filename.empty(); }

  //////////////////////////////
  /// Core Parser Calls

  /**
   *  Called on parsing <netcdf location="foo"> element.
   *  Loads the given (local!) location into a new DDX for subsequent transformations.
   *  The current parse scope becomes the loaded dataset's scope.
   *
   *  @throw If the underlying location fails to load, throw an exception.
   */
  void handleBeginLocation(const string& location);

  /**
   * Called on </netcdf>.  Scope becomes empty as we are not in a dataset any longer.
   * Should be the last call before a document end.
   */
  void handleEndLocation();

  /**
   *  Called on <readMetadata/>
   *  This is the default, so ignore unless we already got handleExplicit().
   */
  void handleReadMetadata();

  /**
   * Called on <explicit/>
   * Informs the parser to remove all read in metadata from the dataset before adding the new attributes.
   * Mututally exclusive with handleReadMetadata within in <netcdf>
   * @exception if handleReadMetadata() was already called before handleEndLocation().
   */
  void handleExplicit();

  /**
   * Called on <variable type="foo"> elements, passing down the type from the attribute.
   * Pushes the scope of the variable onto the context, whether simple or structure.
   *
   * NOTE: We default to empty type, assuming the caller doesn't want to type check, but
   * just walk the hierarchy down since we cannot add new variables yet.
   */
  void handleBeginVariable(const string& varName, const string& type="");

  /**
   * Called on </variable>
   * Pops the variable's scope from the context.
   */
  void handleEndVariable();

  /**
   * Called on <attribute name="foo" type="bar" value="baz">.
   *
   * If type == "Structure", the attribute is considered a container (with no value!)
   * If the named attribute container is not found, a new attribute container is added to the current scope.
   * In both cases (found or new), the attribute container's scope is pushed onto the context.
   *
   * NOTE: A leaf attribute is not currently pushed, but this needs to be done to handle characters content, see below.
   *
   * @exception  An parse exception will be thrown if this call is made while already parsing a leaf attribute.
   *
   * TODO BUG Since NcML allows values as the character content of the element, I need to handle that.
   * Currently, we expect the value string to be contained within value now.
   *
   * TODO BUG This fails to split() the value into a vector if type != (String or Structure)!
   */
  void handleBeginAttribute(const string& name, const string& type, const string& value);

  /**
   * Called on </attribute>.
   * If it was an attribute container, pops the scope of the attribute container from the context.
   */
  void handleEndAttribute();

  //////////////////////////////
  // Interface SaxParser

  virtual bool onStartDocument();
  virtual bool onEndDocument();
  virtual bool onStartElement(const std::string& name, const AttrMap& attrs);
  virtual bool onEndElement(const std::string& name);
  virtual bool onCharacters(const std::string& content);
  virtual bool onParseWarning(std::string msg);
  virtual bool onParseError(std::string msg);

}; // class NCMLParser

} //namespace ncml_module

#endif /* NCMLHELPER_H_ */

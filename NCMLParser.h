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

#ifndef __NCML_MODULE_NCML_PARSER_H__
#define __NCML_MODULE_NCML_PARSER_H__

#include "config.h"

#include <memory>
#include <stack>
#include <string>
#include <vector>

#include "SaxParser.h" // interface superclass
#include "AttrTable.h" // needed due to parameter with AttrTable::Attr_iter
#include "ScopeStack.h";

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
 *  @brief NcML Parser for adding/modifyinbg/removing AIS to existing local datasets
 *
 *  Core engine for parsing an NcML structure and modifying the DDS (which includes attribute data)
 *  of a single dataset by adding new metadata to it. (http://docs.opendap.org/index.php/AIS_Using_NcML).
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
 *  1) Clearing all attributes if <explicit/> tag is given.
 *  2) Adding or modifying existing atomic attributes at any scope (global, variable, nested variable, nested attribute container)
 *  3) Adding (or traversing the scope of existing) attribute containers using <attribute type="Structure"> to refer to the container.
 *  4) Traversing a hierarchy of variable scopes to set the scope for 1) or 2)
 *  5) We handle orgName renaming for attribute@orgName only.
 *  6) We can remove an attribute (including container recursively) at any scope with <remove/>
 *
 *  Design and Control Flow:
 *
 *  1) We use the SaxParser interface callbacks to handle calls from a SAX parser (or potentially DOM treewalk) which we create on parse() call.
 *
 *  2) We assume a single <netcdf> node with a local data in netcdf@location.  When we parse this node, we use a DDSLoader to
 *  create a new DDS with DDX information (full AttrTable's) in it.
 *
 *  3) We maintain a pointer to the currently active AttrTable as we get other SAX parser calls.  As we enter/exit the lexical scope of
 *  attribute containers or Constructor variables we keep track of this on a scope stack which allows us to know the fully qualified name
 *  of the current scope as well as the type of the innermost scope for error checking.
 *
 *  4) As we process NcML elements we modify the DDS as needed.
 *
 *  5) When complete, we return the loaded and transformed DDS to the caller.
 *
 *  We throw BESInternalError for logic errors in the code.
 *
 *  We throw BESSyntaxUserError for parse errors in the NcML or for user requests to modify nonexisting entities or for non-existent scopes, etc.
 *
 *  TODO REFACTOR We really need to get rid of all the long const string& parameter lists and make them into a struct to avoid misordering errors.

 *  TODO REFACTOR Related to above, Change dispatcher from switch to factory and polymorphic processing with NCMLElement abstract class, etc.
 *      This solves th long parameter list issue and will make things more robust moving forward with new elements and functionality.
 *
 *  @author mjohnson <m.johnson@opendap.org>
 */
namespace ncml_module
{

class NCMLParser : public SaxParser
{
private:

  /**
  * Whether <explicit/> or <readMetadata/> was specified.
  * PERFORM_DEFAULT means we have not see either tag.
  * READ_METADATA says we saw <readMetadata/>
  * EXPLICIT means we saw <explicit/>
  * PROCESSED means the directive was already processed for the current location since it happens only once.
  */
  enum SourceMetadataDirective { PERFORM_DEFAULT=0, READ_METADATA, EXPLICIT, PROCESSED };

private: // data rep

  // If true, we will consider unknown ncml elements as parse errors and raise exception.
  // If false, we just BESDEBUG the warning and ignore them entirely.
  static bool sThrowExceptionOnUnknownElements;

  // name of the ncml file we are parsing
  string _filename;

  // true if we have entered a location element and not closed it yet.
  bool _parsingLocation;

  // Handed in at creation, this is a helper to load a given DDS.  It is assumed valid for the life of this.
  DDSLoader& _loader;

  // The response object containing the DDS for the <netcdf> node we are processing, or null if not processing.
  auto_ptr<BESDDSResponse> _ddsResponse;

  // what to do with existing metadata after it's read in from parent.
  SourceMetadataDirective _metadataDirective;

  // pointer to currently processed variable, or NULL if none (ie we're at global level).
  BaseType* _pVar;

  // the current attribute table for the scope we are in.  Global table if not within a variable,
  // table for the variable if we are in a variable.
  // Also, if we have a nested attribute table (ie attribute table contains a container) this will be
  // the table for the scope at the current parse point.
  AttrTable* _pCurrentTable;

  // As we parse, we'll use this as a stack for keeping track of the current
  // scope we're in.  In other words, this stack will refer to the container where _pCurrTable is in the DDS.
  // if empty() then we're in global dataset scope (or no scope if not parsing location yet).
  ScopeStack _scope;

  // Temp storage for tokenizing value strings without constantly making new vector<string>'s.
  std::vector<std::string> _tokens;

  // Used by the tokenizer.  If we get attribute@separator attribute in handleBeginAttribute
  // We set the given string here for the tokenizer to know for onCharacters call.
  // It defaults to whitespace.
  std::string _separators;

private: //methods

  /** Is the innermost scope an atomic (leaf) attribute? */
  bool isScopeAtomicAttribute() const;

  /** Is the inntermost scope an attribute container? */
  bool isScopeAttributeContainer() const;

  /** Is the innermost scope an non-Constructor variable? */
  bool isScopeSimpleVariable() const;

  /** Is the innermost scope a hierarchical (Constructor) variable? */
  bool isScopeCompositeVariable() const;

  /** Is the innermost scope a variable of some sort? */
  bool isScopeVariable() const { return (isScopeSimpleVariable() || isScopeCompositeVariable()); }

  /** Is the innermost scope the global attribute table of the DDS? */
  bool isScopeGlobal() const;

  /**  Are we inside the scope of a location element <netcdf> at this point of the parse?
   * Note that this means anywhere in the the scope stack, not the innermost (local) scope
  */
  bool withinLocation() const { return _parsingLocation; }

  /** Returns whether there is a variable element on the scope stack SOMEWHERE.
   *  Note we could be nested down within multiple variables or attribute containers,
   *  but this will be true if anywhere in current scope we're nested within a variable.
  */
  bool withinVariable() const { return _parsingLocation && _pVar; }

  /** Helper to get the dds from _ddsResponse
   *  Error to call _ddsResponse is null.
   */
  DDS* getDDS() const;

  /** Clear any volatile parse state (basically after each netcdf node).
   * Also used by the dtor.
   */
  void resetParseState();

  /** Look up the variable called varName in pContainer.
       If !pContainer, look in _dds top level.
       @return the BaseType* or NULL if not found.
   */
 BaseType* getVariableInContainer(const string& varName, BaseType* pContainer);

 /** Lookup and return the variable named varName in the DDS and return it.
  * @param varName name of the variable to find in the top level getDDS().
  * @return the variable pointer else NULL if not found.
  */
 BaseType* getVariableInDDS(const string& varName);

 /**
  *  Set the current scope to the variable pVar and update the _pCurrentTable to reflect this variables attributetable.
  *  If pVar is null and there is a valid dds, then set _pCurrentTable to the global attribute table.
  */
 void setCurrentVariable(BaseType* pVar);

 /** Return whether the actual type of \c var match the type \c expectedType.
  *  Here expectedType is assumed to have been through the @@@
  *  Special cases for NcML:
  *  1) We map expectedType == "Structure" to match DAP Constructor types: Grid, Sequence, Structure.
  *  2) We define expectedType.empty() to match ANY DAP type.
  */
 static bool typeCheckDAPVariable(const BaseType& var, const string& expectedType);

  /**
   *   @brief Top level call for handling all attribute types and operations (addition, rename, change values).
   *   At current scope (_pCurrentTable), either add the attribute given by type, name, value if it doesn't exist
   *   or change the value of the attribute if it does exist.
   *   If type=="Structure", we assume the attribute is a container and will handle it differently (@see processAttributeContainerForTable)
   *   Also if orgName is !empty(), we need to look that name up in current scope and change it to \c name.
   *   This calls the more specific functions:
   *   @see processAtomicAttributeAtCurrentScope
   *   @see processAttributeContainerAtCurrentScope
   */
  void processAttributeAtCurrentScope(const string& name, const string& ncmlType, const string& value, const string& orgName);

  /**
   * @brief Handle addition of, renaming of, and changing values of atomic attributes in current scope.
   */
  void processAtomicAttributeAtCurrentScope(const string& name, const string& type, const string& value, const string& orgName);

  /**
   * Given an attribute with type==Structure at current scope _pCurrentTable, either add a new container to if
   * one does not exist with /c name, otherwise assume we're just specifying a new scope for a later child attribute.
   * In either case, push the scope of the container into our instance's scope for future calls.
   */
  void processAttributeContainerAtCurrentScope(const string& name, const string& type, const string& value, const string& orgName);

  /**
   *  Pulls global table out of the current DDS, or null if no current DDS.
   */
  AttrTable* getGlobalAttrTable();

  /**
   * @return if the attribute with name already exists in the current scope.
   * @param name name of the attribute
  */
  bool attributeExistsAtCurrentScope(const string& name);

  /** Find an attribute with name in the current scope (_pCurrentTable) _without_ recursing.
    * If found, attr will point to it, else pTable->attr_end().
    * Note this works for both atomic and container attributes.
    * @return whether it was found
    * */
  bool findAttribute(const string& name, AttrTable::Attr_iter& attr) const;

  /** Create a new attribute and put it into _pCurrentTable (current scope).
   * @param name name of the attribute
   * @param type DAP type of the attribute
   * @param value attribute value as a string, could be unsplit list of tokens.
   *              If so, they will be split using current _separators and added as an array.
   */
  void addNewAttribute(const string& name, const string& type, const string& value);

  /**
     * Change the existing attribute's value.
     * It is an error to call this if !attributeExistsAtCurrentScope().
     * If type.empty(), then just leave the type as it is now, otherwise use type.
     * @param name local scope name of the attribute
     * @param type type of the attribute, or "" to leave it the same.
     * @param value new value for the attribute, could be unsplit list of tokens.
     *              If so, they will be split using current _separators and added as an array.
     */
  void mutateAttributeAtCurrentScope(const string& name, const string& type, const string& value);

  /**
   * @brief Rename the existing atomic attribute named \c orgName to \c newName
   * If newValue is also specified, replace the old values, otherwise leave the old values intact.
   * @param newName the new name for the attribute.  newName must not already exist at current scope or an exception will be thrown.
   * @param newType a new type for the attribute or empty() if leave the same.
   * @param newValue  new value(s) for the renamed entry to replace the old.  Old values are kept if newValue.empty().
   * @param orgName  the original name of the attribute to rename.  Must be atomic and exist at current scope.
   *
   * @exception Thrown if attribute orgName doesn't exist in scope
   * @exception Thrown if attribute newNme already exists in scope
   * @exception Thrown if attribute orgName is a container and not atomic.
   */
  void renameAtomicAttribute(const string& newName, const string& newType, const string& newValue, const string& orgName);

  /**
   * @brief Rename the existing attribute container at current scope with name orgName to newName.
   * @param newName the new name for the container, must not exist in this scope.
   * @param orgName the original name of the container at current scope, must exist and be a container.
   * @exception thrown if orgName attribute container does not exist or is not an attribute container.
   * @return the attribute container whose name we changed.
   */
  AttrTable* renameAttributeContainer(const string& newName, const string& orgName);

  /** Do the proper tokenization of values for the given dapTypeName into _tokens
   * using current _separators.
   * Strings and URL types produce a single "token"
   * @return the number of tokens added.
   */
  int tokenizeValues(const string& values, const string& dapTypeName);

  /**
   *  Tokenize the given string values for the DAP type using the given delimiters currently in _separators and
   *  Result is stored in _tokens and valid until the next call or until we explicitly clear it.
   *  Special Cases: dapType == String and URL will produce just one token, ignoring delimiters.
   *  It is an error to have any values for dapType==Structure, but for this case
   *  we push a single "" onto _tokens.
   *  @return the number of tokens added to _tokens.
   */
  int tokenizeValuesForDAPType(const string& values, AttrType dapType);

  /**
   * Change the <explicit> vs <readMetadata> functionality, but doing error checking for parse errors
   * @param newVal the new directive
   */
  void changeMetadataDirective(SourceMetadataDirective newVal);

  /** If not already PROCESSED, will perform the metadata directive, effectively
    * clearing the DDS attributes if if was set to explicit.
    * */
  void processMetadataDirectiveIfNeeded();

  /** This clears out ALL the current DDS's AttrTable's recursively. */
  void clearAllAttrTables();

  /** Clear the attribute table for \c var .  If constructor variable, recurse.  */
  void clearVariableMetadataRecursively(BaseType* var);

  /** Push the new scope onto the stack. */
  void enterScope(const string& name, ScopeStack::ScopeType type);

  /** Pop off the last scope */
  void exitScope();

  /** Print getScopeString using BESDEBUG */
  void printScope() const;

  /**  Cleanup state to as if we're a new object */
  void cleanup();

public: // Class Helpers

  /** The string describing the type "Structure" */
  static const string STRUCTURE_TYPE;

  /**
    * Convert the NCML type in ncmlType into a canonical type we will use in the parser.
    * Specifically, we map NcML types to their closest DAP equivalent type,
    * but we leave Structure as Structure since it is assumed to mean Constructor for DAP
    * which is a superclass type.
    * Note this passes DAP types through unchanged as well.
    * It is illegal for \c ncmlType to be empty().
    * We return "" to indicate an error in conversion.
    * @param ncmlType a non empty() string that could be an NcML type (or built-in DAP type)
    * @return the converted type or "" if unknown type.
    */
  static string convertNcmlTypeToCanonicalType(const string& ncmlType);

public:
  /**
   * @brief Create a structure that can parse an NCML filename and returned a transformed DDX response.
   *
   * @param loader helper for loading a DDX for locations referred to in the ncml.
   */
  NCMLParser(DDSLoader& loader);

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
   *  @throw BESSyntaxUserError for parse errors such as bad XML or NcML referring to variables that do not exist.
   *  @throw BESInternalError for assertion failures, null ptr exceptions, or logic errors.
   *
   *  TODO perhaps throw a BESNotFoundError for failures to load the ncmlFilename or for anything netcdf@location failures.
   *  These need to be made clear to the response somehow for a 404 or 500 error as per the use case.
   *
   *  @return a new response object with the transformed DDS in it.  The caller assumes ownership of the returned object.
  */
  auto_ptr<BESDDSResponse> parse(const string& ncmlFilename);

  bool parsing() const { return !_filename.empty(); }

  //////////////////////////////
  /// Core Parser Calls

  /**
   *  Called on parsing <netcdf location="foo"> element.
   *  Loads the given (local) location into a new DDX for subsequent transformations.
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
   * Mututally exclusive with handleReadMetadata within <netcdf> tree
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
   * @brief Top level dispatch call on start of <attribute> element.
   *
   * This call assumes the named attribute is to be added or modified at the current
   * parse scope.  There are two cases we handle:
   *
   * 1) Attribute Containers:   if type=="Structure", the attribute element refers to an attribute container which
   *                            will have no value itself but contain other attributes or attribute containers.
   * 2) Atomic (leaf) Attributes:  these attributes can be of any of the other NcML types (not Structure) or DAP atomic types.
   *
   * If the attribute specified by \c name (or alternatively, \c orgName if non empty) is not found in the current
   * scope, it is created and added with the given type and value.  This applies to containers as well, which must have no value.
   * For the case of atomic attributes (scalar or vector), attribute@value may be empty and the value specified in the characters
   * content of the element:
   * i.e. <attribute name="foo" type="String" value="bar"/> == <attribute name="foo" type="String">bar</attribute>
   *
   * If the named attribute is found, we modify it to have the new type and value (essentially removing the old one and readding it).
   *
   * For vector-valued atomics, we assume whitespace is the separator for the values.  If attribute@separator is non-empty,
   * we use it to split the values.
   *
   * If \c orgName is not empty, the attribute at this scope named \c orgName is to be renamed to \c name.
   *
   * @param name the name of the attribute to add or modify.  Also, the new name if orgName != "".
   * @param type can be any of the NcML types (in which case they're mapped to a DAP type), or a DAP type.
   *             If type == "Structure", the attribute is considered an attribute container and value must be "".
   * @param value the untokenized value for the attribute as specified in the attribute@value.  Note this can be empty
   *              and the value specified on the characters content of the element for the case of atomic types.
   *              If type == "Structure", value.empty() must be true.  Note this can contain multiple tokens
   *              for the case of vectors of atomic types.  Default separator is whitespace, although attribute@separator
   *              can specify a new one.  This is handled at the top level dispatcher.
   * @param separator  if non-empty, use the given characters as separators for vector data rather than whitespace.
   * @param orgName If not empty(), this specifies that the attribute named orgName (which should exist) is to be renamed to
   *                 name.
   *
   * @exception  An parse exception will be thrown if this call is made while already parsing a leaf attribute.
   */
  void handleBeginAttribute(const string& name, const string& type, const string& value,
                            const string& separator=" \t",
                            const string& orgName="");

  /**
   * Called on </attribute>.
   * Pops the attribute scope from the stack
   */
  void handleEndAttribute();

  /** @brief Remove the object at local scope with \c name.
   *
   * We can only remove attributes now (this includes containers, recursively).
   *
   * There is no need for a handleEndRemove since we process it right when we see it.
   *
   *  @throw BESSyntaxUserError if type != "attribute" or type != ""
   *  @throw BESSyntaxUserError if name is not found at the current scope.
   *
   *  @param name local scope name of the object to remove
   *  @param type  the type of the object to remove: we only handle "attribute" now!  (this includes containers).
   */
  void handleBeginRemove(const string& name, const string& type);


  //////////////////////////////
  // Interface SaxParser:  Wrapped calls from the libxml C SAX parser

  virtual void onStartDocument();
  virtual void onEndDocument();
  virtual void onStartElement(const std::string& name, const AttrMap& attrs);
  virtual void onEndElement(const std::string& name);
  virtual void onCharacters(const std::string& content);
  virtual void onParseWarning(std::string msg);
  virtual void onParseError(std::string msg);

}; // class NCMLParser

} //namespace ncml_module

#endif /* __NCML_MODULE_NCML_PARSER_H__ */

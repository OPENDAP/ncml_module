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

#include "AttrTable.h" // needed due to parameter with AttrTable::Attr_iter
#include "DDSLoader.h"
#include "SaxParser.h" // interface superclass
#include "ScopeStack.h"

//FDecls
namespace libdap
{
  class BaseType;
  class DAS;
  class DDS;
};
class BESDapResponse;

class BESDataDDSResponse;
class BESDDSResponse;

namespace ncml_module
{
  class DDSLoader;
  class NCMLElement;
}

using namespace libdap;
using namespace std;


/**
 *  @brief NcML Parser for adding/modifying/removing metadata (attributes) to existing local datasets using NcML.
 *
 *  Core engine for parsing an NcML structure and modifying the DDS (which includes attribute data)
 *  of a single dataset by adding new metadata to it. (http://docs.opendap.org/index.php/AIS_Using_NcML).
 *
 *  For the purposes of this class "scope" will mean the attribute table at some place in the DDX (populated DDS object), be it at
 *  the global attribute table, inside a nested attribute container, inside a variable, or inside a nested variable (Structure, e.g.).
 *  This is basically the same as a "fully qualified name" in DAP 2 (ESE-RFC-004.1.1)
 *
 *  This parser can load a given DDX for a single netcdf@location and then modify it by:
 *
 *  1) Clearing all attributes if <explicit/> tag is given.
 *  2) Adding or modifying existing atomic attributes at any scope (global, variable, nested variable, nested attribute container)
 *  3) Adding (or traversing the scope of existing) attribute containers using <attribute name="foo" type="Structure"> to refer to the container.
 *  4) Traversing a hierarchy of variable scopes to set the scope for 1) or 2)
 *  5) We handle orgName renaming for attribute@orgName only.  This renames an attribute.
 *  6) We can remove an attribute (including container recursively) at any scope with <remove/>
 *
 *  Design and Control Flow:
 *
 *  1) We use the SaxParser interface callbacks to handle calls from a SAX parser which we create on parse() call.
 *
 *  2) We assume a single <netcdf> node with a local data in netcdf@location.  When we parse this node, we use a DDSLoader to
 *  create a new DDS with DDX information (full AttrTable's) in it.
 *
 *  3) We maintain a pointer to the currently active AttrTable as we get other SAX parser calls.  As we enter/exit the lexical scope of
 *  attribute containers or Constructor variables we keep track of this on a scope stack which allows us to know the fully qualified name
 *  of the current scope as well as the type of the innermost scope for error checking.
 *
 *  4) As we process NcML elements we modify the DDS as needed.  The elements are all subclasses of NCMLElement
 *      and are factoried up in onStartElement for polymorphic dispatch.  A stack of these is kept for calling
 *      handleContent() and handleEnd() on them as needed.
 *
 *  5) When complete, we return the loaded and transformed DDS to the caller.
 *
 *  We throw BESInternalError for logic errors in the code such as failed asserts or null pointers.
 *
 *  We throw BESSyntaxUserError for parse errors in the NcML (non-existent variables or attributes, malformed NcML, etc).
 *
 *  Limitations:
 *
 *  o We only handle local (same BES) datasets with this version (hopefully we can relax this to allow remote dataset augmentation
 *      as a bes.conf option or something.
 *
 *  o We can only handle a single <netcdf> node, i.e. we can only augment one location with metadata.  Future versions will allow aggregation, etc.
 *
 *  @author mjohnson <m.johnson@opendap.org>
 */
namespace ncml_module
{

class NCMLParser : public SaxParser
{
public: // Friends
  // We allow the various NCMLElement concrete classes to be friends so we can separate out the functionality
  // into several files, one for each NcML element type.
  friend class AttributeElement;
  friend class ExplicitElement;
  friend class NetcdfElement;
  friend class ReadMetadataElement;
  friend class RemoveElement;
  friend class ValuesElement;
  friend class VariableElement;

public:
  /**
   * @brief Create a structure that can parse an NCML filename and returned a transformed response of requested type.
   *
   * @param loader helper for loading a location referred to in the ncml.
      *
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
   *  @return a new response object with the transformed DDS in it.  The caller assumes ownership of the returned object.
   *  It will be of type BESDDSResponse or BESDataDDSResponse depending on the request being processed.
  */
  auto_ptr<BESDapResponse> parse(const string& ncmlFilename, DDSLoader::ResponseType type);

  /** @brief Same as parse, but the response object to parse into is passed down by the caller
   * rather than created.
   *
   * @param ncmlFilename the ncml file to parse
   * @param responseType the type of response.  Must match response.
   * @param response the premade response object.  The caller owns this memory.
   */
  void parseInto(const string& ncmlFilename, DDSLoader::ResponseType responseType, BESDapResponse* response);

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

  ////////////////////////////////////////////////////////////////////////////////
  // Interface SaxParser:  Wrapped calls from the libxml C SAX parser

  virtual void onStartDocument();
  virtual void onEndDocument();
  virtual void onStartElement(const std::string& name, const AttributeMap& attrs);
  virtual void onEndElement(const std::string& name);
  virtual void onCharacters(const std::string& content);
  virtual void onParseWarning(std::string msg);
  virtual void onParseError(std::string msg);

  ////////////////////////////////////////////////////////////////////////////////
  ///////////////////// PRIVATE INTERFACE

private: // Nested types

  /**
  * Whether <explicit/> or <readMetadata/> was specified.
  * PERFORM_DEFAULT means we have not see either tag.
  * READ_METADATA says we saw <readMetadata/>
  * EXPLICIT means we saw <explicit/>
  * PROCESSED means the directive was already processed for the current location since it happens only once.
  */
  enum SourceMetadataDirective { PERFORM_DEFAULT=0, READ_METADATA, EXPLICIT, PROCESSED };

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

  /** Helper to get the dds from _response
   *  Error to call _response is null.
   */
  DDS* getDDS() const;

  /** @return whether we are handling a DataDDS request (in which case getDDS() is a DataDDS)
   * or not.
   */
  bool parsingDataRequest() const;

  /** Clear any volatile parse state (basically after each netcdf node).
   * Also used by the dtor.
   */
  void resetParseState();

  /** Return the variable with name in the current _pVar container.
   * If null, that means look at the top level DDS.
   * @param name the name of the variable to lookup in the _pVar
   * @return the variable or NULL if not found.
   */
  BaseType* getVariableInCurrentVariableContainer(const string& name);

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

 /** Add a COPY of the given new variable at the current scope of the parser.
  *  If the current scope is not a valid location for a variable,
  *  throw a parse error.
  *
  *  The caller should be sure to delete pNewVar if they no longer need it.
  *
  *  Does NOT change the scope!  The caller must do that if they want it done.
  *
  *  @param pNewVar  the template for a new variable to add at current scope with pNewVar->name().
  *                  pNewVar->ptr_duplicate() will actually be stored, so pNewVar is still owned by caller.
  *
  *  @exception if pNewVar->name() is already taken at current scope.
  *  @exception if the current scope is not valid for adding a variable (e.g. attribute container)
  */
 void addCopyOfVariableAtCurrentScope(BaseType& varTemplate);

 /** Get the current variable container we are in.  If NULL, we are
  * within the top level DDS scope and not a cosntructor variable.
  */
 BaseType* getCurrentVariable() const { return _pVar; }

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

  /** Gets the current attribute table for the scope we are currently at. */
  AttrTable* getCurrentAttrTable() const { return _pCurrentTable; }

  /** Set the current attribute table for the scope */
  void setCurrentAttrTable(AttrTable* table);

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


  /** Do the proper tokenization of values for the given dapAttrTypeName into _tokens
   * using current _separators.
   * Strings and URL types produce a single "token"
   * @return the number of tokens added.
   */
  int tokenizeAttrValues(vector<string>& tokens, const string& values, const string& dapAttrTypeName, const string& separator);

  /**
   *  Tokenize the given string values for the DAP type using the given delimiters currently in _separators and
   *  Result is stored in _tokens and valid until the next call or until we explicitly clear it.
   *  Special Cases: dapType == String and URL will produce just one token, ignoring delimiters.
   *  It is an error to have any values for dapType==Structure, but for this case
   *  we push a single "" onto _tokens.
   *  @return the number of tokens added to _tokens.
   */
  int tokenizeValuesForDAPType(vector<string>& tokens, const string& values, AttrType dapType, const string& separator);

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

  /** Get the current scope (DAP fully qualified name) */
  string getScopeString() const;

  /** Get a typed scope string */
  string getTypedScopeString() const;

  /** Push the element onto the element stack */
  void pushElement(NCMLElement* elt);

  /** Pop the element off the stack and delete it */
  void popElement();

  /** The top of the element stack, the thing we are currently processing */
  NCMLElement* getCurrentElement() const;

  /** Return an immutable view of the element stack.
   * Iterator returns them in order of innermost (top of stack) to outermost */
  typedef std::vector<NCMLElement*>::const_reverse_iterator ElementStackConstIterator;
  ElementStackConstIterator getElementStackBegin() const { return _elementStack.rbegin(); }
  ElementStackConstIterator getElementStackEnd() const { return _elementStack.rend(); }

  /** Delete all the NCMLElement* in _elementStack and clear it. */
  void deleteElementStack();

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

  /**
   * @brief Make sure the given tokens are valid for the listed type.
   * For example, makes sure floats are well formed, UInt16 are unsigned, etc.
   * A successful call will return normally, else we throw an exception.
   *
   * @throw BESUserSyntaxError on invalid values.
   */
   void checkDataIsValidForCanonicalTypeOrThrow(const string& type, const vector<string>& tokens) const;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

  // The type of response in _response
  DDSLoader::ResponseType _responseType;

  // The response object containing the DDS (or DataDDS) for the <netcdf> node we are processing, or null if not processing.
  // Type is based on _responseType.   We do not own this memory!  It is a temp while we parse and is handed in.
  BESDapResponse* _response;

  // what to do with existing metadata after it's read in from parent.
  SourceMetadataDirective _metadataDirective;

  // pointer to currently processed variable, or NULL if none (ie we're at global level).
  BaseType* _pVar;

  // the current attribute table for the scope we are in.  Global table if not within a variable,
  // table for the variable if we are in a variable.
  // Also, if we have a nested attribute table (ie attribute table contains a container) this will be
  // the table for the scope at the current parse point.
  AttrTable* _pCurrentTable;

  // A stack of NcML elements we push as we begin and pop as we end.
  // The memory is owned by this, so we must clear this in dtor and
  // on pop.
  std::vector<NCMLElement*> _elementStack;

  // As we parse, we'll use this as a stack for keeping track of the current
  // scope we're in.  In other words, this stack will refer to the container where _pCurrTable is in the DDS.
  // if empty() then we're in global dataset scope (or no scope if not parsing location yet).
  ScopeStack _scope;
}; // class NCMLParser

} //namespace ncml_module

#endif /* __NCML_MODULE_NCML_PARSER_H__ */

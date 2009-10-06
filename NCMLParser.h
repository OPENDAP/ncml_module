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
#include "NCMLElement.h" // NCMLElement::Factory
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
  class AggregationElement;
  class DDSLoader;
  class DimensionElement;
  class NCMLElement;
  class NetcdfElement;
  class OtherXMLParser;
}

using namespace libdap;
using namespace std;


/**
 *  @brief NcML Parser for adding/modifying/removing metadata (attributes) to existing local datasets using NcML.
 *
 *  Core engine for parsing an NcML structure and modifying the DDS (which includes attribute data)
 *  of a single dataset by adding new metadata to it. (http://docs.opendap.org/index.php/AIS_Using_NcML).
 *
 *  The main documentation for the NCML Module is here: http://docs.opendap.org/index.php/BES_-_Modules_-_NcML_Module
 *
 *  For the purposes of this class "scope" will mean the attribute table at some place in the DDX (populated DDS object), be it at
 *  the global attribute table, inside a nested attribute container, inside a variable, or inside a nested variable (Structure, e.g.).
 *  This is basically the same as a "fully qualified name" in DAP 2 (ESE-RFC-004.1.1)
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
 *  @author mjohnson <m.johnson@opendap.org>
 */
namespace ncml_module
{

class NCMLParser : public SaxParser
{
public: // Friends
  // We allow the various NCMLElement concrete classes to be friends so we can separate out the functionality
  // into several files, one for each NcML element type.
  friend class AggregationElement;
  friend class AttributeElement;
  friend class DimensionElement;
  friend class ExplicitElement;
  friend class NetcdfElement;
  friend class ReadMetadataElement;
  friend class RemoveElement;
  friend class ValuesElement;
  friend class VariableAggElement;
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

  /** Is the innermost scope a <netcdf> node? */
  bool isScopeNetcdf() const;

  /** Is the innermost scope a <aggregation> node? */
  bool isScopeAggregation() const;

  /**  Are we inside the scope of a location element <netcdf> at this point of the parse?
   * Note that this means anywhere in the the scope stack, not the innermost (local) scope
  */
  bool withinNetcdf() const { return _currentDataset != 0; }

  /** Returns whether there is a variable element on the scope stack SOMEWHERE.
   *  Note we could be nested down within multiple variables or attribute containers,
   *  but this will be true if anywhere in current scope we're nested within a variable.
  */
  bool withinVariable() const { return withinNetcdf() && _pVar; }

  DDSLoader& getDDSLoader() const { return _loader; }

  /** @return the currently being parsed dataset (the NetcdfElement itself)
   * or NULL if we're outside all netcdf elements.
   */
  NetcdfElement* getCurrentDataset() const { return _currentDataset; }

  /**
   *  Set the current dataset as dataset.
   *
   *  Sets the current attribute table to be the global attribute table
   *  of dataset.
   *
   *  Does NOT handle
   *  nesting, but is called by pushCurrentDataset and popCurrentDataset.
   *
   *  REQUIRES: dataset && dataset->isValid()
   *  After this call, (getCurrentDataset() == dataset)
   */
  void setCurrentDataset(NetcdfElement* dataset);

  /** @return the root (top level) NetcdfElement, or NULL if we're not that far into a parse yet */
  NetcdfElement* getRootDataset() const { return _rootDataset; }

  /** Helper to get the dds from getCurrentDataset, or null.
   */
  DDS* getDDSForCurrentDataset() const;

  /**
   * "Push" the given dataset as the new current dataset.  If it's the root,
   * then set it up as such with the top level response object.
   * If not, then add it to the current aggregation element, which must exist or exception.
   * Also make sure to maintain the invariant that the current table points to this
   * dataset's table!
   */
  void pushCurrentDataset(NetcdfElement* dataset);

  /**
   * After establishing that dataset isn't root, this is called to
   * prepare its response object and add it to the current aggregation of
   * the current dataset (both of which must exist) as a child.
   *
   * @exception If the current dataset does not have an aggregation
   */
  void addChildDatasetToCurrentDataset(NetcdfElement* dataset);

  /**
   * "Pop" the current dataset.  We pass in a ptr to what we expect it to be
   * for logic testing.  If dataset is NULL, we don't check the logic.
   * If dataset is not null, we throw internal error if it doesn't match the
   * _currentDataset!
   */
  void popCurrentDataset(NetcdfElement* dataset);

  /** @return whether we are handling a DataDDS request (in which case getDDS() is a DataDDS)
   * or not.
   */
  bool parsingDataRequest() const;

  /** Clear any volatile parse state (basically after each netcdf node).
   * Also used by the dtor.
   */
  void resetParseState();

  /** @brief Load the given location into
   * the given response, making sure to use the given responseType.
   */
  void loadLocation(const std::string& location, DDSLoader::ResponseType responseType, BESDapResponse* response);

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

 /** @brief Delete the variable at the current scope, whether the top-level DDS
  * or a variable container.  If the variable to remove is a container (constructor)
  * then also recursively delete all children.
  */
 void deleteVariableAtCurrentScope(const string& name);

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
  *  Here expectedType is assumed to have been through the
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

  /** This clears out ALL the AttrTable's in dds recursively.
    * First it clears the global DDS table, then recurses on all
    * contained variables. */
  void clearAllAttrTables(DDS* dds);

  /** Clear the attribute table for \c var .  If constructor variable, recurse.  */
  void clearVariableMetadataRecursively(BaseType* var);

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

  /** Push the element onto the element stack
   * and ref() it. */
  void pushElement(NCMLElement* elt);

  /** Pop the element off the stack and unref() it
   * It may still exist if other objects maintain strong references to it.
   * */
  void popElement();

  /** The top of the element stack, the thing we are currently processing */
  NCMLElement* getCurrentElement() const;

  /** Return an immutable view of the element stack.
   * Iterator returns them in order of innermost (top of stack) to outermost */
  typedef std::vector<NCMLElement*>::const_reverse_iterator ElementStackConstIterator;
  ElementStackConstIterator getElementStackBegin() const { return _elementStack.rbegin(); }
  ElementStackConstIterator getElementStackEnd() const { return _elementStack.rend(); }

  /** unref() all the NCMLElement* in _elementStack and clear it.
   * We don't delete them, but leave that up to the Factory dtor if there's dangling refs.
   *  */
  void clearElementStack();

  /** Helper call from onStartElement to do the work if we're not in OtherXML parsing state. */
  void processStartNCMLElement(const std::string& name, const AttributeMap& attrs);

  /** Helper call from onEndElement to do the work if we're not in OtherXML parsing state. */
  void processEndNCMLElement(const std::string& name);

  /**
   * @return the first dimension with dimName in the fully enclosed scope from getCurrentDataset() to root or null if not found.
   * */
  const DimensionElement* getDimensionAtLexicalScope(const string& dimName) const;

  /**
   * Print out all the dimensions at current scope and all enclosing scopes from getCurrentDataset() to root.
   * They are printed in order of most specific scope first to root scope last.
   */
  string printAllDimensionsAtLexicalScope() const;

  /**
   * Put the parser into an OtherXML parsing state where it no longer
   * creates NCMLElement's, but just leaves the element on top of the stack
   * while it parses in arbitrary XML until the top level NCMLElement is finally
   * closed, whereupon it goes back into the normal parsing state.
   *
   * It's up to the caller to handle the memory for the pointer and to
   * handle how the parsed data is used.
   *
   * @param pOtherXMLParser  weak pointer (not owned) which acts as the proxy
   *                to receive all parser calls until the element on the stack that created
   *                it is closed.
   */
  void enterOtherXMLParsingState(OtherXMLParser* pOtherXMLParser);
  bool isParsingOtherXML() const;


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

  // Handed in at creation, this is a helper to load a given DDS.  It is assumed valid for the life of this.
  DDSLoader& _loader;

  // The type of response in _response
  DDSLoader::ResponseType _responseType;

  // The response object containing the DDS (or DataDDS) for the root dataset we are processing, or null if not processing.
  // Type is based on _responseType.   We do not own this memory!  It is a temp while we parse and is handed in.
  // NOTE: The root dataset will use this for its response object!
  BESDapResponse* _response;

  // The element factory to use to create our NCMLElement's.
  // All objects created by this factory will be deleted in the dtor
  // regardless of their ref counts!
  NCMLElement::Factory _elementFactory;

  // The root dataset, as a NetcdfElement*.
  NetcdfElement* _rootDataset;

  // The currently being parsed dataset, as a NetcdfElement
  NetcdfElement* _currentDataset;

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

  // If not null, we've temporarily stopped the normal NCML parse and are passing
  // all calls to this proxy until the element on the stack when it was added is
  // closed (and the parser depth is zero!).
  OtherXMLParser* _pOtherXMLParser;

}; // class NCMLParser

} //namespace ncml_module

#endif /* __NCML_MODULE_NCML_PARSER_H__ */

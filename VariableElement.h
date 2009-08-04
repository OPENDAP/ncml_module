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
#ifndef __NCML_MODULE__VARIABLE_ELEMENT_H__
#define __NCML_MODULE__VARIABLE_ELEMENT_H__

#include "NCMLElement.h"

namespace libdap
{
  class BaseType;
}

using namespace std;
namespace ncml_module
{

  /**
   * @brief Concrete class for NcML <variable> element
   *
   * This class handles the processing of <variable> elements
   * in the NcML.
   */
  class VariableElement : public NCMLElement
  {
  public:
    VariableElement();
    VariableElement(const VariableElement& proto);
    virtual ~VariableElement();
    virtual const string& getTypeName() const;
    virtual VariableElement* clone() const; // override clone with more specific subclass
    virtual void setAttributes(const AttributeMap& attrs);
    virtual void handleBegin(NCMLParser& p);
    virtual void handleContent(NCMLParser& p, const string& content);
    virtual void handleEnd(NCMLParser& p);
    virtual string toString() const;

    static const string _sTypeName;

  private:

   /**
    * Handle the beginning of the variable element.
    * If _type=="" we assume we just want to match any type and traverse down.
    * If _type is specified, we assume we want to typecheck.
    * Pushes the variable onto the scope stack.
    * The parser's attribute table becomes this variable's table.
    */
    void processBegin(NCMLParser& p);

    /**
     * Handle exiting the scope of the variable.
     * Pops the current attribute table of the parser up to the
     * parent of this variable's table.
     * The p.getCurrentVariable() will also become the parent of the
     * current variable, or NULL if top-level variable.
     */
    void processEnd(NCMLParser& p);

    /** process this variable as one that already exists at the current parser scope
     * within the current dataset.  Essentially this means making the variable be
     * the new current scope of the parser for subsequent other variable or attribute calls.
     * @param p the parser to effect
     * @param pVar the existing variable in current scope, already looked up.  If null, we look it up from _name.
     */
    void processExistingVariable(NCMLParser& p, libdap::BaseType* pVar);

    /**
     * If _orgName is specified, this function is called from processBegin().
     * If a variable named _orgName exists at current parser scope,
     * the variable is renamed to _name.
     *
     * @param p the parser to effect
     *
     * @exception BESSyntaxUserError if _orgName does not exist at current scope
     * @exception BESSyntaxUserError if _name already DOES exist at current scope
     *
     * Assumes: !_orgName.empty()
     * On exit, the scope of p will be the renamed variable.
     */
    void processRenameVariable(NCMLParser& p);

    /**
     * Called from processBegin() if a variable with _name does NOT exist at
     * current parser scope of p.
     *
     * A new variable of type _type is created at the current parse scope.
     * This new variable will be the new scope of p on exit.
     *
     * If !_shape.empty(), then the created variable is a DAP Array of
     * type _type.  _shape will be tokenized with whitespace separators
     * in order to resolve the dimensions of the Array.  _shape can contain
     * either integers or mnemonic references to previously declared <dimension>'s
     * which must exist at the current parse scope of p.
     */
    void processNewVariable(NCMLParser& p);

    /** @brief Tell the parser to use pVar as the current scope.
     * This also set's the current table to the pVar table.
     * pVar must not be null.*/
    void enterScope(NCMLParser& p, libdap::BaseType* pVar);

    /** Pop off this variable from the scope. */
    void exitScope(NCMLParser& p);

  private:
    string _name;
    string _type;
    string _shape; // empty() => existing var (shape implicit) or if new var, then scalar (rank 0).
    string _orgName; // if !empty(), the name of existing variable we want to rename to _name
  };

}

#endif /* __NCML_MODULE__VARIABLE_ELEMENT_H__ */

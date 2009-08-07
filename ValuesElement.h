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
#ifndef __NCML_MODULE__VALUESELEMENT_H__
#define __NCML_MODULE__VALUESELEMENT_H__

#include "NCMLElement.h"
#include "NCMLDebug.h"
#include <sstream>
#include <string>
#include <vector>

using std::string;

namespace libdap
{
  class BaseType;
}

namespace ncml_module
{
  class NCMLParser;
  class VariableElement;

  class ValuesElement : public NCMLElement
  {
  public:
    static const string _sTypeName;

    ValuesElement();
    ValuesElement(const ValuesElement& proto);
    virtual ~ValuesElement();
    virtual const string& getTypeName() const;
    virtual ValuesElement* clone() const; // override clone with more specific subclass
    virtual void setAttributes(const AttributeMap& attrs);
    virtual void handleBegin(NCMLParser& p);
    virtual void handleContent(NCMLParser& p, const string& content);
    virtual void handleEnd(NCMLParser& p);
    virtual string toString() const;

  private: // Methods

    /** @return true if we have a non-empty start and/or increment.
     * This means that we DO NOT expect non-whitespace content
     * and that we need to auto-generate the values for the given variable!
     */
    bool shouldAutoGenerateValues() const { return ((!_start.empty()) && (!_increment.empty())); }

    /** @brief Name says it all
     *  If _start or _increment cannot be successfully parsed into the type of pVar,
     *  then throw a parse error!
     *  @exception BESSyntaxUserError if _start or _increment are not valid numeric strings for pVar type.
     */
    void validateStartAndIncrementForVariableTypeOrThrow(libdap::BaseType& var) const;

    /** Assumes _tokens has tokenized values, go and set the variable pVar
     * with these values as appropriate for the type.
     *
     * @param p the parser state
     * @param var the variable these values are being set into
     *
     * Note: the number of tokens MUST be valid for the given type or exception!
     * @exception BESSyntaxUserError if the number of values specified does not match
     *                  the dimensionality of the pVar
     */
    void setVariableValuesFromTokens(NCMLParser& p, libdap::BaseType& pVar);

    /** @brief Autogenerate uniform interval numeric values from _start and _increment into variable
     *
     * @param p the parser state to use
     * @param var the variable to set the values for
     */
    void autogenerateAndSetVariableValues(NCMLParser& p, libdap::BaseType& var);

    /** A parameterized set function for all the DAP simple types (Byte, UInt32, etc...) and
     * their ValueType in DAPType::setValue(ValueType val)
     *
     * @typename DAPType: the subclass of simple type of BaseType
     * @typename ValueType: the internal simple type stored in DAPType, the arg type to DAPType::setValue()
     *
     * @param var the simple type var (scalar) of subclass type DAPType
     * @param valueAsToken the unparsed token version of the value.  Will be read using streams.
     */
    template <class DAPType, typename ValueType> void setScalarValue(libdap::BaseType& var, const string& valueAsToken);

    /** Special case for parsing char's instead of bytes. */
    void parseAndSetCharValue(libdap::BaseType& var, const string& valueAsToken);

    /**
     * Figure out the NcML type of <variable> element we are within by walking up the parse stack of p
     */
    std::string getNCMLTypeForVariable(NCMLParser& p) const;

    /** Get the VariableElement we are contained within in the parse */
    const VariableElement* getContainingVariableElement(NCMLParser& p) const;

  private: // Data Rep
    string _start;
    string _increment;
    string _separator; // defaults to whitespace

    // If we got handleContent successfully!
    bool _gotContent;

    // Temp to tokenize the content on handleContent()
    std::vector<string> _tokens;
  };

}

#endif /* __NCML_MODULE__VALUESELEMENT_H__ */

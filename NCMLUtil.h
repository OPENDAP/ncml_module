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
#ifndef __NCML_MODULE_NCML_UTIL_H__
#define __NCML_MODULE_NCML_UTIL_H__

namespace libdap
{
  // FDecls
  class BaseType;
  class DDS;
  class DAS;
}

class BESDapResponse;

#include "NCMLCommonTypes.h"
#include <string>
#include <vector>

namespace ncml_module
{
  /**
   *  Static class of utility functions
   */
  class NCMLUtil
  {
    NCMLUtil() {}
  public:
    ~NCMLUtil() {}

    /** Delimiter set for tokenizing whitespace separated data.  Currently " \t" */
    static const std::string WHITESPACE;

    /**
     * Split str into tokens using the characters in delimiters as split boundaries.
     * Return the number of tokens appended to tokens.
     */
    static int tokenize(const std::string& str,
        std::vector<std::string>& tokens,
        const std::string& delimiters = " \t");

    /** Split str into a vector with one char in str per token slot. */
    static int tokenizeChars(const std::string& str, std::vector<std::string>& tokens);

    /** Does the string contain only ASCII 7-bit characters
     * according to isascii()?
     */
    static bool isAscii(const std::string& str);

    /** Is all the string whitespace as defined by chars in WHITESPACE ?  */
    static bool isAllWhitespace(const std::string& str);


    /** @brief Return the value of the given attribute from the map, else the given default.
     * @param map  map to search
     * @param name name of the attribute to find
     * @param default  what to return if name not found
     * @return the attribute value or default as a const reference.
     */
    static const std::string& findAttrValue(const AttributeMap& map, const std::string& name, const std::string& def="");


     /** Given we have a valid attribute tree inside of the DDS, recreate it in the DAS.
          @param das the das to clear and populate
          @param dds_const the source dds
      */
    static void populateDASFromDDS(libdap::DAS* das, const libdap::DDS& dds_const);

    /** Make a deep copy of the global attributes and variables within dds_in
     * into *dds_out.
     * Doesn't affect any other top-level data.
     * @param dds_out place to copy global attribute table and variables into
     * @param dds_in source DDS
     */
    static void copyVariablesAndAttributesInto(libdap::DDS* dds_out, const libdap::DDS& dds_in);

    /**
     * Return the DDS* for the given response object. It is assumed to be either a
     * BESDDSResponse or BESDataDDSResponse.
     * @param reponse either a  BESDDSResponse or BESDataDDSResponse to extract DDS from.
     * @return the DDS* contained in the response object, or NULL if incorrect response type.
     */
    static libdap::DDS* getDDSFromEitherResponse(BESDapResponse* response);

    /** Currently BaseType::set_name only sets in BaseType.  Unfortunately, the DDS transmission
     * for Vector subclasses uses the name of the template BaseType* describing the variable,
     * which is not set by set_name.  This is a workaround until Vector overrides BaseType::set_name
     * to also set the name of the template _var if there is one.
     */
    static void setVariableNameProperly(libdap::BaseType* pVar, const std::string& name);
  };
}

#endif /* __NCML_MODULE_NCML_UTIL_H__ */

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
#ifndef NCML_MODULE_UTIL_H_
#define NCML_MODULE_UTIL_H_

namespace libdap
{
  // FDecls
   class DDS;
   class DAS;
}

#include <string>
#include <vector>

namespace ncml_module
{
  class NcmlUtil
  {
  public:
    NcmlUtil() {}
    ~NcmlUtil() {}

    /** Delimiter set for tokenizing whitespace separated data.  Currently " \t" */
    static const std::string WHITESPACE;

    /**
     * Split str into tokens using the characters in delimiters as split boundaries.
     * Return the number of tokens appended to tokens.
     */
    static int tokenize(const std::string& str,
        std::vector<std::string>& tokens,
        const std::string& delimiters = " \t");


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
  };
}

#endif /* NCML_MODULE_UTIL_H_ */

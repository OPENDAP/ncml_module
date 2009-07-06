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
#include "NcmlUtil.h"
#include "BESDebug.h"
#include "BESInternalError.h"
#include "DAS.h"
#include "DDS.h"

using namespace libdap;
using namespace std;

namespace ncml_module
{

  const std::string NcmlUtil::WHITESPACE = " \t";

  int
  NcmlUtil::tokenize(const string& str,
                 vector<string>& tokens,
                 const string& delimiters)
  {
      // Skip delimiters at beginning.
      string::size_type lastPos = str.find_first_not_of(delimiters, 0);
      // Find first "non-delimiter".
      string::size_type pos     = str.find_first_of(delimiters, lastPos);

      int count=0; // how many we added.
      while (string::npos != pos || string::npos != lastPos)
      {
          // Found a token, add it to the vector.
          tokens.push_back(str.substr(lastPos, pos - lastPos));
          count++;
          // Skip delimiters.  Note the "not_of"
          lastPos = str.find_first_not_of(delimiters, pos);
          // Find next "non-delimiter"
          pos = str.find_first_of(delimiters, lastPos);
      }
      return count;
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
             if (!child)
               {
                 throw BESInternalError("Type cast error", __FILE__, __LINE__);
               }

             BESDEBUG("ncml", "Var " << child->name() << " is composite, so recursively adding attribute tables" << endl);
             populateAttrTableForContainerVariableRecursive(newTable, child);
           }
      }
  }


  // This is basically the opposite of transfer_attributes.
  void
  NcmlUtil::populateDASFromDDS(DAS* das, const DDS& dds_const)
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
        throw BESInternalError("Unexpected Container Error creating DAS from DDS in NCMLHandler", __FILE__, __LINE__);
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
            if (!consVar)
              {
                throw BESInternalError("Type cast error", __FILE__, __LINE__);
              }

            populateAttrTableForContainerVariableRecursive(clonedVarTable, consVar);
          }
      }
  }

  // This function was added since DDS::operator= had some bugs we need to fix.
  // At that point, we can just use that function, probably.
  void
  NcmlUtil::copyVariablesAndAttributesInto(DDS* dds_out, const DDS& dds_in)
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

}

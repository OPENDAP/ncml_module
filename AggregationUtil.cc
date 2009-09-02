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
#include "AggregationUtil.h"
#include <AttrTable.h>
#include <BaseType.h>
#include <DDS.h>
#include "NCMLDebug.h" // This the ONLY dependency on NCML Module I want in this class since the macros there are general it's ok...

using std::string;
using std::vector;
using libdap::AttrTable;
using libdap::BaseType;
using libdap::DDS;

namespace ncml_module
{
  void
  AggregationUtil::performUnionAggregation(DDS* pOutputUnion, const vector<DDS*>& datasetsInOrder)
  {
    VALID_PTR(pOutputUnion);
    vector<DDS*>::const_iterator endIt = datasetsInOrder.end();
    vector<DDS*>::const_iterator it;
    for (it = datasetsInOrder.begin(); it != endIt; ++it)
      {
        DDS* pDDS = *it;
        VALID_PTR(pDDS);

        // Merge in the global attribute tables
        unionAttrTablesInto(&(pOutputUnion->get_attr_table()), pDDS->get_attr_table());

        // Merge in the variables, which carry their tables along with them since the AttrTable is
        // within the variable's "namespace", or lexical scope.
        unionAllVariablesInto(pOutputUnion, *pDDS);
      }
  }

  void
  AggregationUtil::unionAttrTablesInto(AttrTable* pOut, const AttrTable& fromTableIn)
  {
    // semantically const
    AttrTable& fromTable = const_cast<AttrTable&>(fromTableIn);
    AttrTable::Attr_iter endIt = fromTable.attr_end();
    AttrTable::Attr_iter it;
    for (it = fromTable.attr_begin(); it != endIt; ++it)
      {
        const string& name = fromTable.get_name(it);
        AttrTable::Attr_iter attrInOut;
        bool foundIt = findAttribute(*pOut, name, attrInOut);
        // If it's already in the output, then skip it
        if (foundIt)
          {
            BESDEBUG("ncml", "Union of AttrTable: an attribute named " << name << " already exist in output, skipping it..." << endl);
            continue;
          }
        else // put a copy of it into the output
          {
             // containers need deep copy
             if (fromTable.is_container(it))
               {
                 AttrTable* pOrigAttrContainer = fromTable.get_attr_table(it);
                 NCML_ASSERT_MSG(pOrigAttrContainer,
                     "AggregationUtil::mergeAttrTables(): expected non-null AttrTable for the attribute container: " + name);
                 AttrTable* pClonedAttrContainer = new AttrTable(*pOrigAttrContainer);
                 VALID_PTR(pClonedAttrContainer);
                 pOut->append_container(pClonedAttrContainer, name);
                 BESDEBUG("ncml", "Union of AttrTable: adding a deep copy of attribute=" << name << " to the merged output." << endl);
               }
             else // for a simple type
               {
                 string type = fromTable.get_type(it);
                 vector<string>* pAttrTokens = fromTable.get_attr_vector(it);
                 VALID_PTR(pAttrTokens);
                 // append_attr makes a copy of the vector, so we don't have to do so here.
                 pOut->append_attr(name, type, pAttrTokens);
               }
          }
      }
  }

  bool
  AggregationUtil::findAttribute(const AttrTable& inTable, const string& name, AttrTable::Attr_iter& attr)
  {
    AttrTable& inTableSemanticConst = const_cast<AttrTable&>(inTable);
    AttrTable* pContainer = 0;
    inTableSemanticConst.find(name, &pContainer, &attr);
    return (pContainer != 0);
  }

  void
  AggregationUtil::unionAllVariablesInto(libdap::DDS* pOutputUnion, const std::vector<libdap::DDS*>& datasetsInOrder)
  {
    vector<DDS*>::const_iterator endIt = datasetsInOrder.end();
    vector<DDS*>::const_iterator it;
    for (it = datasetsInOrder.begin(); it != endIt; ++it)
      {
        unionAllVariablesInto(pOutputUnion, *(*it) );
      }
  }

  void
  AggregationUtil::unionAllVariablesInto(libdap::DDS* pOutputUnion, const libdap::DDS& fromDDS)
  {
    DDS& dds = const_cast<DDS&>(fromDDS); // semantically const
    DDS::Vars_iter endIt = dds.var_end();
    DDS::Vars_iter it;
    for (it = dds.var_begin(); it != endIt; ++it)
      {
        BaseType* var = *it;
        if (var)
          {
            bool addedVar = addCopyOfVariableIfNameIsAvailable(pOutputUnion, *var);
            if (addedVar)
              {
                BESDEBUG("ncml", "Variable name=" << var->name() << " wasn't in the union yet and was added." << endl);
              }
            else
              {
                BESDEBUG("ncml", "Variable name=" << var->name() << " was already in the union and was skipped." << endl);
              }
          }
      }
  }

  bool
  AggregationUtil::addCopyOfVariableIfNameIsAvailable(libdap::DDS* pOutputUnion, const libdap::BaseType& var)
  {
    bool ret = false;
    BaseType* existingVar = findVariableAtDDSTopLevel(*pOutputUnion, var.name());
    if (!existingVar)
      {
        // Add the var.   add_var does a clone, so we don't need to.
        pOutputUnion->add_var(const_cast<BaseType*>(&var));
        ret = true;
      }
    return ret;
  }

  libdap::BaseType*
  AggregationUtil::findVariableAtDDSTopLevel(const libdap::DDS& dds_const, const string& name)
  {
    BaseType* ret = 0;
    DDS& dds = const_cast<DDS&>(dds_const); // semantically const
    DDS::Vars_iter endIt = dds.var_end();
    DDS::Vars_iter it;
    for (it = dds.var_begin(); it != endIt; ++it)
      {
        BaseType* var = *it;
        if (var && var->name() == name)
          {
            ret = var;
            break;
          }
      }
    return ret;
  }

}

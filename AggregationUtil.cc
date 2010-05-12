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

// agg_util includes
#include "Dimension.h"

// libdap includes
#include <Array.h> // libdap
#include <AttrTable.h>
#include <BaseType.h>
#include <DDS.h>

// Outside includes (MINIMIZE THESE!)
#include "NCMLDebug.h" // This the ONLY dependency on NCML Module I want in this class since the macros there are general it's ok...


using std::string;
using std::vector;
using libdap::Array;
using libdap::AttrTable;
using libdap::BaseType;
using libdap::DDS;
using libdap::Vector;

namespace agg_util
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
     attr = inTableSemanticConst.simple_find(name);
    return (attr != inTableSemanticConst.attr_end());
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

  template <class LibdapType>
  LibdapType*
  AggregationUtil::findTypedVariableAtDDSTopLevel(const libdap::DDS& dds, const string& name)
  {
    BaseType* pBT = findVariableAtDDSTopLevel(dds, name);
    if (pBT)
      {
        return dynamic_cast<LibdapType*>(pBT);
      }
    else
      {
        return 0;
      }
  }

  void
  AggregationUtil::produceOuterDimensionJoinedArray(
            Array* pJoinedArray,
            const std::string& joinedArrayName,
            const std::string& newOuterDimName,
            const std::vector<libdap::Array*>& fromVars,
            bool copyData)
  {
    string funcName = "AggregationUtil::createOuterDimensionJoinedArray:";

    NCML_ASSERT_MSG(fromVars.size() > 0,
        funcName +  "Must be at least one Array in input!");

    // uses the first one as template for type and shape
    if (!validateArrayTypesAndShapesMatch(fromVars, true))
      {
        THROW_NCML_PARSE_ERROR(-1, // Unspecified parse line for this case...
            funcName + " The input arrays must all have the same data type and dimensions but do not!");
      }

    // The first will be used to "set up" the pJoinedArray
    Array* templateArray = fromVars[0];
    VALID_PTR(templateArray);
    BaseType* templateVar = templateArray->var();
    NCML_ASSERT_MSG(templateVar,
        funcName + "Expected a non-NULL prototype BaseType in the first Array!");

    // Set the template var for the type.
    pJoinedArray->add_var(templateVar);
    // and force the name to be the desired one, not the prototype's
    pJoinedArray->set_name(joinedArrayName);

    // Copy the attribute table from the template over...  We're not merging or anything.
    pJoinedArray->set_attr_table(templateArray->get_attr_table());

    // Create a new outer dimension based on the number of inputs we have.
    // These append_dim calls go left to right, so we need to add the new dim FIRST.
    pJoinedArray->append_dim(fromVars.size(), newOuterDimName);

    // Use the template to add inner dimensions to the new array
    for (Array::Dim_iter it = templateArray->dim_begin(); it != templateArray->dim_end(); ++it)
      {
        int dimSize = templateArray->dimension_size(it);
        string dimName = templateArray->dimension_name(it);
        pJoinedArray->append_dim(dimSize, dimName);
      }

    if (copyData)
      {
        // Make sure we have capacity for the full length of the up-ranked shape.
        pJoinedArray->reserve_value_capacity(pJoinedArray->length());
        // Glom the data together in
        joinArrayData(pJoinedArray, fromVars,
              false, // we already reserved the space
              true); // but please clear the Vector buffers after you use each Array in fromVars to help on memory.
      }
  }

  bool
  AggregationUtil::validateArrayTypesAndShapesMatch(
       const std::vector<libdap::Array*>& arrays,
       bool enforceMatchingDimNames)
  {
    NCML_ASSERT(arrays.size() > 0);
    bool valid = true;
    Array* pTemplate = 0;
    for (vector<Array*>::const_iterator it = arrays.begin(); it != arrays.end(); ++it)
      {
        // Set the template from the first one.
        if (!pTemplate)
          {
            pTemplate = *it;
            VALID_PTR(pTemplate);
            continue;
          }

        valid = ( valid &&
                  doTypesMatch(*pTemplate, **it) &&
                  doShapesMatch(*pTemplate, **it, enforceMatchingDimNames)
                );
        // No use continuing
        if (!valid)
          {
            break;
          }
      }
    return valid;
  }

  bool
  AggregationUtil::doTypesMatch(const libdap::Array& lhsC, const libdap::Array& rhsC)
  {
    // semantically const
    Array& lhs = const_cast<Array&>(lhsC);
    Array& rhs = const_cast<Array&>(rhsC);
    return (lhs.var() && rhs.var() && lhs.var()->type() == rhs.var()->type());
  }

  bool
  AggregationUtil::doShapesMatch(const libdap::Array& lhsC, const libdap::Array& rhsC, bool checkDimNames)
  {
    // semantically const
    Array& lhs = const_cast<Array&>(lhsC);
    Array& rhs = const_cast<Array&>(rhsC);

    // Check the number of dims matches first.
    bool valid = true;
    if (lhs.dimensions() != rhs.dimensions())
      {
         valid = false;
      }
    else
      {
        // Then iterate on both in sync and compare.
        Array::Dim_iter rhsIt = rhs.dim_begin();
        for (Array::Dim_iter lhsIt = lhs.dim_begin(); lhsIt != lhs.dim_end(); (++lhsIt, ++rhsIt) )
          {
            valid = (valid && ( lhs.dimension_size(lhsIt) == rhs.dimension_size(rhsIt) ));

            if (checkDimNames)
              {
                valid = (valid &&  ( lhs.dimension_name(lhsIt) == rhs.dimension_name(rhsIt) ));
              }
          }
      }
    return valid;
  }

  unsigned int
  AggregationUtil::collectVariableArraysInOrder(std::vector<Array*>& varArrays, const std::string& collectVarName, const vector<DDS*>& datasetsInOrder)
  {
    unsigned int count = 0;
    vector<DDS*>::const_iterator endIt = datasetsInOrder.end();
    vector<DDS*>::const_iterator it;
    for (it = datasetsInOrder.begin(); it != endIt; ++it)
      {
        DDS* pDDS = const_cast<DDS*>(*it);
        VALID_PTR(pDDS);
        Array* pVar = dynamic_cast<Array*>(findVariableAtDDSTopLevel(*pDDS, collectVarName));
        if (pVar)
          {
            varArrays.push_back(pVar);
            count++;
          }
      }
    return count;
  }

  bool
  AggregationUtil::couldBeCoordinateVariable(BaseType* pBT)
  {
    Array* pArr = dynamic_cast<Array*>(pBT);
    if (pArr &&
        (pArr->dimensions() == 1))
      {
        // only one dimension, so grab the first and make sure we only got one.
        Array::Dim_iter it = pArr->dim_begin();
        bool matches = (pArr->dimension_name(it) == pArr->name());
        NCML_ASSERT_MSG( (++it == pArr->dim_end()),
              "Logic error: NCMLUtil::isCoordinateVariable(): expected one dimension from Array, but got more!");
        return matches;
      }
    else
      {
        return false;
      }
  }

  void
  AggregationUtil::joinArrayData(Array* pAggArray,
       const std::vector<Array*>& varArrays,
       bool reserveStorage/*=true*/,
       bool clearDataAfterUse/*=false*/)
  {
    // Make sure we get a pAggArray with a type var we can deal with.
    VALID_PTR(pAggArray->var());
    NCML_ASSERT_MSG(pAggArray->var()->is_simple_type(),
        "AggregationUtil::joinArrayData: the output Array is not of a simple type!  Can't aggregate!");

    // If the caller wants us to do it, sum up length() and reserve that much.
    if (reserveStorage)
      {
        // Figure it how much we need...
        unsigned int totalLength = 0;
        {
          vector<Array*>::const_iterator it;
          vector<Array*>::const_iterator endIt = varArrays.end();
          for (it = varArrays.begin(); it != endIt; ++it)
            {
              Array* pArr = *it;
              if (pArr)
                {
                  totalLength += pArr->length();
                }
            }
        }
        pAggArray->reserve_value_capacity(totalLength);
      }

    // For each Array, make sure it's read in and copy its data into the output.
    unsigned int nextElt = 0;  // keeps track of where we are to write next in the output
    vector<Array*>::const_iterator it;
    vector<Array*>::const_iterator endIt = varArrays.end();
    for (it = varArrays.begin(); it != endIt; ++it)
      {
        Array* pArr = *it;
        VALID_PTR(pArr);
        NCML_ASSERT_MSG(pArr->var() && (pArr->var()->type() == pAggArray->var()->type()),
            "AggregationUtil::joinArrayData: one of the arrays to join has different type than output!  Can't aggregate!");

        // Make sure it was read in...
        if (!pArr->read_p())
          {
            pArr->read();
          }

        // Copy it in with the Vector call and update our location
        nextElt += pAggArray->set_value_slice_from_row_major_vector(*pArr, nextElt);

        if (clearDataAfterUse)
          {
            pArr->clear_local_data();
          }
      }

    // That's all folks!
  }

  void
  AggregationUtil::printConstraints(std::ostream& os, const Array& rcArray)
  {
//    struct dimension
//        {
//            int size;  ///< The unconstrained dimension size.
//            string name;    ///< The name of this dimension.
//            int start;  ///< The constraint start index
//            int stop;  ///< The constraint end index
//            int stride;  ///< The constraint stride
//            int c_size;  ///< Size of dimension once constrained
//        };
    os << "Array constraints: " << endl;
    Array& theArray = const_cast<Array&>(rcArray);
    Array::Dim_iter it;
    Array::Dim_iter endIt = theArray.dim_end();
    for (it = theArray.dim_begin(); it != endIt; ++it)
      {
        Array::dimension d = *it;
        os << "Dim = {" << endl;
        os << "name=" << d.name << endl;
        os << "start=" << d.start << endl;
        os << "stride=" << d.stride << endl;
        os << "stop=" << d.stop << endl;
        os << " }" << endl;
      }
     os << "End Array constraints" << endl;
  }

  void
  AggregationUtil::printConstraintsToDebugChannel(const std::string& debugChannel, const libdap::Array& fromArray)
  {
	  ostringstream oss;
	  BESDEBUG(debugChannel, "Printing onstraints for Array: " << fromArray.name() << ": " << oss.str() << endl);
	  AggregationUtil::printConstraints(oss, fromArray);
  }

  void
  AggregationUtil::transferArrayConstraints(Array* pToArray, const Array& fromArrayConst, bool skipFirstDim,
		  bool printDebug /* = false */,
		  const std::string& debugChannel /* = "agg_util" */)
  {
    VALID_PTR(pToArray);
    Array& fromArray = const_cast<Array&>(fromArrayConst);

    // Make sure there's no constraints on output.  Shouldn't be, but...
    pToArray->reset_constraint();

    if (printDebug)
      {
        BESDEBUG(debugChannel, "Printing constraints on fromArray name= " << fromArray.name() <<
            " before transfer..." << endl);
        printConstraintsToDebugChannel(debugChannel, fromArray);
      }

    // Only real way to the constraints is with the iterator,
    // so we'll iterator on the fromArray and move
    // to toarray iterator in sync.
    Array::Dim_iter fromArrIt = fromArray.dim_begin();
    Array::Dim_iter fromArrEndIt = fromArray.dim_end();
    Array::Dim_iter toArrIt = pToArray->dim_begin();
    for (;
        fromArrIt != fromArrEndIt;
        ++fromArrIt)
      {
        if (skipFirstDim && (fromArrIt == fromArray.dim_begin()) )
          {
            continue;
          }

        NCML_ASSERT_MSG(fromArrIt->name == toArrIt->name,
            "GAggregationUtil::transferArrayConstraints: "
            "Expected the dimensions to have the same name but they did not.");
        pToArray->add_constraint(
            toArrIt,
            fromArrIt->start,
            fromArrIt->stride,
            fromArrIt->stop );
        ++toArrIt;
      }

    if (printDebug)
      {
        BESDEBUG(debugChannel, "Printing constrains on pToArray after transfer..." << endl);
        printConstraintsToDebugChannel(debugChannel, *pToArray);
      }
  }


}

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
#ifndef __AGG_UTIL__AGGREGATION_UTIL_H__
#define __AGG_UTIL__AGGREGATION_UTIL_H__

#include <AttrTable.h>
#include <iostream>
#include <string>
#include <vector>

namespace libdap
{
  class Array;
  class BaseType;
  class DDS;
};

namespace agg_util
{
  class Dimension;
};

namespace agg_util
{
  /**
   *   A static class for encapsulating the aggregation functionality on libdap.
   *   This class should have references to libdap and STL, but should NOT
   *   contain any references to other ncml_module classes.  This will allow us to
   *   potentially package the aggregation functionality into its own lib
   *   or perhaps roll it into libdap.
   */
  class AggregationUtil
  {
  private: // This is a static class for now...
    AggregationUtil() {}
    ~AggregationUtil() {}

  public:

    // Typedefs
    typedef std::vector<const libdap::DDS*> ConstDDSList;

    /**
     * Perform an NCML-type union aggregation on the datasets in datasetsInOrder into pOutputUnion.
     * The first named instance of a variable or attribute in a forward iteration of datasetsInOrder
     * ends up in pOutputUnion.  Not that named containers are considered as a unit, so we do not recurse into
     * their children.
     */
    static void performUnionAggregation(
        libdap::DDS* pOutputUnion,
        const ConstDDSList& datasetsInOrder);

    /**
     * Merge any attributes in tableToMerge whose names do not already exist within *pOut into pOut.
     * @param pOut the table to merge into.  On exit it will contain its previous contents plus any new attributes
     *        from fromTable
     * @param fromTable
     */
    static void unionAttrTablesInto(libdap::AttrTable* pOut, const libdap::AttrTable& fromTable);

    /**
     *  Lookup the attribute with given name in inTable and place a reference in attr.
     *  @return whether it was found and attr is validly effected.
     * */
    static bool findAttribute(const libdap::AttrTable& inTable, const string& name, libdap::AttrTable::Attr_iter& attr);

    /**
     * For each variable within the top level of each dataset in datasetsInOrder (forward iteration)
     * add a _clone_ of it to pOutputUnion if a variable with the same name does not exist in
     * pOutputUnion yet.
     */
    static void unionAllVariablesInto(libdap::DDS* pOutputUnion, const ConstDDSList& datasetsInOrder);

    /**
     * For each variable in fromDDS top level, union it into pOutputUnion if a variable with the same name isn't already there
     * @see addCopyOfVariableIfNameIsAvailable().
     */
    static void unionAllVariablesInto(libdap::DDS* pOutputUnion, const libdap::DDS& fromDDS);

    /**
     * If a variable does not exist within pOutputUnion (top level) with the same name as var,
     * then place a clone of var into pOutputUnion.
     *
     * @return whether pOutputUnion changed with the addition of var.clone().
     */
    static bool addCopyOfVariableIfNameIsAvailable(libdap::DDS* pOutputUnion, const libdap::BaseType& var);

    /**
     * Find a variable with name at the top level of the DDS.
     * Do not recurse.  DDS::var() will look inside containers, so we can't use that.
     * @return the variable within dds or null if not found.
     */
    static libdap::BaseType* findVariableAtDDSTopLevel(const libdap::DDS& dds, const string& name);

    /**
     * Template wrapper for findVariableAtDDSTopLevel() which
     * does the find but only return non-NULL if the found
     * BaseType* can be dynamically cast to template type LibdapType.
     * @param dds the dds to search
     * @param name the name of the variable to find
     * @return the pointer to the found object if it's dynamically castable
     *         to LibdapType*, else NULL.
     * @see findVariableAtDDSTopLevel()
     */
    template <class LibdapType> static LibdapType* findTypedVariableAtDDSTopLevel(const libdap::DDS& dds, const string& name);

    /**
     *  Basic joinNew aggregation into pJoinedArray on the array of inputs fromVars.
     *
     *  If all the Arrays in fromVars are rank D, then the joined Array will be rank D+1
     *  with the new outer dimension with dimName which will be of cardinality fromVars.size().
     *  The row major order of the data in the new Array will be assumed in the order the fromVars are given.
     *
     *  Data values will ONLY be copied if copyData is set.
     *
     *  fromVars[0] will be used as the "prototype" in terms of attribute metadata, so only its Attributes
     *  will be copied into the resulting new Array.
     *
     *  Errors:
     *  The shape and type of all vars in fromVars MUST match each other or an exception is thrown!
     *  There must be at least one array in fromVars or an exception is thrown.
     *
     * @param pJoinedArray      the array to make into the new array.  It is assumed to have no dimensions
     *                          to start and should be an empty prototype.
     *
     * @param joinedArrayName   the name of pJoinedArray will be set to this
     * @param newOuterDimName   the name of the new outer dimension will be set to this
     * @param fromVars          the input Array's the first of which will be the "template"
     * @param copyData          whether to copy the data from the Array's in fromVars or just create the shape.
     */
    static void produceOuterDimensionJoinedArray(libdap::Array* pJoinedArray,
          const std::string& joinedArrayName,
          const std::string& newOuterDimName,
          const std::vector<libdap::Array*>& fromVars,
          bool copyData);

    /**
     * Scan all the arrays in _arrays_ using the first as a template
     * and make sure that they all have the same data type and they all
     * have the same dimensions.  (NOTE: we only use the sizes to
     * validate dimensions, not the "name", unless enforceMatchingDimNames is set)
     */
    static bool validateArrayTypesAndShapesMatch(const std::vector<libdap::Array*>& arrays,
        bool enforceMatchingDimNames);

    /** Do the lhs and rhs have the same data type? */
    static bool doTypesMatch(const libdap::Array& lhs, const libdap::Array& rhs);

    /** Do the lhs and rhs have the same shapes?  Only use size for dimension compares unless
     * checkDimNames
     */
    static bool doShapesMatch(const libdap::Array& lhs, const libdap::Array& rhs, bool checkDimNames);

    /**
     * Fill in varArrays with Array's named collectVarName from each DDS in datasetsInOrder, in that order.
     * In other words, the ordering found in datasetsInOrder will be preserved in varArrays.
     * @return the number of variables added to varArrays.
     * @param varArrays  the array to push the found variables onto.  Will NOT be cleared, so can be added to.
     * @param collectVarName  the name of the variable to find at top level DDS in datasetsInOrder
     * @param datasetsInOrder the datasets to search for the Array's within.
     */
    static unsigned int collectVariableArraysInOrder(
          std::vector<libdap::Array*>& varArrays,
          const std::string& collectVarName,
          const ConstDDSList& datasetsInOrder);

    /**
      * @return whether this is a 1D Array whose dimension name is the same as its variable name.
      * We consider this to define a "coordinate variable" in the sense of an NCML dataset
      * and will use it as a Grid map vector.
    */
    static bool couldBeCoordinateVariable(libdap::BaseType* pBT);

    /**
     * Copy the simple type data Vector for each Array in varArrays into pAggArray, sequentially,
     * effectively appending all the row major data in each entry in varArray into the row major order
     * of pAggArray.
     *
     * If the data in varArray's has not been read, it calls read() on each Array first.

     * If reserveStorage is set, pAggArray will first have enough its capacity reserved to store all the data within varArray's
     * (the sum of all lengths).  This should be false if the caller already reserved the proper capacity.
     *
     * @param pAggArray the output to place the appended data
     * @param varArrays the Array's whose data is to be copied into pAggArray, in the order of the vector.
     * @param reserveStorage  if true, sets the capacity of pAggArray to be enough to contain all the elements in varArrays.
     *                        Note: this might not match the length of the ouput, so the caller really should do this!
     * @param clearDayaAfterUse  if true, Vector::clear_local_data() will be called on each member of varArray's after it is copied into the output.
     *                           this should tighten up memory if its known the data will no longer be needed.
     *
     * @exception if any Array in varArray's does not have the same element type as pAggArray.
     * @exception if there is not enough storage in pAggArray to collect all the data.
     * @exception on any problems will a read() call on any Array in varArrays.
     */
    static void joinArrayData(libdap::Array* pAggArray,
        const std::vector<libdap::Array*>& varArrays,
        bool reserveStorage=true,
        bool clearDataAfterUse=false);

    /**
     * Stream out the current constraints for all the dimensions in the Array.
     * @param os  the output stream
     * @param fromArray the array whose dimensions to print.
     */
    static void printConstraints(std::ostream& os, const libdap::Array& fromArray);

    /** Output using BESDEBUG to the debugChannel channel.
     * Prints the constraints on the dimensions of fromArray.
     * @param debugChannel name of the output channel
     * @param fromArray  the Array whose constraints should be printed to the debugChannel
     */
    static void printConstraintsToDebugChannel(const std::string& debugChannel, const libdap::Array& fromArray);

    /**
        * Copy the constraints from the from Array into the pToArray
        * in Dim_iter order.
        * if skipFirstDim, the first dimension of fromArray will be skipped,
        * for the case of copying from the aggregated array to a subset array.
        * @param pToArray array to put constraints into
        * @param fromArray array to take constraints from
        * @param skipFirstDim whether the first dim of fromArray is aggregated and
        *                  should be skipped.
        */
    static void transferArrayConstraints(
    		libdap::Array* pToArray,
    		const libdap::Array& fromArray,
    		bool skipFirstDim,
    		bool printDebug = false,
    		const std::string& debugChannel = "agg_util");

  };


  /**
   * For each ptr element pElt in vecToClear, delete pElt and remove it
   * from vecToClear.
   * On exit, vecToClear.empty() and all ptrs it used to contain have been delete'd.
   * @param vecToClear
   */
  template <typename T>
  void clearVectorAndDeletePointers(std::vector<T*>& vecToClear)
  {
    while (!vecToClear.empty())
      {
        T* pElt = vecToClear.back();
        delete pElt;
        vecToClear.pop_back();
      }
  }

  /** Assumes T has unref() */
  template <class T>
  void clearAndUnrefAllElements(std::vector<T*>& vecToClear)
   {
     while (!vecToClear.empty())
       {
         T* pElt = vecToClear.back();
         pElt->unref();
         vecToClear.pop_back();
       }
   }

  /** Assumes T has ref() */
  template <class T>
  void appendVectorOfRCObject(std::vector<T*>& intoVec,
       const std::vector<T*>& fromVec)
  {
    class std::vector<T*>::const_iterator it;
    class std::vector<T*>::const_iterator endIt = fromVec.end();
    for (it = fromVec.begin(); it != endIt; ++it)
       {
         T* pElt = *it;
         if (pElt)
           {
             pElt->ref();
           }
         intoVec.push_back(pElt);
       }
   }

}

#endif /* __AGG_UTIL__AGGREGATION_UTIL_H__ */

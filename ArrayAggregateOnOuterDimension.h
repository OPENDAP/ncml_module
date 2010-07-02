//////////////////////////////////////////////////////////////////////////////
// This file is part of the "NcML Module" project, a BES module designed
// to allow NcML files to be used to be used as a wrapper to add
// AIS to existing datasets of any format.
//
// Copyright (c) 2010 OPeNDAP, Inc.
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

#ifndef __AGG_UTIL__ARRAY_AGGREGATE_ON_OUTER_DIMENSION_H__
#define __AGG_UTIL__ARRAY_AGGREGATE_ON_OUTER_DIMENSION_H__

#include "AggregationUtil.h" // agg_util
#include "AggMemberDataset.h" // agg_util
#include <Array.h>  // libap::Array
#include "Dimension.h" // agg_util
#include "DDSLoader.h" // agg_util
#include <memory>
#include <string>
#include <vector>

using std::string;
using std::vector;
using libdap::Array;

namespace agg_util
{
  /**
   * class ArrayAggregateOnOuterDimension: subclass of libdap::Array
   *
   * Array variable which contains information for performing a
   * joinNew (new outer dimension) aggregation of an Array variable using
   * samples of this variable in a specified list of member datasets.
   *
   * The list is specified as a list of RCPtr<AggMemberDataset>,
   * i.e. reference-counted AggMemberDataset's (AMD).  The AMD is in
   * charge of lazy-loading it's contained DataDDS as needed
   * as well as for only loading the required data for a read() call.
   * In other words, read() on this subclass will respect the constraints
   * given to the superclass Array.
   *
   * Note: this class is designed to lazy-load the member
   * datasets _only if there are needed for the actual serialization_
   * at read() call time.
   *
   * Note: the member datasets might be external files or might be
   * wrapped virtual datasets (specified in NcML) or nested
   * aggregation's.
   */
  class ArrayAggregateOnOuterDimension : public libdap::Array
  {
  public:
    /**
     * Construct a joinNew Array aggregation given the parameters.
     *
     * @param proto the Array to use as a prototype for the UNaggregated Array
     *              (ie module the new dimension).
     *              It is the object for the aggVar as loaded from memberDatasets[0].
     * @param newDim the new outer dimension this instance will add to the proto Array template
     * @param memberDatasets  list of the member datasets forming the aggregation.
     *                        this list will be copied internally so memberDatasets
     *                        may be destroyed after this call (though the element
     *                        AggMemberDataset objects will be ref()'d in our copy).
     * @param loaderProto loader to reuse for loading the memberDatasets if needed.
     * @param arrayGetter  smart ptr to the algorithm for getting out the constrained
     *                     array from each individual AMDList DataDDS.
     *                     Ownership transferred to this (clearly).
     */
    ArrayAggregateOnOuterDimension(const libdap::Array& proto,
         const Dimension& newDim,
         const AMDList& memberDatasets,
         const DDSLoader& loaderProto,
         std::auto_ptr<ArrayGetterInterface>& arrayGetter
         );

    /** Construct from a copy */
    ArrayAggregateOnOuterDimension(const ArrayAggregateOnOuterDimension& proto);

    /** Destroy any local memory */
    virtual ~ArrayAggregateOnOuterDimension();

    /**
     * Virtual Constructor: Make a deep copy (clone) of the object
     * and return it.
     * @return ptr to the cloned object.
     */
    virtual ArrayAggregateOnOuterDimension* ptr_duplicate();

    /**
     * Assign this from rhs object.
     * @param rhs the object to copy from
     * @return *this
     */
    ArrayAggregateOnOuterDimension& operator=(const ArrayAggregateOnOuterDimension& rhs);

    virtual void set_send_p(bool state);
    virtual void set_in_selection(bool state);

    /**
     * Get the list of AggMemberDataset's that comprise this aggregation
     */
    const AMDList& getDatasetList() const;

    /**
    * Read in only those datasets that are in the currently constrained output
    * making sure to apply the internal dimension constraints to the
    * member datasets as well before reading them in.
    * Stream the data into this's output buffer correctly for serialization.
    * Note: This does nothing if read_p() is already set.
    * Note: This also does nothing if the variable !is_in_selection()
    * or !send_p().
    * @return success.
    */
    virtual bool read();

  private: // helpers

    /** Duplicate just the local (this subclass) data rep */
    void duplicate(const ArrayAggregateOnOuterDimension& rhs);

    /** Delete any heap memory */
    void cleanup() throw();

    /** Print out the constraints on fromArray to the debug channel */
    void printConstraints(const Array& fromArray);

    /** Actually go through the constraints and stream the correctly
     * constrained data into the superclass's output buffer for
     * serializing out.
     */
    void readMemberArraysAndAggregateData();

    /**
     * Load the given dataset's DataDDS.
     * Find the aggVar of the given name in it, must be Array.
     * Transfer the constraints from the local template to it.
     * Call read() on it.
     * Stream the data into rOutputArray's output buffer
     * at the element index nextElementIndex.
     *
     * @param rOutputArray  the Array to output the data into
     * @param atIndex  where in the output buffer of rOutputArray
     *                          to stream it (note: not byte, element!)
     * @param subArrayProto  the Array to use as the template for the
     *                       constraints on loading the dataset.
     * @param name the name of the aggVar
     * @param dataset the dataset to load for this element.
     */
    static void addDatasetArrayDataToOutputArray(
        libdap::Array& oOutputArray,
        unsigned int atIndex, // element (not byte!) to place the data in output buffer
        const libdap::Array& subArrayProto,
        const string& name,
        AggMemberDataset& dataset);

  private: // data rep

    // Use this to laod the member datasets as needed
    DDSLoader _loader;

    // The new outer dimension description
    Dimension _newDim;

    // Entries contain information on loading the individual datasets
    // in a lazy evaluation as needed if they are in the output.
    // The elements are ref counted, so need not be destroyed.
    AMDList _datasetDescs;

    // A template for the unaggregated (sub) Array
    // It will be used to constrain and validate other
    // dataset aggVar's as they are loaded.
    // OWNED heap memory.
    std::auto_ptr<libdap::Array> _pSubArrayProto;

    // Use this object in order to get the
    // constrained, read data out for aggregating into this object.
    std::auto_ptr<ArrayGetterInterface> _pArrayGetter;

  }; // class ArrayAggregateOnOuterDimension

}

#endif /* __AGG_UTIL__ARRAY_AGGREGATE_ON_OUTER_DIMENSION_H__ */

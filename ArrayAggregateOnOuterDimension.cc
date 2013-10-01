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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//
// Please see the files COPYING and COPYRIGHT for more information on the GLPL.
//
// You can contact OPeNDAP, Inc. at PO Box 112, Saunderstown, RI. 02874-0112.
/////////////////////////////////////////////////////////////////////////////

#include "ArrayAggregateOnOuterDimension.h"

#include "AggregationException.h"
#include <DataDDS.h> // libdap::DataDDS

// only NCML backlinks we want in this agg_util class.
#include "NCMLDebug.h" // BESDEBUG and throw macros
#include "NCMLUtil.h" // SAFE_DELETE, NCMLUtil::getVariableNoRecurse

// BES debug channel we output to
static const string DEBUG_CHANNEL("agg_util");

// Local flag for whether to print constraints, to help debugging
static const bool PRINT_CONSTRAINTS = true;

namespace agg_util
{

  ArrayAggregateOnOuterDimension::ArrayAggregateOnOuterDimension(
      const libdap::Array& proto,
      const AMDList& memberDatasets,
      std::auto_ptr<ArrayGetterInterface>& arrayGetter,
      const Dimension& newDim
  )
  : ArrayAggregationBase(proto, memberDatasets, arrayGetter) // no new dim yet in super chain
  , _newDim(newDim)
  {
    BESDEBUG(DEBUG_CHANNEL, "ArrayAggregateOnOuterDimension: ctor called!" << endl);

    // Up the rank of the array using the new dimension as outer (prepend)
    BESDEBUG(DEBUG_CHANNEL,
        "ArrayAggregateOnOuterDimension: adding new outer dimension: "
        << _newDim.name
        << endl);
    prepend_dim(_newDim.size, _newDim.name);
  }

  ArrayAggregateOnOuterDimension::ArrayAggregateOnOuterDimension(
      const ArrayAggregateOnOuterDimension& proto
      )
  : ArrayAggregationBase(proto)
  , _newDim()
  {
    BESDEBUG(DEBUG_CHANNEL,
        "ArrayAggregateOnOuterDimension() copy ctor called!" <<
        endl);
    duplicate(proto);
  }

  ArrayAggregateOnOuterDimension::~ArrayAggregateOnOuterDimension()
  {
    BESDEBUG(DEBUG_CHANNEL, "~ArrayAggregateOnOuterDimension() dtor called!" << endl);
    cleanup();
  }

  ArrayAggregateOnOuterDimension*
  ArrayAggregateOnOuterDimension::ptr_duplicate()
  {
    return new ArrayAggregateOnOuterDimension(*this);
  }

  ArrayAggregateOnOuterDimension&
  ArrayAggregateOnOuterDimension::operator=(const ArrayAggregateOnOuterDimension& rhs)
  {
    if (this != &rhs)
      {
        cleanup();
        ArrayAggregationBase::operator=(rhs);
        duplicate(rhs);
      }
    return *this;
  }

  ////////////////////////////////////////////////////////////////////////////////////////////
  // helpers

  void
  ArrayAggregateOnOuterDimension::duplicate(const ArrayAggregateOnOuterDimension& rhs)
  {
    _newDim = rhs._newDim;
  }

  void
  ArrayAggregateOnOuterDimension::cleanup() throw()
  {
  }

  /* virtual */
  void
  ArrayAggregateOnOuterDimension::transferOutputConstraintsIntoGranuleTemplateHook()
  {
    // transfer the constraints from this object into the subArray template
    // skipping our first dim which is the new one and not in the subArray.
    agg_util::AggregationUtil::transferArrayConstraints(
        &(getGranuleTemplateArray()), // into this template
        *this, // from this
        true, // skip first dim in the copy since we handle it special
        false, // also skip it in the toArray for the same reason.
        true, // print debug
        DEBUG_CHANNEL); // on this channel
  }

  /* virtual */
  void
  ArrayAggregateOnOuterDimension::readConstrainedGranuleArraysAndAggregateDataHook()
  {
    // outer one is the first in iteration
     const Array::dimension& outerDim = *(dim_begin());
     BESDEBUG(DEBUG_CHANNEL,
         "Aggregating datasets array with outer dimension constraints: "
         << " start=" << outerDim.start
         << " stride=" << outerDim.stride
         << " stop=" << outerDim.stop
         << endl);

     // Be extra sure we have enough datasets for the given request
     if (static_cast<unsigned int>(outerDim.size) != getDatasetList().size())
       {
         // Not sure whose fault it was, but tell the author
         THROW_NCML_PARSE_ERROR(-1,
             "The new outer dimension of the joinNew aggregation doesn't "
             " have the same size as the number of datasets in the aggregation!");
       }

     // Prepare our output buffer for our constrained length
     reserve_value_capacity();

     // this index pointing into the value buffer for where to write.
     // The buffer has a stride equal to the _pSubArrayProto->length().
     int nextElementIndex = 0;

     // Traverse the dataset array respecting hyperslab
     for (int i = outerDim.start;
         i <= outerDim.stop && i < outerDim.size;
         i += outerDim.stride)
       {
         AggMemberDataset& dataset = *((getDatasetList())[i]);

         try
         {
           agg_util::AggregationUtil::addDatasetArrayDataToAggregationOutputArray(
               *this, // into the output buffer of this object
               nextElementIndex, // into the next open slice
               getGranuleTemplateArray(), // constraints template
               name(), // aggvar name
               dataset, // Dataset who's DDS should be searched
               getArrayGetterInterface(),
               DEBUG_CHANNEL
           );
         }
         catch (agg_util::AggregationException& ex)
         {
           std::ostringstream oss;
           oss << "Got AggregationException while streaming dataset index="
               << i
               << " data for location=\""
               << dataset.getLocation()
               << "\" The error msg was: "
               << std::string(ex.what());
           THROW_NCML_PARSE_ERROR(-1, oss.str());
         }

         // Jump forward by the amount we added.
         nextElementIndex += getGranuleTemplateArray().length();
       }

     // If we succeeded, we are at the end of the array!
     NCML_ASSERT_MSG(nextElementIndex == length(),
         "Logic error:\n"
         "ArrayAggregateOnOuterDimension::read(): "
         "At end of aggregating, expected the nextElementIndex to be the length of the "
         "aggregated array, but it wasn't!");
  }

}

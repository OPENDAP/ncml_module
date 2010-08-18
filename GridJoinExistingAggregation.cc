///////////////////////////////////////////////////////////////////////////////
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
#include "GridJoinExistingAggregation.h"

#include "AggregationUtil.h" // agg_util
#include "ArrayJoinExistingAggregation.h" // agg_util
#include <Array.h> // libdap
#include "DDSLoader.h"
#include "NCMLDebug.h"

using libdap::Array;
using libdap::Grid;

static const string DEBUG_CHANNEL("agg_util");

namespace agg_util
{

  GridJoinExistingAggregation::GridJoinExistingAggregation(
      const libdap::Grid& proto,
      const AMDList& memberDatasets,
      const DDSLoader& loaderProto,
      const Dimension& joinDim)
  // init WITHOUT a template.  We need to do maps specially.
  : GridAggregationBase(proto.name(), memberDatasets, loaderProto)
  , _joinDim(joinDim)
  {
    createRep(proto, memberDatasets);
  }

  GridJoinExistingAggregation::GridJoinExistingAggregation(
      const GridJoinExistingAggregation& proto)
  : GridAggregationBase(proto)
  , _joinDim(proto._joinDim)
  {
    duplicate(proto);
  }

  /* virtual */
  GridJoinExistingAggregation::~GridJoinExistingAggregation()
  {
    cleanup();
  }

  /* virtual */
  GridJoinExistingAggregation*
  GridJoinExistingAggregation::ptr_duplicate()
  {
    return new GridJoinExistingAggregation(*this);
  }

  GridJoinExistingAggregation&
  GridJoinExistingAggregation::operator=(const GridJoinExistingAggregation& rhs)
  {
    if (this != &rhs)
      {
        cleanup();
        GridAggregationBase::operator=(rhs);
        duplicate(rhs);
      }
    return *this;
  }

  ///////////////////////////////////////////////////////////
  // Subclass Impl (Protected)

  /* virtual */
  void
  GridJoinExistingAggregation::readAndAggregateConstrainedMapsHook()
  {
    static const string sFuncName("GridJoinExistingAggregation::readAndAggregateConstrainedMapsHook(): ");
    THROW_NCML_INTERNAL_ERROR(sFuncName + "Impl Me!");

    /* TODO Refactor the joinnew versions and then make some reduced size test cases.
     * We need to load the other maps, noting that we need to read the ENTIRE
     * subgrid since the HDF and otehr handlers CANNOT READ STANDALONE COPIES
     * of a map. Argh.
     *
     * The inner dim maps needs to be then copied from the proto subgrid into this
     * object's map output buffers.
     *
     * The aggregated dimensions should just work, with the constraints passed in as given.
     */
  }

  ///////////////////////////////////////////////////////////
  // Private Impl

  void
  GridJoinExistingAggregation::duplicate(const GridJoinExistingAggregation& rhs)
  {
    _joinDim = rhs._joinDim;
  }

  void
  GridJoinExistingAggregation::cleanup() throw()
  {
  }

  void
  GridJoinExistingAggregation::createRep(
      const libdap::Grid& constProtoSubGrid,
      const AMDList& memberDatasets)
  {
    static const string sFuncName("GridJoinExistingAggregation::createRep(): ");

    // Semantically const calls below
    Grid& protoSubGrid = const_cast<Grid&>(constProtoSubGrid);

    // Set up the shape, don't add maps
    setShapeFrom(protoSubGrid, false);

    // Add the maps by hand, leaving out the first (outer dim) one
    // since we need to make add that specially.
    Grid::Map_iter firstIt = protoSubGrid.map_begin();
    Grid::Map_iter endIt = protoSubGrid.map_end();
    for (Grid::Map_iter it = firstIt;
        it != endIt;
        ++it)
      {
        // Skip the first one, assuming it's the join dim
        if (it == firstIt)
          {
             NCML_ASSERT_MSG( (*it)->name() == _joinDim.name,
                 sFuncName +
                 "Expected the first map to be the outer dimension "
                 "named " + _joinDim.name +
                 " but it was not!  Logic problem.");
             continue;
          }

        // Add the others.
        Array* pMap = dynamic_cast<Array*>(*it);
        VALID_PTR(pMap);
        add_map(pMap, true); // add as a copy
      }

    BESDEBUG(DEBUG_CHANNEL, sFuncName +
        "Replacing the Grid's data Array with an ArrayAggregateOnOuterDimension..." << endl);

    // This is the prototype we need.  It will have been set in the ctor.
    Array* pArr = static_cast<Array*>(array_var());
    NCML_ASSERT_MSG(pArr, sFuncName +
         "Expected to find a contained data Array but we did not!");

    // Create the Grid version of the read getter and make a new AAOOD from our state.
    std::auto_ptr<ArrayGetterInterface> arrayGetter(new TopLevelGridDataArrayGetter());

    // Create the subclass that does the work and replace our data array with it.
    // Note this ctor will prepend the new dimension itself, so we do not.
    std::auto_ptr<ArrayJoinExistingAggregation> aggDataArray(
          new ArrayJoinExistingAggregation(
              *pArr, // prototype, already should be setup properly _without_ the new dim
              memberDatasets,
              arrayGetter,
              _joinDim
              ));

    // Make sure null since sink function
    // called on the auto_ptr
    NCML_ASSERT(!(arrayGetter.get()));

    // Replace our data Array with this one.  Will delete old one and may throw.
    set_array(aggDataArray.get());

    // Release here on successful set since set_array uses raw ptr only.
    // In case we threw then auto_ptr cleans up itself.
    aggDataArray.release();
  }


} // namespace agg_util

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

#include "AggregationUtil.h" // agg_util
#include <Array.h> // libdap
#include "GridAggregationBase.h" // agg_util
#include "NCMLDebug.h"

using libdap::Array;
using libdap::BaseType;
using libdap::Grid;

// Local debug flags
static const string DEBUG_CHANNEL("agg_util");
static const bool PRINT_CONSTRAINTS(true);

namespace agg_util
{
  GridAggregationBase::GridAggregationBase(
      const libdap::Grid& proto,
      const AMDList& memberDatasets,
      const DDSLoader& loaderProto)
  : Grid(proto)
  , _loader(loaderProto.getDHI())
  , _pSubGridProto(cloneSubGridProto(proto))
  , _memberDatasets(memberDatasets)
  {
  }

  GridAggregationBase::GridAggregationBase(
        const string& name,
        const AMDList& memberDatasets,
        const DDSLoader& loaderProto)
    : Grid(name)
    , _loader(loaderProto.getDHI())
    , _pSubGridProto(0)
    , _memberDatasets(memberDatasets)
  {
  }

  GridAggregationBase::GridAggregationBase(const GridAggregationBase& proto)
  : Grid(proto)
  , _loader(proto._loader.getDHI())
  , _pSubGridProto(0) // init below
  , _memberDatasets()
  {
    duplicate(proto);
  }

  /* virtual */
  GridAggregationBase::~GridAggregationBase()
  {
    cleanup();
  }

  GridAggregationBase&
  GridAggregationBase::operator=(const GridAggregationBase& rhs)
  {
    if (this != &rhs)
      {
        cleanup();
        Grid::operator=(rhs);
        duplicate(rhs);
      }
    return *this;
  }

  void
  GridAggregationBase::setShapeFrom(const libdap::Grid& constProtoSubGrid, bool addMaps)
  {
    // calls used are semantically const, but not syntactically.
    Grid& protoSubGrid = const_cast<Grid&>(constProtoSubGrid);

    // Save a clone of the template for read() to use.
    // We always use these maps...
    _pSubGridProto = auto_ptr<Grid>(cloneSubGridProto(protoSubGrid));

    // Pass in the data array and maps from the proto by hand.
    Array* pDataArrayTemplate = protoSubGrid.get_array();
    VALID_PTR(pDataArrayTemplate);
    set_array( static_cast<Array*>(pDataArrayTemplate->ptr_duplicate()) );

    // Now the maps in order if asked
    if (addMaps)
      {
        Grid::Map_iter endIt = protoSubGrid.map_end();
        for (Grid::Map_iter it = protoSubGrid.map_begin();
            it != endIt;
            ++it)
          {
            // have to case, the iter is for some reason BaseType*
            Array* pMap = dynamic_cast<Array*>(*it);
            VALID_PTR(pMap);
            add_map(pMap, true); // add as a copy
          }
      }
  }

  /* virtual */
  const AMDList&
  GridAggregationBase::getDatasetList() const
  {
    return _memberDatasets;
  }

  /* virtual */
  bool
  GridAggregationBase::read()
  {
    BESDEBUG_FUNC(DEBUG_CHANNEL, "Function entered..." << endl);

    if (read_p())
      {
        BESDEBUG_FUNC(DEBUG_CHANNEL, "read_p() set, early exit!");
        return true;
      }

    if (PRINT_CONSTRAINTS)
      {
        printConstraints(*(get_array()));
      }

    // Call the subclass hook methods to do this work properly
    readAndAggregateConstrainedMapsHook();

    // Now make the read call on the data array.
    // The aggregation subclass will do the right thing.
    Array* pAggArray = get_array();
    VALID_PTR(pAggArray);

    // Only do this portion if the array part is supposed to serialize!
    if (pAggArray->send_p() || pAggArray->is_in_selection())
      {
        pAggArray->read();
      }

    // Set the cache bit.
    set_read_p(true);
    return true;
  }

  /////////////////////////////////////////
  ///////////// Helpers

  Grid*
  GridAggregationBase::getSubGridTemplate()
  {
    return _pSubGridProto.get();
  }

  void
  GridAggregationBase::duplicate(const GridAggregationBase& rhs)
  {
    _loader = DDSLoader(rhs._loader.getDHI());

    std::auto_ptr<Grid> pGridTemplateClone(( (rhs._pSubGridProto.get()) ?
        (static_cast<Grid*>(rhs._pSubGridProto->ptr_duplicate())) :
        (0) ));
    _pSubGridProto = pGridTemplateClone;

    _memberDatasets = rhs._memberDatasets;
  }

  void
  GridAggregationBase::cleanup() throw()
  {
    _loader.cleanup();

    _memberDatasets.clear();
    _memberDatasets.resize(0);
  }

  /* virtual */
  void
  GridAggregationBase::readAndAggregateConstrainedMapsHook()
  {
    // Transfers constraints to the proto grid and reads it
    readProtoSubGrid();

    // Copy the read-in, constrained maps from the proto grid
    // into our output maps.
    copyProtoMapsIntoThisGrid(getAggregationDimension());
  }

  /* static */
  libdap::Grid*
  GridAggregationBase::cloneSubGridProto(const libdap::Grid& proto)
  {
    return static_cast<Grid*>( const_cast<Grid&>(proto).ptr_duplicate() );
  }

  void
  GridAggregationBase::printConstraints(const Array& fromArray)
  {
    ostringstream oss;
    AggregationUtil::printConstraints(oss, fromArray);
    BESDEBUG("ncml:2", "Constraints for Grid: " << name() << ": " << oss.str() << endl);
  }

  void
  GridAggregationBase::readProtoSubGrid()
  {
    Grid* pSubGridTemplate = getSubGridTemplate();
    VALID_PTR(pSubGridTemplate);

    // Call the specialized subclass constraint transfer method
    transferConstraintsToSubGridHook(pSubGridTemplate);

    // Pass it the values for the aggregated grid...
    pSubGridTemplate->set_send_p(send_p());
    pSubGridTemplate->set_in_selection(is_in_selection());

    // Those settings will be used by read.
    pSubGridTemplate->read();

    // For some reason, some handlers only set read_p for the parts, not the whole!!
    pSubGridTemplate->set_read_p(true);
  }

  void
  GridAggregationBase::copyProtoMapsIntoThisGrid(const Dimension& aggDim)
  {
    Grid* pSubGridTemplate = getSubGridTemplate();
    VALID_PTR(pSubGridTemplate);

    Map_iter mapIt;
    Map_iter mapEndIt = map_end();
    for (mapIt = map_begin();
        mapIt != mapEndIt;
        ++mapIt)
      {
        Array* pOutMap = static_cast<Array*>(*mapIt);
        VALID_PTR(pOutMap);

        // If it isn't getting dumped, then don't bother with it
        if (! (pOutMap->send_p() || pOutMap->is_in_selection()) )
          {
            continue;
          }

        // We don't want to touch the aggregation dimension since it's
        // handled specially.
        if (pOutMap->name() == aggDim.name)
          {
            if (PRINT_CONSTRAINTS)
              {
                BESDEBUG_FUNC(DEBUG_CHANNEL,
                    "About to call read() on the map for the new outer dimension name=" <<
                    aggDim.name <<
                    " It's constraints are:" << endl);
                printConstraints(*pOutMap);
              }

            // Make sure it's read with these constraints.
            pOutMap->read();
            continue;
          }

        // Otherwise, find the map in the protogrid and copy it's data into this.
        Array* pProtoGridMap = const_cast<Array*>(AggregationUtil::findMapByName(*pSubGridTemplate, pOutMap->name()));
        NCML_ASSERT_MSG(pProtoGridMap,
            "Couldn't find map in prototype grid for map name=" + pOutMap->name() );
        BESDEBUG_FUNC(DEBUG_CHANNEL,
            "About to call read() on prototype map vector name="
            << pOutMap->name() << " and calling transfer constraints..." << endl);

        // Make sure the protogrid maps were properly read
        NCML_ASSERT_MSG(pProtoGridMap->read_p(),
            "Expected the prototype map to have been read but it wasn't.");

        // Make sure the lengths match to be sure we're not gonna blow memory up
        NCML_ASSERT_MSG(pOutMap->length() == pProtoGridMap->length(),
            "Expected the prototype and output maps to have same length() "
            "after transfer of constraints, but they were not so we can't "
            "copy the data!");

        // The dimensions will have been set up correctly now so length() is correct...
        // We assume the pProtoGridMap matches at this point as well.
        // So we can use this call to copy from one vector to the other
        // so we don't use temp storage in between
        pOutMap->reserve_value_capacity(); // reserves mem for length
        pOutMap->set_value_slice_from_row_major_vector(*pProtoGridMap, 0);
        pOutMap->set_read_p(true);
      }
  }

  /* virtual */
  void
  GridAggregationBase::transferConstraintsToSubGridHook(Grid* /*pSubGrid*/)
  {
    THROW_NCML_INTERNAL_ERROR("Impl me!");
  }

}
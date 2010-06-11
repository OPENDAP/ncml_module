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
#ifndef __AGG_UTIL__GRID_AGGREGATE_ON_OUTER_DIMENSION_H__
#define __AGG_UTIL__GRID_AGGREGATE_ON_OUTER_DIMENSION_H__

#include <string>
#include <vector>

#include "AggMemberDataset.h" // agg_util
#include "Dimension.h" // agg_util
#include "DDSLoader.h" // agg_util

#include <Grid.h> // libdap

namespace libdap
{
  class Array;
};

using std::string;
using std::vector;
using libdap::Array;
using libdap::Grid;

namespace agg_util
{
  /**
   * class GridAggregateOnOuterDimension : public libdap::Grid
   *
   * Grid that performs a joinNew aggregation by taking
   * an ordered list of datatsets which contain Grid's with
   * matching signatures (ie array name, dimensions, maps)
   * and creating a new outer dimension with cardinality the
   * number of datasets in the aggregation.
   *
   * The resulting aggregated grid will be one plus the
   * rank of the array's of the member dataset Grids.
   *
   * We assume we have created the proper shape of this
   * Grid so that the map vectors in the Grid superclass
   * are correct (as loaded from the first dataset!)
   *
   * The data itself will be loaded as needed during serialize
   * based on the response.
   *
   * TODO OPTIMIZE One major issue is we create one of these
   * objects per aggregation variable, even if these
   * vars are in the same dataset.  So when we read,
   * we load the dataset for each variable!  Does the BES
   * cache them or something?  Maybe we can avoid doing this
   * with some smarts?  Not sure...  I get the feeling
   * the other handlers do the same sort of thing,
   * but can be smarter about seeking, etc.
   */
  class GridAggregateOnOuterDimension : public libdap::Grid
  {
  public:
    /** Create the new Grid from the template proto... we'll
     * have the same name, dimension and attributes.
     * @param proto  template to use, will be the aggVar from the FIRST member dataset!
     * @param memberDatasets  descriptors for the dataset members of the aggregation.  ASSUMED
     *                  that the contained DDS will have a Grid var with name proto.name() that
     *                  matches rank and maps, etc!!
     * @param loaderProto loaded template to use (borrow dhi from) to load the datasets
     */
    GridAggregateOnOuterDimension(const Grid& proto,
        const Dimension& newDim,
        const AMDList& memberDatasets,
        const DDSLoader& loaderProto);

    GridAggregateOnOuterDimension(const GridAggregateOnOuterDimension& proto);

    virtual ~GridAggregateOnOuterDimension();

    virtual GridAggregateOnOuterDimension* ptr_duplicate();

    GridAggregateOnOuterDimension& operator=(const GridAggregateOnOuterDimension& rhs);

    // Overrides to make sure I am getting the calls.
    virtual void set_send_p(bool state);
    virtual void set_in_selection(bool state);

    /**
     * Read in only those datasets that are in the constrained output
     * making sure to apply the internal dimension constraints to the
     * member datasets properly before reading them!
     * Stream the data into the output buffer correctly.
     * @return success.
     */
    virtual bool read();

  private: // helpers

    /** Duplicate just the local data rep */
    void duplicate(const GridAggregateOnOuterDimension& rhs);

    /** Delete any heap memory */
    void cleanup() throw();

    /** Given the parts to create the Grid from,
     * add the new dimension to the Array so that our shape is correct.
     * NOTE: We do NOT add the new map here yet since it may be given
     * explicitly in the aggregation, so we let the aggregation do it.
     * TODO: Consider adding a placeholder map here that we can remove
     * and replace in the case that it does get replaced later.
     */
    void createRep();

    /** Helper function to load and stream the given dataset's data into the pAggArray.
     * Also will validate that shapes match the prototype, etc etc.
     * @param pAggArray  the output Array to stream the data, assumed reserved enough memory.
     * @param atIndex the location in pAggArray (element) to start streaming
     * @param protoSubArray the example subarray, whose structure the dataset member needs to match!
     * @param gridName the grid's name (right now name(), but preparing to refactor)
     * @param dataset the dataset to load, stream and close.
     * TODO this might be able to refactor all the way into AggregationUtil static class.
     */
    void addDatasetGridArrayDataToAggArray(Array* pAggArray,
         unsigned int atIndex,
         const Array& protoSubArray,
         const string& gridName,
         AggMemberDataset& dataset);

    /**
     * Transfer the constraints to the prototype and read it in.
     */
    void readProtoSubGrid();

    /** Copy the maps read in from readProtoSubGrid into
     * this Grid's maps.
     * */
    void copyProtoMapsIntoThisGrid();

    /**
     * For all the datasets requested in the outer dimension,
     * load them in and stream their data into this
     * object's array portion.
     */
    void readAndAggregateGrids();

    /**
     * For the data array and all maps, transfer the constraints
     * from the super grid (ie this) to all the grids in the
     * given prototype subgrid.
     *
     * Note that this Grid has one more outer dimension than the
     * subgrid, so the first one on this will clearly be skipped.
     *
     * @param pToGrid
    */
    void transferConstraintsToSubGrid(Grid* pSubGrid);

    void transferConstraintsToSubGridMaps(Grid* pSubGrid);
    void transferConstraintsToSubGridArray(Grid* pSubGrid);

    void printConstraints(const Array& fromArray);

    /** Find the map with the given name or NULL if not found */
    static Array* findMapByName(Grid& inGrid, const string& findName);

  private: // data rep
    // Use this to laod the member datasets as needed
    DDSLoader _loader;

    // The new outer dimension description
    Dimension _newDim;

    // Entries contain information on loading the individual datasets
    // in a lazy evaluation as need if they are in the output.
    AMDList _datasetDescs;

    // A template for the unaggregated (sub) Grids.
    // It will be used to validate other datasets as they are loaded.
    Grid* _pSubGridProto;
  };

}

#endif // __AGG_UTIL__GRID_AGGREGATE_ON_OUTER_DIMENSION_H__

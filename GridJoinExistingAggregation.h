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

#ifndef __AGG_UTIL__GRID_JOIN_EXISTING_AGGREGATION_H__
#define __AGG_UTIL__GRID_JOIN_EXISTING_AGGREGATION_H__

#include "Dimension.h" // agg_util
#include "GridAggregationBase.h" // agg_util

namespace agg_util
{
  class GridJoinExistingAggregation : public GridAggregationBase
  {
  public:

    GridJoinExistingAggregation(const libdap::Grid& proto,
        const AMDList& memberDatasets,
        const DDSLoader& loaderProto,
        const Dimension& joinDim);

    GridJoinExistingAggregation(const GridJoinExistingAggregation& proto);

    virtual ~GridJoinExistingAggregation();

    virtual GridJoinExistingAggregation* ptr_duplicate();

    GridJoinExistingAggregation& operator=(const GridJoinExistingAggregation& rhs);

  protected: // subclass interface

    /**
    * Hook for subclasses to handle the read() functionality on the
    * maps.
    * Called from read()!
    */
    virtual void readAndAggregateConstrainedMapsHook();

  private: // helpers

    /** Duplicate just the local data rep */
    void duplicate(const GridJoinExistingAggregation& rhs);

    /** Delete any heap memory */
    void cleanup() throw();

    /** Create the representation.
     * Replaces our data Array with an aggregating one.
     * Sets up the outer dimension properly.
     * @param protoSubGrid  prototype for the shape pre-agg.
     * @param granuleList   agg granules
     */
    void createRep(const libdap::Grid& protoSubGrid, const AMDList& granuleList);

  private: // Data Rep

    Dimension _joinDim;

  }; // class GridJoinExistingAggregation
} // namespace agg_util

#endif /* __AGG_UTIL__GRID_JOIN_EXISTING_AGGREGATION_H__ */

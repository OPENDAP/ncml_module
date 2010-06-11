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
#ifndef __AGG_UTIL__AGG_MEMBER_DATASET_H__
#define __AGG_UTIL__AGG_MEMBER_DATASET_H__

#include "RCObject.h"
#include <string>
#include <vector>

namespace libdap
{
  class DataDDS;
  class DDS;
};

using libdap::DataDDS;
using libdap::DDS;

namespace agg_util
{
  /**
   * Abstract helper superclass for allowing lazy access to the DataDDS
   * for an aggregation. This is used during a read() if the dataset
   * is needed in an aggregation.
   *
   * Note: This inherits from RCObject so is a ref-counted object to avoid
   * making excessive copies of it and especially of any contained DDS.
   *
   * Currently, there are two concrete subclasses:
   *
   * o AggMemberDatasetUsingLocationRef: to load an external location
   *            into a DataDDS as needed
   *            (laxy eval so not loaded unless in the output of read() )
   *
   * o AggMemberDatasetDDSWrapper: to hold a pre-loaded DataDDS for the
   *            case of virtual or pre-loaded datasets
   *            (data declared in NcML file, nested aggregations, e.g.)
   *            In this case, getLocation() is presumed empty().
  */
  class AggMemberDataset : public RCObject
  {
  public:
    AggMemberDataset(const std::string& location);
    virtual ~AggMemberDataset();

    AggMemberDataset& operator=(const AggMemberDataset& rhs);
    AggMemberDataset(const AggMemberDataset& proto);

    /** The location to which the AggMemberDataset refers
     * Note: this could be "" for some subclasses
     * if they are virtual or nested */
    const std::string& getLocation() const { return _location; }

    /**
     * Return the DataDDS for the location, loading it in
     * if it hasn't yet been loaded.
     * Can return NULL if there's a problem.
     * (Not const due to lazy eval)
     * @return the DataDDS ptr containing the loaded dataset or NULL.
     */
    virtual const libdap::DataDDS* getDataDDS() = 0;

  private: // data rep
    std::string _location; // non-empty location from which to load DataDDS
  };

  // List is ref-counted ptrs to AggMemberDataset concrete subclasses.
  typedef std::vector< RCPtr<AggMemberDataset> > AMDList;

}; // namespace agg_util

#endif /* __AGG_UTIL__AGG_MEMBER_DATASET_H__ */

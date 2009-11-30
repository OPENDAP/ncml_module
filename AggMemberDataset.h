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

#include <string>

class BESDataDDSResponse;

namespace libdap
{
  class DataDDS;
  class DDS;
};

namespace agg_util
{
  class DDSLoader;
};

using libdap::DataDDS;
using libdap::DDS;
using std::string;


namespace agg_util
{
  /**
   * Helper class for storing data about member datasets
   * of an aggregation so that their data response can be
   * loaded only if it's in the request.
   *
   * NOTE: This relies on the DDSLoader from the NCMLParser
   * which is rehijacked to load datasets.
   * TODO Investigate a cleaner way to load these datasets
   * than continuing the chain of DHI hijacking...
   */
  class AggMemberDataset
  {
  private: // disallow

  public:
    AggMemberDataset(const string& location="");
    virtual ~AggMemberDataset();

    AggMemberDataset& operator=(const AggMemberDataset& rhs);
    AggMemberDataset(const AggMemberDataset& proto);

    const string& getLocation() const { return _location; }

    /** Load the given location as a data response so that we have a valid DataDDS
     * for streaming data from */
    void loadDataDDS(DDSLoader& loader);

    /** Get the DDS for the location.
     * Make sure to call loadDDS first!
     * @return the DDS or null if not loaded.
     *  */
    libdap::DataDDS* getDataDDS() const;

    /**
     * Delete any loaded DataDDS to tighten up memory usage.
     * ON EXIT: (getDataDDS() == 0)
     */
    void clearDataDDS();

  private: // helpers
    void cleanup() throw();

  private: // data rep
    string _location;
    BESDataDDSResponse* _pDataResponse; // holds our loaded DDS
  };

}

#endif /* __AGG_UTIL__AGG_MEMBER_DATASET_H__ */

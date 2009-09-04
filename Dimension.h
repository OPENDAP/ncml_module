///////////////////////////////////////////////////////////////////////////////
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

#ifndef __AGG_UTIL__DIMENSION_H__
#define __AGG_UTIL__DIMENSION_H__

#include <string>
#include <vector>

namespace agg_util
{
  /**
   * Struct for holding information about a dimension of data,
   * minimally a name and a cardinality (size).
   *
   * We use this class as a go-between to keep
   * ncml_module::DimensionElement out of the agg_util::AggregationUtil
   * dependencies.
   *
   * There's really no invariants on the data rep now, so I will leave it as a struct.
   *
   */
  struct Dimension
  {
  public:
    Dimension();
    Dimension(const std::string& nameArg, unsigned int sizeArg, bool isSharedArg, bool isSizeConstantArg);
    ~Dimension();

    // The name of the dimension (merely mnemonic)
    std::string name;

    // The cardinality of the dimension (number of elements)
    unsigned int size;

    // Whether the dimension in considered as shared across objects
    bool isShared;

    // whether the size is allowed to change or not, important for joinExisting, e.g.
    bool isSizeConstant;
  };

  /** Container class for a table of dimensions for a given dataset */
  class DimensionTable
  {
  public:
    DimensionTable(unsigned int capacity=0);
    ~DimensionTable();

    /** Add the dimension to the table if one with the same name doesn't already exist.
     *  If a dimension with the same name is already there, it is NOT added.
     */
    void addDimensionUnique(const Dimension& dim);

    /** Find the dimension with the given name.  If found,
     * place it into pOut if pOut is non-null
     * @return whether it was found
     */
    bool findDimension(const std::string& name, Dimension* pOut=0) const;

    const std::vector<Dimension>& getDimensions() const { return _dimensions; }

  private:
    std::vector<Dimension> _dimensions;
  };

}

#endif /* __AGG_UTIL__DIMENSION_H__ */
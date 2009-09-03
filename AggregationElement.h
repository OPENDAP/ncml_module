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
#ifndef __NCML_MODULE__AGGREGATION_ELEMENT_H__
#define __NCML_MODULE__AGGREGATION_ELEMENT_H__

#include "NCMLElement.h"
#include "NCMLUtil.h"
#include <string>
#include <vector>

namespace libdap
{
  class DDS;
};


using std::string;
using std::vector;
using libdap::DDS;

namespace ncml_module
{
  class NetcdfElement;
  class NCMLParser;

  class AggregationElement : public NCMLElement
  {
  public:
    // Name of the element
    static const string _sTypeName;

    // All possible attributes for this element.
    static const vector<string> _sValidAttrs;

    AggregationElement();
    AggregationElement(const AggregationElement& proto);
    virtual ~AggregationElement();
    virtual const string& getTypeName() const;
    virtual AggregationElement* clone() const; // override clone with more specific subclass
    virtual void setAttributes(const AttributeMap& attrs);
    virtual void handleBegin(NCMLParser& p);
    virtual void handleContent(NCMLParser& p, const string& content);
    virtual void handleEnd(NCMLParser& p);
    virtual string toString() const;

    const string& type() const { return _type; }
    const string& dimName() const {return _dimName; }
    const string& recheckEvery() const { return _recheckEvery; }

    /** Set the parent and return the old one, which could be null.
     * Should only be called on a handleBegin() call here when we know we can find it
     * in the NCMLParser at the current scope.
     * We only retain a weak reference to parent.
     */
    NetcdfElement* setParentDataset(NetcdfElement* parent);
    NetcdfElement* getParentDataset() const { return _parent; }


    /** Add a new dataset to the aggregation for the parse.
     * We now have a strong reference to it. */
    void addChildDataset(NetcdfElement* pDataset);

  private: // methods


    void processUnion();
    void processJoinNew();
    void processJoinExisting();

    /** Helper to pull out the DDS's for the child datasets and shove them into
     *  a vector<DDS*> for processing.
     *  On exit, datasets[i] contains _datasets[i]->getDDS().
     */
    void collectDatasetsInOrder(vector<DDS*>& ddsList) const;

    /**
     * Perform the union aggregation on the child dataset dimensions tables into
     * getParentDataset()'s dimension table.
     * If checkDimensionMismatch is set, then we throw a parse error
     * if a dimension with the same name is specified but its size doesn't
     * match the one that is in the union.
     */
    void mergeDimensions(bool checkDimensionMismatch=true);

    static vector<string> getValidAttributes();

  private:
    string _type; // required oneof { union | joinNew | joinExisting | forecastModelRunCollection | forecastModelSingleRunCollection }
    string _dimName;
    string _recheckEvery;

    // Set in handleBegin().
    NCMLParser* _parser;

    // Our containing NetcdfElement, which must exist.  This needs to be a weak reference to avoid ref loop....
    NetcdfElement* _parent;

    // The vector of loaded, parsed datasets, as NetcdfElement*.  We have a strong reference to these
    // if they are in this container and we must deref() them on dtor.
    vector<NetcdfElement*> _datasets;
  };

}

#endif /* __NCML_MODULE__AGGREGATION_ELEMENT_H__ */

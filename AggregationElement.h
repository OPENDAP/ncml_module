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

#include "AggMemberDataset.h"
#include <memory>
#include "NCMLElement.h"
#include "NCMLUtil.h"

namespace agg_util
{
  class Dimension;
};

namespace libdap
{
  class Array;
  class DDS;
  class Grid;
};

using agg_util::AggMemberDataset;
using libdap::Array;
using libdap::DDS;
using libdap::Grid;
using std::auto_ptr;

namespace ncml_module
{
  class NetcdfElement;
  class NCMLParser;
  class ScanElement;

  class AggregationElement : public NCMLElement
  {
  private:
    AggregationElement& operator=(const AggregationElement& rhs); // disallow

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
    virtual void setAttributes(const XMLAttributeMap& attrs);
    virtual void handleBegin();
    virtual void handleContent(const string& content);
    virtual void handleEnd();
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

    /** Set the variable with name as an aggregation variable for this
     * aggregation.
     */
    void addAggregationVariable(const string& name);

    /**
     * @return whether the variable with name has been added as an aggregation variable
     */
    bool isAggregationVariable(const string& name) const;

    string printAggregationVariables() const;

    typedef vector<string>::const_iterator AggVarIter;
    AggVarIter beginAggVarIter() const;
    AggVarIter endAggVarIter() const;

    /** Add a child ScanElement to the Aggregation
     * to be used to to add to the list of child datasets.
     * This will be a strong (ref()'d) reference.
     * @param pScanner the element to which to maintain a strong ref
     *                  and use in the processing.
     */
    void addScanElement(ScanElement* pScanner);

    /**
     * Called when the enclosing dataset is closing for the aggregation to handle
     * any post processing that it needs to, in particular adding any map vectors to Grid's.
     */
    void processParentDatasetComplete();

    /** If a child scan contains a dateFormatMark, then
       * we want to add a "_CoordinateAxisType" of "Time"
       * By setting this to a non empty() string,
       * a new attribute _CoordinateAxisType will be
       * added to aggregation variable in a joinNew
       * or joinExisting aggregation.
       * @param cat  the desired value for _CoordinateAxisType, or
       *             "" if no attribute addition is desired.
       */
    void setAggregationVariableCoordinateAxisType(const std::string& cat);

    /** Return the value set by setAggregationVariableCoordinateAxisType()
     * or "" if none was set.
     */
    const std::string& getAggregationVariableCoordinateAxisType() const;

  private: // methods

    void processUnion();
    void processJoinNew();
    void processJoinExisting();

    /** Helper called from processJoinNew iteration for each aggVar */
    void processJoinNewOnAggVar(
        const std::string& varName,
        DDS* pAggDDS,
        DDS* pTemplateDDS);

    /**
     * Create and add a new ArrayAggregateOnOuterDimension
     * instance to the given aggDDS which will lazy-evaluate
     * to produce the given aggregation during a read()
     * and serialization of the containing aggDDS.
     *
     * Note: memberDatasets will be copied and _does not need
     * to exist after this call_ (hence const).
     * Since the contained RCPtr<AggMemberDataset>'s will be copied,
     * the contained objects will be ref()'d and exist within
     * the newly constructed object and any copies of it.
     *
     * @param aggDDS the DDS we are aggregating the var into
     * @param arrayTemplate the template for the sub Array's members
     *                      (no new outer dim in it yet)
     * @param dim description of the new outer dimension
     * @param memberDatasets the list of dataset wrappers to
     *                       be copied in the construction of
     *                       the new aggregated variable.
    */
    void processAggVarJoinNewForArray(DDS& aggDDS,
        const Array& arrayTemplate,
        const agg_util::Dimension& dim,
        const agg_util::AMDList& memberDatasets );

    /**
     * Create and add a new aggregated Grid subclass
     * GridAggregateOnOuterDimension to aggDDS, using gridTemplate,
     * the aggregation variable pulled from the template (first) dataset
     * to define the shape of the aggregation members.
     *
     * Note: memberDatasets will be copied and _does not need
     * to exist after this call_ (hence const).
     * Since the contained RCPtr<AggMemberDataset>'s will be copied,
     * the contained objects will be ref()'d and exist within
     * the newly constructed object and any copies of it.
     *
     * @param aggDDS         the DDS we are aggregating the var into
     * @param gridTemplate   the template for the grid's members
     *                       (no new outer dim in it yet)
     * @param dim description of the new outer dimension
     * @param memberDatasets the list of dataset wrappers to
     *                       be copied in the construction of
     *                       the new aggregated variable.
     * @param aggDDS         the DDS we are aggregating the var into
     * @param dim            description of the new outer dimension
     * @param memberDatasets the list of dataset wrappers to
     *                       be copied in the construction of
     *                       the new aggregated variable.
    */
    void processAggVarJoinNewForGrid(DDS& aggDDS,
        const Grid& gridTemplate,
        const agg_util::Dimension& dim,
        const agg_util::AMDList& memberDatasets );

    /** Helper to pull out the DDS's for the child datasets and shove them into
     *  a vector<DDS*> for processing.
     *  On exit, datasets[i] contains _datasets[i]->getDDS().
     */
    void collectDatasetsInOrder(vector<const DDS*>& ddsList) const;

    /**
     * Fill the argument list _rMemberDatasets_ with shared references to
     * AggMemberDataset concrete subclasses (i.e. RCPtr<AggMemberDataset>)
     * The added datasets will be appended to the end of rMemberDatasets
     * in the order they are specified in the actual aggregation.
     *
     * @param rMemberDatasets place to put the new dataset classes.
     */
    void collectAggMemberDatasets(agg_util::AMDList& rMemberDatasets) const;

    /**
     * If there are any contained <scan> elements, process them in
     * order to add the scanned datasets into our list.
     */
    void processAnyScanElements();

    /**
     * Perform the union aggregation on the child dataset dimensions tables into
     * getParentDataset()'s dimension table.
     * If checkDimensionMismatch is set, then we throw a parse error
     * if a dimension with the same name is specified but its size doesn't
     * match the one that is in the union.
     */
    void mergeDimensions(bool checkDimensionMismatch=true);

    /** Called from processParentDatasetComplete() if we're a joinNew. */
    void processParentDatasetCompleteForJoinNew();

    /** Make sure the variable in pBT is a valid coordinate variable for the dimension dim
     * and return it as an Array* if so.  Else throw or return null.
     * A valid c.v. will:
     *          o Be an Array with name dim.name with 1 dimension whose name is also dim.name
     *          o Have a length() that matches dim.size
     * @param pBT  the variable to validate as a dim c.v.
     * @param dim  the dimension information to check for
     * @return pBT cast as an Array* if it is valid, else NULL.
     */
    libdap::Array* ensureVariableIsProperNewCoordinateVariable(
        libdap::BaseType* pBT,
        const agg_util::Dimension& dim,
        bool throwIfInvalid) const;

    /**
     * Search the given dds for a coordinate variable (CV) matching
     * dim and return it.
     *
     * This means it must:
     * o Have the name dim.name
     * o Be a 1D Array with shape matching dim
     *
     * If such a matching CV is found, it is returned.
     *
     * If a variable with dim.name does not exist in the DDS, NULL is returned.
     *
     * If a variable with dim.name is found
     * but it does NOT match the above criteria the either:
     *    If throwOnInvalidCV:  a parse exception will be thrown explaining the problem.
     *    If !throwOnInvalidCV: NULL is returned.
     *
     * @param dds the dds to search (likely the parent dataset)
     * @param dim  the dimension to search for a coorfinate variable for.
     * @param throwOnInvalidCV  whether to just return NULL or throw an exception if
     *                          a variable with name == dim.name is found but doesn't match.
     * @return
     */
    libdap::Array* findMatchingCoordinateVariable(
        const DDS& dds,
        const agg_util::Dimension& dim,
        bool throwOnInvalidCV=true) const;

    /**
     *  Called when we find an existing variable with the same name as the dim.name
     *  and the variable has NOT had values set on it yet (deferred).
     *
     *  We will:
     *    o Create the actual data for the coordinate variable as if there were
     *      no deferred variable at all
     *    o Ensure the type of placeholder elt and new var elts are the same or throw
     *    o Copy the metadata (AttrTable) in pBT into the new one
     *    o Remove pBT from the DDS since by definition it will be a scalar and not an Array type.
     *    o Add the newly created one to the dataset
     *    o Inform the dataset that the variables values are now valid.  This will REMOVE the entry
     *       since the object will be going away!!
     *    o Lookup the object ACTUALLY in the DDS and return it.
     *
     *  We throw if the element type of pBT is not the same as the automatic created array
     *  (which will be String or double as per ncml spec at this point).
     *
     * @param pBT  the variable to add values to
     * @param dim  the dimension information to add to pBT
     * @return  pBT cast to an Array if all goes well, else null.
     */
    libdap::Array* processDeferredCoordinateVariable(libdap::BaseType* pBT, const agg_util::Dimension& dim);

    /**
     * Use the list of datasets to create a new coordinate variable for the given dimension.
     * The coordinate variable will have the same name as the dimension, with its shape
     * as the dimension as well.
     *
     * The values will be taken from the netcdf@coordValue attributes on the child datasets,
     * if these values exist.  If they can be parsed as numbers, then the coordinate
     * variable will be an array of type double.  If not, then the unparsed string will
     * be used and the coord var will be an Array of type string.
     *
     * If the values do not exist, then the string netcdf@location will be used.
     * If any location is empty, then a name will be created referring to the index
     * of the dataset in question.
     *
     * The Array is not added to the DDS, just returned as new memory.
     *
     * @param dim the dimension to create the coord var for
     * @return a newly created Array with length() == dim.size() containing the
     *         values of the proper type.  (Either string or double now).
     */
    auto_ptr<libdap::Array> createCoordinateVariableForNewDimension(const agg_util::Dimension& dim) const;

    /**
     * Add functionality for createCoordinateVariableForNewDimension():
     * We call that function to make the proper new c.v.
     * and then add the variable to the current parent dataset's
     * DDS.
     * Since this copies the new var, we lookup the actual array in the DDS and return a
     * pointer to the actual one in the DDS.
     * NULL could be returned if an error, though most (or all)
     * error conditions will throw.
     * @param dds   the dds to add to (our parent dds)
     * @param dim  the dimension info for the new variable (including it's name!)
     * @return the new c.v. contained within dds or null if error.
     * */
    libdap::Array* createAndAddCoordinateVariableForNewDimension(libdap::DDS& dds, const agg_util::Dimension& dim);

    /**
     * Scans the child datasets and fills a new coordinate variable Array with the netcdf@coordValue from each.
     * If any dataset fails to specify the attribute, a parse error is thrown.
     *
     * If the first dataset's coordValue is a number, then the returned Array will be of type Float64.
     * If any subsequent datasets fail to be numbers, a parse error is thrown.
     *
     * If the first is not a number, then the return Array will be of type String and the attribute will
     * be passed in directly.
     *
     * @param dim  the dimension we're using to make the coord variable
     * @return the new Array with type string or double.  It will have length() == dim.size()
     * @exception parse error if any datasets fail to have coordValue set.
     * @exception parse error if the first dataset coord is a number but any others are not.
     */
    auto_ptr<libdap::Array> createCoordinateVariableForNewDimensionUsingCoordValue(const agg_util::Dimension& dim) const;
    auto_ptr<libdap::Array> createCoordinateVariableForNewDimensionUsingCoordValueAsDouble(const agg_util::Dimension& dim) const;
    auto_ptr<libdap::Array> createCoordinateVariableForNewDimensionUsingCoordValueAsString(const agg_util::Dimension& dim) const;

    /**
     * For each child dataset, use its netcdf@location as the coordValue.
     * If it is empty (ie virtual dataset) then use the symbol "Dataset_i"
     * where the 'i' will be the index of the dataset in parse order.
     * @param dim the dimension to create the coordinate variable for
     * @return the new Array of type String with the values in it.  It
     *          will have length() == dim.size().
     */
    auto_ptr<libdap::Array> createCoordinateVariableForNewDimensionUsingLocation(const agg_util::Dimension& dim) const;

    /** Add the attribute _CoordinateAxisType with value cat to the array pCV. */
    static void addCoordinateAxisType(libdap::Array& rCV, const std::string& cat);

    // Return the list of valid attribute names.
    static vector<string> getValidAttributes();

  private: // Data rep

    string _type; // required oneof { union | joinNew | joinExisting | forecastModelRunCollection | forecastModelSingleRunCollection }
    string _dimName;
    string _recheckEvery;

    // Our containing NetcdfElement, which must exist.  This needs to be a weak reference to avoid ref loop....
    NetcdfElement* _parent;

    // The vector of explicit, ordered NetcdfElement*.  We assume a STRONG reference to these
    // if they are in this container and we must deref() them on dtor.
    vector<NetcdfElement*> _datasets;

    // The vector of scan elements
    vector<ScanElement*> _scanners;

    // A vector containing the names of the variables to be aggregated in this aggregation.
    // Not used for union.
    vector<string> _aggVars;

    // If set, we want to create a new attribute _CoordinateAxisType
    // with this value on each aggVar.
    std::string _coordinateAxisType;

  };

}

#endif /* __NCML_MODULE__AGGREGATION_ELEMENT_H__ */

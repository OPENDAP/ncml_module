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
#ifndef __NCML_MODULE__NETCDF_ELEMENT_H__
#define __NCML_MODULE__NETCDF_ELEMENT_H__

#include "DDSLoader.h"
#include "NCMLElement.h"

using namespace std;

namespace libdap
{
  class DDS;
}

class BESDapResponse;

namespace ncml_module
{
  class AggregationElement;
  class DimensionElement;
  class NCMLParser;

  /**
   * @brief Concrete class for NcML <netcdf> element
   *
   * This element specifies the location attribute for the local
   * data file that we wrap and load into a DDX (DDS w/ AttrTable tree).
   *
   * We keep a ptr to our containing NCMLParser to help with
   * the differences needed if we are the root dataset element or not,
   * in particular that the response object is either passed in to us (root)
   * or loaded brandy new if we're the child of an aggregation.
   */
  class NetcdfElement : public NCMLElement
  {
  private:
    NetcdfElement& operator=(const NetcdfElement& rhs); //disallow

  public:
    static const string _sTypeName;
    static const vector<string> _sValidAttributes;

    NetcdfElement();
    NetcdfElement(const NetcdfElement& proto);
    virtual ~NetcdfElement();
    virtual const string& getTypeName() const;
    virtual NetcdfElement* clone() const; // override clone with more specific subclass
    virtual void setAttributes(const AttributeMap& attrs);
    virtual void handleBegin();
    virtual void handleContent(const string& content);
    virtual void handleEnd();
    virtual string toString() const;

    // Accessors for attributes we deal with.
    // TODO Add these as we support aggregation attributes
    const string& location() const { return _location; }
    const string& id() const { return _id; }
    const string& title() const { return _title; }

    /**
     * @return whether this is initialized properly and ready to be used.
     * It should be true after handleBegin() if all is well.
     */
    bool isValid() const;

    /**
     * Return the DDS for this dataset.
     */
    virtual libdap::DDS* getDDS() const;

    bool getProcessedMetadataDirective() const
    {
      return _gotMetadataDirective;
    }

    void setProcessedMetadataDirective()
    {
      _gotMetadataDirective = true;
    }

    /** Used by the NCMLParser to let us know to borrow the response
     * object and not own it.  Used for the root element only!
     * Nested datasets will create and own their own!
     */
    void borrowResponseObject(BESDapResponse* pResponse);

    /** Kind of superfluous, but tells this object to clear its reference
     * to pReponse, which had better match _response or we throw internal exception.
     */
    void unborrowResponseObject(BESDapResponse* pResponse);

    /**
     * Called if this is a member of an aggregation (i.e. not root)
     * to create a dynamic response object of the given type.
     * This call or borrowResponseObject() must be called before this is used.
     */
    void createResponseObject(DDSLoader::ResponseType type);

    /**
     * @return the DimensionElement with the given name in the
     * dimension table for THIS NetcdfElement only (no traversing
     * up the tree is allowed).  If not found, NULL is returned.
     */
    const DimensionElement* getDimensionInLocalScope(const string& name) const;

    /**
     * @return the first DimensionElement with name found by checking
     * getDimensionInLocalScope() starting at this element and traversing upwards
     * to the enclosing scopes (in case we're in an aggregation).
     * Return NULL if not found.
     * @Note this allows dimensions to be lexically "shadowed".
     * @Note Ultimately, shared dimensions _cannot_ be shadowed, so we will need to
     * make sure of this when we handle shared dimensions.
     */
    const DimensionElement* getDimensionInFullScope(const string& name) const;

    /** Add the given element to this scope.
     * We maintain a strong reference, so the caller should
     * respect the RCObject count and not delete it on us!
     */
    void addDimension(DimensionElement* dim);

    /** "Print" out the dimensions to a string */
    string printDimensions() const;

    /** Clear the dimension table, releasing all strong references */
    void clearDimensions();

    /** Get the list of dimension elements local to only this dataset, not its enclosing scope.  */
    const std::vector<DimensionElement*>& getDimensionElements() const;

    /** Set our aggregtation to the given agg.
     *
     * If there exists an aggregation already and !throwIfExists, agg will replace it,
     * which might cause the previous one to be deleted.
     *
     * If there exists one already and agg != NULL and throwIfExists, an exception will be thrown.
     *
     * If agg == NULL, it always removes the strong reference to the previous, regardless of throwIfExists.
     */
    void setChildAggregation(AggregationElement* agg, bool throwIfExists=true);

    /** Return the raw pointer (or NULL) to our contained aggregation.
     * Only guaranteed valid for the life of this object.  */
    AggregationElement* getChildAggregation() const { return _aggregation.get(); }

    /** Return the next enclosing dataset for this, or NULL if we're the root.
     * Basically traverse upwards through any aggregation parent to get containing datset.
     */
    NetcdfElement* getParentDataset() const;

    /** @return the AggregationElement that contains me, or NULL if we're root. */
    AggregationElement* getParentAggregation() const;

    /** Set my parent AggregationElement to parent. This is a weak reference.  */
    void setParentAggregation(AggregationElement* parent);

  private:

    /** Ask the parser to load our location into our response object. */
    void loadLocation(NCMLParser& p);

    /** Check the value of the attribute fields and if any are
     * !empty() that we don't support, throw a parse error to tell the author.
     */
    void throwOnUnsupportedAttributes();

    static vector<string> getValidAttributes();

  private:
    string _location;
    string _id;
    string _title;
    string _ncoords;
    string _enhance;
    string _addRecords;
    string _coordValue;
    string _fmrcDefinition;

    // Whether we got a metadata direction element { readMetadata | explicit } for this node yet.
    // Just used to check for more than one.
    bool _gotMetadataDirective;

    // Whether we own the memory for _response and need to destroy it in dtor or not.
    bool _weOwnResponse;

    // Our response object
    // We OWN it if we're not the root element,
    // but the parser owns it if we are the root.
    // The parser sets _weOwnResponse correctly for our dtor.
    BESDapResponse* _response;

    // If non-null, a pointer to the singleton aggregation element for this dataset.
    // We use an RCPtr to automatically ref() it when we set it
    // and to unref() it in our dtor.
    RCPtr<AggregationElement> _aggregation;

    // If we are nested within an aggregation element,
    // this is a back ptr we can use to traverse upwards.
    // If it null if we're a root dataset.
    AggregationElement* _parentAgg;

    // A table of strong references to Dimensions valid for this element's lexical scope.
    // We use raw DimensionElement*, but ref() them upon adding them to the vector
    // and unref() them in the dtor.
    // We won't have that many, so a vector is more efficient than a map for this.
    std::vector< DimensionElement* > _dimensions;
  };

}

#endif /* __NCML_MODULE__NETCDF_ELEMENT_H__ */

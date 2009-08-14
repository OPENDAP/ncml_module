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
#ifndef __NCML_MODULE__NCMLARRAY_H__
#define __NCML_MODULE__NCMLARRAY_H__

#include <libdap/dods-datatypes.h>
#include <libdap/Array.h>
#include <libdap/Vector.h>
#include "NCMLDebug.h"
#include "Shape.h"
#include <sstream>
#include <string>
#include <vector>

using libdap::Array;
using libdap::dods_byte;
using libdap::dods_int16;
using libdap::dods_uint16;
using libdap::dods_int32;
using libdap::dods_uint32;
using libdap::dods_float32;
using libdap::dods_float64;

using std::string;
using std::vector;

namespace ncml_module
{
  class Shape;

  /**
   * @brief A parameterized subclass of libdap::Array that allows us to apply constraints on
   * NcML-specified data prior to serialization.  All the code is in the .h, so no .cc is defined.
   *
   * This class caches the full set of data values of basic type T for the unconstrained
   * super Array shape.  It forces read_p() to ALWAYS be false to force a call of this->read()
   * prior to any serialization of super Vector's data buffer.
   *
   * In a read() call, if constraints have been applied to the Array superclass, this class
   * will effect the Vector superclass data buffer (Vector._buf) in order to calculate
   * and store the constrained data into Vector._buf so serialize() passes the constrained
   * data properly.  It maintains a copy of the full set of data, however, in case constraints
   * are later removed or changed.  It maintains a copy of the constrained shape used to generate
   * the current Vector._buf so that on subsequent read() calls it can check to see if the constraints
   * have changed and if so recompute Vector._buf.
   *
   * We use a template on the underlying data type stored, such as dods_byte, dods_int32, etc.
   * Note that this can ALSO be std::string, in which case Vector does special processing.  We
   * need to be aware of this in processing data.
   */
template <typename T>
class NCMLArray : public libdap::Array
{
public:
  NCMLArray()
    : Array("",0)
    , _allValues(0)
    , _noConstraints(0)
    , _currentConstraints(0)
    {
    }

  NCMLArray(const string& name)
  : Array(name,0)
  , _allValues(0)
  , _noConstraints(0)
  , _currentConstraints(0)
  {
  }

  NCMLArray(const NCMLArray<T>& proto)
  : Array(proto)
  , _allValues(0)
  , _noConstraints(0)
  , _currentConstraints(0)
  {
    copyLocalRepFrom(proto);
  }

  /** Destroy any locally cached data */
  virtual ~NCMLArray()
  {
    destroy(); // helper
  }

  NCMLArray&
  operator=(const NCMLArray<T>& rhs)
  {
    if (&rhs == this)
      {
        return *this;
      }

    // Call the super assignment
    Array::operator=(rhs);

    // Copy local private rep
    copyLocalRepFrom(rhs);

    return *this;
  }

  /** Override virtual constructor, deep clone */
  virtual NCMLArray<T>* ptr_duplicate()
  {
    return new NCMLArray(*this);
  }

  /** Override to return false if we have uncomputed constraints
   * and only true if the current constraints match the Vector value buffer.
   */
  virtual bool read_p()
  {
    // If we haven't computed constrained buffer yet, or they changed,
    // we must call return false to force read() to be called again.
    return !haveConstraintsChangedSinceLastRead();
  }

  /** Override to disable setting of this flag.  We will leave it false
   * unless the constraints match the Vector value buffer.
   */
  virtual void set_read_p(bool /* state */)
  {
    // Just drop it on the floor we compute it
    // Array::set_read_p(state);
  }

  /**
   * If there are no constraints and this is the first call to read(),
   * we will do nothing, assuming the sueprclasses have everything under control.
   *
   * If there are constraints, this function will create the correct
   * buffer in Vector with the constrained data, generated from cached
   * local values gathered to be from the unconstrained state.
   *
   * The first call to read() will assume the CURRENT Vector buffer
   * has ALL values (unconstrained) and store a local copy before
   * generating a Vector buffer for the current constraints.
   *
   * After this call, the caller can be assured that the Vector's data buffer
   * has properly constrained data matching the current super Array's constraints.
   * Subsequent calls to read() will see if the constraints used to create the
   * Vector data buffer have changed and if so recompute a new Vector buffer from
   * the locally cached values.
   */
  virtual bool read()
  {
    BESDEBUG("ncml", "NCMLArray::read() called!" << endl);

    // If we haven't cached yet, but there's no constraints on the super,
    // then the super has the right data and we have nothing to do.
    // No reason to make copy or do work if there's no constraints!
    if (!_allValues && !isConstrained())
      {
        BESDEBUG("ncml", "NCMLArray<T>::read() called, but no constraints.  Assuming data buffer is correct.");
        return true;
      }

    // If first call, cache the full dataset.  Throw if there's an error with this.
    cacheSuperclassStateIfNeeded();

    // If _currentConstraints is null or different than current Array dimensions,
    // compute the constrained data buffer from the local data cache and the current Array dimensions.
    if (haveConstraintsChangedSinceLastRead())
      {
        // Enumerate and set the constrained values into Vector super..
        createAndSetConstrainedValueBuffer();

        // Copy the constraints we used to generate these values
        // so we know if we need to redo this in another call to read() or not.
        cacheCurrentConstraints();
      }
    return true;
  }

  /////////////////////////////////////////////////////////////
  // We have to override these to make a copy in this subclass as well since constraints added before read().
  // TODO Consider instead allowing add_constraint() in Array to be virtual so we can catch it and cache at that point rather than
  // all the time!!

  // Helper macros to avoid a bunch of cut & paste

#define NCMLARRAY_CHECK_ARRAY_TYPE_THEN_CALL_SUPER(arrayValue, sz)  \
  if (typeid(arrayValue) != typeid(T*)) \
  { \
    THROW_NCML_INTERNAL_ERROR("NCMLArray<T>::set_value(): got wrong type of value array, doesn't match type T!"); \
  } \
  bool ret = Vector::set_value((arrayValue), (sz)); \
  cacheSuperclassStateIfNeeded(); \
  return ret;

#define NCMLARRAY_CHECK_VECTOR_TYPE_THEN_CALL_SUPER(vecValue, sz) \
  if (typeid(vecValue) != typeid(vector<T>&)) \
  { \
    THROW_NCML_INTERNAL_ERROR("NCMLArray<T>::setValue(): got wrong type of value vector, doesn't match type T!"); \
  } \
  bool ret = Vector::set_value((vecValue), (sz)); \
  cacheSuperclassStateIfNeeded(); \
  return ret;

  virtual bool set_value(dods_byte *val, int sz)
  {
    NCMLARRAY_CHECK_ARRAY_TYPE_THEN_CALL_SUPER(val, sz);
  }

  virtual bool set_value(vector<dods_byte> &val, int sz)
  {
    NCMLARRAY_CHECK_VECTOR_TYPE_THEN_CALL_SUPER(val, sz);
  }

  virtual bool set_value(dods_int16 *val, int sz)
  {
    NCMLARRAY_CHECK_ARRAY_TYPE_THEN_CALL_SUPER(val, sz);
  }

  virtual bool set_value(vector<dods_int16> &val, int sz)
  {
    NCMLARRAY_CHECK_VECTOR_TYPE_THEN_CALL_SUPER(val, sz);
  }

  virtual bool set_value(dods_uint16 *val, int sz)
  {
    NCMLARRAY_CHECK_ARRAY_TYPE_THEN_CALL_SUPER(val, sz);
  }

  virtual bool set_value(vector<dods_uint16> &val, int sz)
  {
    NCMLARRAY_CHECK_VECTOR_TYPE_THEN_CALL_SUPER(val, sz);
  }

  virtual bool set_value(dods_int32 *val, int sz)
  {
    NCMLARRAY_CHECK_ARRAY_TYPE_THEN_CALL_SUPER(val, sz);
  }

  virtual bool set_value(vector<dods_int32> &val, int sz)
  {
    NCMLARRAY_CHECK_VECTOR_TYPE_THEN_CALL_SUPER(val, sz);
  }

  virtual bool set_value(dods_uint32 *val, int sz)
  {
    NCMLARRAY_CHECK_ARRAY_TYPE_THEN_CALL_SUPER(val, sz);
  }

  virtual bool set_value(vector<dods_uint32> &val, int sz)
  {
    NCMLARRAY_CHECK_VECTOR_TYPE_THEN_CALL_SUPER(val, sz);
  }

  virtual bool set_value(dods_float32 *val, int sz)
  {
    NCMLARRAY_CHECK_ARRAY_TYPE_THEN_CALL_SUPER(val, sz);
  }

  virtual bool set_value(vector<dods_float32> &val, int sz)
  {
    NCMLARRAY_CHECK_VECTOR_TYPE_THEN_CALL_SUPER(val, sz);
  }

  virtual bool set_value(dods_float64 *val, int sz)
  {
    NCMLARRAY_CHECK_ARRAY_TYPE_THEN_CALL_SUPER(val, sz);
  }

  virtual bool set_value(vector<dods_float64> &val, int sz)
  {
    NCMLARRAY_CHECK_VECTOR_TYPE_THEN_CALL_SUPER(val, sz);
  }

  virtual bool set_value(string *val, int sz)
  {
    NCMLARRAY_CHECK_ARRAY_TYPE_THEN_CALL_SUPER(val, sz);
  }

  virtual bool set_value(vector<string> &val, int sz)
  {
    NCMLARRAY_CHECK_VECTOR_TYPE_THEN_CALL_SUPER(val, sz);
  }

#undef NCMLARRAY_CHECK_ARRAY_TYPE_THEN_CALL_SUPER
#undef NCMLARRAY_CHECK_VECTOR_TYPE_THEN_CALL_SUPER

protected:

  /** Get the current dimensions of our superclass Array as a Shape object. */
  Shape getSuperShape() const
  {
    // make the Shape for our superclass Array
    return Shape(*this);
  }

  /**
   * Return whether the superclass Array has been constrained along
   * any dimensions.
   */
  bool isConstrained() const
  {
    Shape superShape = getSuperShape();
    return superShape.isConstrained();
  }

  /** Return whether the constraints used to create Vector._buf for the last read()
   * have changed, meaning we need to recompute Vector._buf using the new values.
   */
  bool haveConstraintsChangedSinceLastRead() const
  {
    // If there's none, then they've changed by definition.
    if (!_currentConstraints)
      {
        return true;
      }
    else // compare the current values to those currently in our Array slice
      {
        return ((*_currentConstraints) == getSuperShape());
      }
  }

  /** Store the current super Array shape as the current constraints so we remember */
  void cacheCurrentConstraints()
  {
    // If got some already, blow them away...
    if (_currentConstraints)
      {
        delete _currentConstraints; _currentConstraints = 0;
      }
    _currentConstraints = new Shape(*this);
    BESDEBUG("ncml", "NCMLArray: Cached current constraints:" << (*_currentConstraints) << endl);
  }

  /**
   * The first time we get a read(), we want to grab the current state of the superclass Array and Vector
   * and cache them locally before we apply any needed constraints.
   * ASSUME: that the current value of the _buf in Vector super is the FULL set of values for the UNCONSTRAINED Array.
   * We also grab the dimensions from the super Array, but realize that these may be already constrained.
   * For this reason, we cache the UNCONSTRAINED shape (i.e. the d.size() for all dimensions) as well
   * as the full set of current dimensions, constrained or not.  The latter tells of if constraints have
   * been applied and if we need to compute a new _buf in read().
   */
  void cacheSuperclassStateIfNeeded()
  {
    // We had better have a template or else the width() calls will be wrong.
    NCML_ASSERT(var());

    // First call, make sure we grab unconstrained state.
    if (!_noConstraints)
      {
        cacheUnconstrainedDimensions();
      }

    // If we haven't gotten this yet, go get it,
    // assuming the super Vector contains all values
    if (!_allValues)
      {
        BESDEBUG("ncml", "NCMLArray<T>:: we don't have unconstrained values cached, caching from Vector now..." << endl);
        unsigned int spaceSize = _noConstraints->getUnconstrainedSpaceSize();
        // These must match or we're not getting all the data and the caller messed up.
        NCML_ASSERT_MSG(static_cast<unsigned int>(length()) == spaceSize,
            "NCMLArray expected superclass Vector length() to be the same as unconstrained space size, but it wasn't!");
        // Make new default storage with enough space for all the data.
        _allValues = new vector<T>(spaceSize);
        NCML_ASSERT(_allValues->size() == spaceSize); // the values should all be default for T().
        // Grab the address of the start of the vector memory block.
        // This is safe since vector memory is required to be contiguous
        T* pFirstElt = &((*_allValues)[0]);
        // Now overwrite the defaults in from the buffer.
        unsigned int stored = buf2val(reinterpret_cast<void**>(&pFirstElt));
        NCML_ASSERT((stored/sizeof(T)) == spaceSize); // make sure it did what it was supposed to do.
        // OK, we have our copy now!
      }

    // We ignore the current constraints since we don't worry about them until later in read().
  }

  void cacheUnconstrainedDimensions()
  {
    // We already got it...
    if (_noConstraints)
      {
        return;
      }

    // Copy from the super Array's current dimensions and force values to define an unconstrained space.
    _noConstraints = new Shape(*this);
    _noConstraints->setToUnconstrained();

    BESDEBUG("ncml", "NCMLArray: cached unconstrained shape=" << (*_noConstraints) << endl);
  }

  /**
   * If we have constraints that are not the same as the constraints
   * on the last read() call (or if this is first read() call), use
   * the super Array's current constrained dimension values to set Vector->val2buf()
   * with the proper constrained data.
   * ASSUMES: cacheSuperclassStateIfNeeded() has already been called once so
   * that this instance's state contains all the unconstrained data values and shape.
   */
  void createAndSetConstrainedValueBuffer()
  {
    BESDEBUG("ncml", "NCMLArray<T>::createAndSetConstrainedValueBuffer() called!" << endl);

    // Whether to validate, depending on debug build or not.
 #ifdef NDEBUG
     const bool validateBounds = false;
 #else
     const bool validateBounds = true;
 #endif

    // These need to exist or caller goofed.
    VALID_PTR(_noConstraints);
    VALID_PTR(_allValues);

    // This reflects the current constraints, so is what we want.
    unsigned int numVals = length();
    vector<T> values; // Exceptions may wind through and I want this storage cleaned, so vector<T> rather than T*.
    values.reserve(numVals);

    // Our current space, with constraints
    const Shape shape = getSuperShape();
    Shape::IndexIterator endIt = shape.endSpaceEnumeration();
    Shape::IndexIterator it;
    unsigned int count=0;  // just a counter for number of points for sanity checking
    for (it = shape.beginSpaceEnumeration(); it != endIt; ++it, ++count)
      {
        // Take the current point in constrained space, look it up in cached 3values, set it as next elt in output
        values.push_back( (*_allValues)[_noConstraints->getRowMajorIndex(*it, validateBounds)] );
      }

    // Sanity check the number of points we added.  They need to match or something is wrong.
    if (count != static_cast<unsigned int>(length()))
      {
        stringstream msg;
        msg << "While adding points to hyperslab buffer we got differing number of points "
        "from Shape space enumeration as expected from the constraints! "
        "Shape::IndexIterator produced " << count << " points but we expected " << length();
        THROW_NCML_INTERNAL_ERROR(msg.str());
      }

    if (count != shape.getConstrainedSpaceSize())
      {
        stringstream msg;
        msg << "While adding points to hyperslab buffer we got differing number of points "
        "from Shape space enumeration as expected from the shape.getConstrainedSpaceSize()! "
        "Shape::IndexIterator produced " << count << " points but we expected " << shape.getConstrainedSpaceSize();
        THROW_NCML_INTERNAL_ERROR(msg.str());
      }

    // Otherwise, we're good, so blast the values into the valuebuffer.
    // tell it to reuse the current buffer since by definition is has enough room since it holds all points to start.
    val2buf(static_cast<void*>(&(values[0])), true);
  }

private: // This class ONLY methods

  /** Copy in this the local data (private rep) in proto
   * Used by ptr_duplicate() and copy ctor */
  void copyLocalRepFrom(const NCMLArray<T>& proto)
  {
    // Avoid unnecessary finagling
    if (&proto == this)
      {
        return;
      }

    // Blow away any old data before copying new
    destroy();

    // If there are values, make a copy of the vector.
    if (proto._allValues)
      {
        _allValues = new vector<T>(*(proto._allValues));
      }

    if (proto._noConstraints)
      {
        _noConstraints = new Shape(*(proto._noConstraints));
      }

    if (proto._currentConstraints)
      {
        _currentConstraints = new Shape(*(proto._currentConstraints));
      }
  }

  /** Helper to destroy all the local data to pristine state. */
  void destroy() throw ()
  {
    delete _allValues; _allValues=0;
    delete _noConstraints; _noConstraints=0;
    delete _currentConstraints; _currentConstraints=0;
  }

private:

  // The unconstrained data set, cached from super in first call to cacheSuperclassStateIfNeeded()
  // if it is null.
  std::vector<T>* _allValues;

  // The Shape for the case of NO constraints on the data, or null if not set yet.
  Shape* _noConstraints;

  // The Shape for the CURRENT dimensions in super Array, used to calculate the transmission buffer
  // for read() and also to check if haveConstraintsChangedSinceLastRead().  Null if not set yet.
  Shape* _currentConstraints;

}; // class NCMLArray<T>

} // namespace ncml_module

#endif /* __NCML_MODULE__NCMLARRAY_H__ */

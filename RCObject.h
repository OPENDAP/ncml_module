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
#ifndef __NCML_MODULE__REF_COUNTED_OBJECT_H__
#define __NCML_MODULE__REF_COUNTED_OBJECT_H__

#include <set>
#include <string>

namespace ncml_module
{
  class RCOBjectPool;
  class RCObject;

  typedef std::set<RCObject*> RCObjectSet;

  /**
   * A monitoring "pool" class for created RCObject's which allows us to
   * forcibly delete any RCOBject's regardless of their ref counts
   * when we know we are done with them, say after an exception.
   *
   * @TODO For now, this won't be a real pool, where objects are pre-allocated
   * ahead of time and reused, etc.  Support for this sort of thing will
   * require a template class containing a vector of reusable objects
   * for each type we need to factory up.  At that point this will serve
   * as a base class for the concrete pool.
   */
  class RCObjectPool
  {
    friend class RCObject;

public:
    /** Create an empty pool */
    RCObjectPool();

    /** Forcibly delete all remaining objects in pool, regardless of ref count */
    virtual ~RCObjectPool();

    /** @return whether the pool is currently monitoring the object
     * or not.
     */
    bool contains(RCObject* pObj) const;

    /** Add the object to the pool uniquely.
      * When the pool is destroyed, pObj will be destroyed
      * as well, regardless of its count, if it is still live.
      */
    void add(RCObject* pObj);

    /**
     * Tell the pool that the object's count is 0 and it can be released to be
     * deleted or potentially reused again later.  This method is protected
     * since we don't want users calling this, but only subclasses and our friend the RCObject itself.
     */
    void release(RCObject* pObj);

protected:
    /** Call delete on all objects remaining in _liveObjects and clear it out.
     * After call, _liveObjects.empty().
     */
    void deleteAllObjects();

private:

    // A set of the live, monitored objects.
    // Lookups are log(N) and entries unique, as required.
    // All entries in this list will be delete'd in the dtor.
    RCObjectSet _liveObjects;
  };

  /**
   * @brief A base class for a simple reference counted object.
   *
   * Use as a base class for objects that need to delete themselves
   * when their reference count goes to 0.
   *
   * When a strong reference to the object is required, the
   * caller uses ref().  When the reference needs to be released,
   * unref() is called.  p->unref() should be considered potentially
   * identical to delete p; since it can cause the object to be deleted.
   * The pointer should NOT be used after an unref() unless it was known
   * to be preceded by a ref(), or unless the count is checked prior to unref()
   * and found to be > 1.
   *
   * A new RCObject has a count of 0, and will only be destroyed automatically
   * if the count goes from 1 back to 0, so the caller is in charge of it unless the first
   * ref() call.  Be careful storing these in std::auto_ptr!  Instead, use a
   * RCObjectRef(new RCObject()) in place of auto_ptr for hanging onto
   * an RCOBject* in a local variable before possible early exit.
   *
   * @see RCPtr which can be used as a temporary
   * reference similar to std::auto_ptr<T>, but which uses the
   * reference counting system to safely handle a RCObject
   * as a temporary in a location where an exception might cause it to be
   * leaked.  This is especially useful when the object is removed from
   * a refcounted container but safely needs to be used locally before destruction.
   *
   * @note This class influenced by Scott Meyers and Open Inventor ref counting stuff.
   *
   * @note I'd almost rather use boost::shared_ptr and boost::weak_ptr
   * for this stuff since they can be used in STL containers and
   * are thread-safe, but adding a boost dependency for just shared_ptr
   * seems a bit like overkill now.  If we add boost in the future,
   * consider changing this over!!
   *
   * @TODO Consider adding a pointer to an abstract MemoryPool or what have you
   * so that a Factory can implement the interface and these objects can be stored
   * in a list as well as returned from factory.  That way the factory can forcibly
   * clear all dangling references from the pool in its dtor in the face of exception
   * unwind or programmer ref counting error.
   */
  class RCObject
  {
    friend class RCObjectPool;
  private:
    RCObject& operator=(const RCObject& rhs); //disallow

  public:
    /** If the pool is given, the object will be released back to the pool when its count hits 0,
     * otherwise it will be deleted.
     */
    RCObject(RCObjectPool* pool=0);

    /** Copy ctor: Starts count at 0 and adds us to the proto's pool
     * if it exists.
     */
    RCObject(const RCObject& proto);

    virtual ~RCObject();

    /** Increase the reference count by one.
     * const since we do not consider the ref count part of the semantic constness of the rep */
    int ref() const;

    /** Decrease the reference count by one.  If it goes from 1 to 0,
     * delete this and this is no longer valid.
     * @return the new ref count.  If it is 0, the called knows the
     * object was deleted.
     *
     * It is illegal to unref() an object with a count of 0.  We don't
     * throw to allow use in dtors, so the caller is to not do it!
     *
     * const since the reference count is not part of the semantic constness of the rep
     */
    int unref() const throw();

    /** Get the current reference count */
    int getRefCount() const;

    /** Just prints the count and address  */
    virtual std::string toString() const;

  private: // interface

    /** Same as toString(), just not virtual so we can always use it. */
    std::string printRCObject() const;

  private: // data rep

    // The reference count... mutable since we want to ref count const objects as well,
    // and the count doesn't affect the semantic constness of the subclasses.
    mutable int _count;

    // If not null, the object is from the given pool and should be release()'d to the
    // pool when count hits 0, not deleted.  If null, it can be deleted.
    RCObjectPool* _pool;
  };

  /** @brief A reference to an RCObject which automatically ref() and deref() on creation and destruction.
   *
   * Use this for temporary references to an RCObject* instead of std::auto_ptr to avoid leaks or double deletion.
   *
   * For example,
   *
   * RCPtr<RCObject> obj = RCPtr<RCObject>(new RCObject());
   * // count is now 1.
   * // make a call to add to container that might throw exception.
   * // we assume the container will up the ref() itself on a successful addition.
   * addToContainer(obj.get());
   * // if previous line exceptions, ~RCPtr will unref() it back to 0, causing it to delete.
   * // if we get here, the object is safely in the container and has been ref() to 2,
   * // so ~RCPtr correctly drops it back to 0.
   *
   * @note We don't do weak references, so make sure to not generate reference loops with back pointers!!
   * Back pointers should be raw pointers (until/unless we add weak references) and ref() should only
   * be called when it's a strong (ownership) reference!
   */
  template <class T>
  class RCPtr
  {
  public:
    RCPtr(T* pRef = 0)
    {
      _obj = pRef;
      init();
    }

    RCPtr(const RCPtr& from)
    : _obj(from._obj)
    {
      init();
    }

    ~RCPtr()
    {
      if (_obj)
        {
          _obj->unref();
          _obj = 0;
        }
    }

    RCPtr&
    operator=(const RCPtr& rhs)
    {
      if (rhs._obj != _obj)
        {
         // keep it around for a deref
          RCObject* oldObj = _obj;

          // Grab the rhs object and up the ref.
          _obj = rhs._obj;
          init();

          // We just dropped an old reference, so unref() is not null.
          if (oldObj)
            {
              oldObj->unref();
            }
        }
      return *this;
    }

    T&
    operator*() const
    {
      // caller is on their own if they deref this as null,
      // so should check with get() first.
      return *_obj;
    }

    T*
    operator->() const
    {
      return _obj;
    }

    T*
    get() const
    {
      return _obj;
    }

    /**
     * If not null, ref() the object and then return it.
     *
     * Useful for adding a reference to a
     * container, e.g.:
     *
     * RCPtr<T> myObj;
     * vector<T> myVecOfObj;
     * myVecOfObj.push_back(myObj.refAndGet());
     *
     * @return
     */
    T*
    refAndGet() const
    {
      if (_obj)
        {
          _obj->ref();
        }
      return _obj;
    }

  private:
    void init()
    {
      if (_obj)
        {
          _obj->ref();
        }
    }

  private:
    T* _obj;
  };

}

#endif /* __NCML_MODULE__REF_COUNTED_OBJECT_H__ */

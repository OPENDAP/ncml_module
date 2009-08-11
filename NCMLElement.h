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
#ifndef __NCML_MODULE__NCMLELEMENT_H__
#define __NCML_MODULE__NCMLELEMENT_H__

#include <iostream>
#include <memory>
#include "SaxParser.h" // for AttrMap
#include <string>
#include <vector>

namespace ncml_module
{
  // FDecls
  class NCMLParser;

  /**
   *  @brief Base class for NcML element concrete classes
   *
   *  Base class to define an NcML element for polymorphic dispatch
   *  in the NCMLParser.  These concrete elements classes will be
   *  made friends of the NCMLParser in order to split the monolithic
   *  parser into chunks with specific functionality.
   *
   *  New concrete subclasses MUST be entered in the factory or they cannot
   *  be created.
   */
  class NCMLElement
  {
  public:

    /**
     *  Singleton factory class for the NcML elements.
     *  Assumption: Concrete subclasses MUST
     *  define the following static methods:
     *  static const string& ConcreteClassName::getTypeName();
     *  static ConcreteClassName* ConcreteClassName::makeInstance(const AttrMap& attrs);
     * */
    class Factory
    {
    private: // only the static can create the singleton
      Factory() : _protos() {}

    public:
      ~Factory();

      /** Get the singleton */
      static Factory& getTheFactory();

      /**
       * Create an element of the proper type with the given AttrMap
       * for its defined attributes.
       * @return the new element or NULL if eltTypeName had to prototype.
       * @param eltName element type name
       * @param attrs the map of the attributes defined for the element
       */
      std::auto_ptr<NCMLElement> makeElement(const std::string& eltTypeName, const AttributeMap& attrs);

    private:
      // Possible prototypes we can create from.  Uses getTypeName() for match.
      typedef std::vector<const NCMLElement*> ProtoList;

      /** Add the prototype subclass as an element we can factory up!
       * Used by createTheFactory to create the initial factory.
       * If a prototype with type name == proto->getTypeName() already
       * exists in the factory, proto will replace it.
       */
      void addPrototype(const NCMLElement* proto);

      /** Return the iterator for the prototype for elementTypeName, or _protos.end() if not found. */
      ProtoList::iterator findPrototype(const std::string& elementTypeName);

      /** Create the singleton into _sInstance
       * and populate it with concrete subclasses
       */
      static void createTheFactory();

      // Singleton
      static Factory*_sInstance;


      ProtoList _protos;
    };

  protected: // data rep

  protected:
    // Abstract: Only subclasses can create these
    NCMLElement() {}

  public:
    virtual ~NCMLElement() {}

    /** Return the type of the element, which should be:
     * the same as ConcreteClassName::getTypeName() */
    virtual const std::string& getTypeName() const = 0;

    /** Make and return a copy of this.
     * Used by the factory from a prototype.
     */
    virtual NCMLElement* clone() const = 0;

    /** Set the attributes of this from the map.
     * @param attrs the attribute map to set this class to.
     */
    virtual void setAttributes(const AttributeMap& attrs) = 0;

    /** Handle a begin on this element.
     * Called after creation and it is assumed the
     * attributes are already set.
     * @param p the parser to effect.
     * */
    virtual void handleBegin(NCMLParser& p) = 0;

    /** Handle the characters content for the element.
     * @param p the parser to effect.
     * @param content the string of characters in the element content.
     */
    virtual void handleContent(NCMLParser& p, const std::string& content) = 0;

    /** Handle the closing of this element.
     * @param p the parser to effect
     * */
    virtual void handleEnd(NCMLParser& p) = 0;

    /** Return a string describing the element */
    virtual std::string toString() const = 0;

    /** Helper for subclasses implementing toString().
     * @return a string with attrName="attrValue" if !attrValue.empty(),
     *         otherwise return the empty string.
     */
    static std::string printAttributeIfNotEmpty(const std::string& attrName, const std::string& attrValue);
  };

}

/** Output obj.toString() to the stream */
inline std::ostream &
operator<<( std::ostream &strm, const ncml_module::NCMLElement &obj )
{
    strm << obj.toString();
    return strm ;
}


#endif /* __NCML_MODULE__NCMLELEMENT_H__ */

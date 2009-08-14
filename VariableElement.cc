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
#include "VariableElement.h"

#include "Array.h"
#include "BaseType.h"
#include <ctype.h>
#include "DimensionElement.h"
#include "dods-limits.h"
#include <memory>
#include "MyBaseTypeFactory.h"
#include "NCMLDebug.h"
#include "NCMLParser.h"
#include "NCMLUtil.h"
#include <sstream>

using namespace libdap;
using std::vector;
using std::auto_ptr;

namespace ncml_module
{

  const string VariableElement::_sTypeName = "variable";

  VariableElement::VariableElement()
  : _name("")
  , _type("")
  , _shape("")
  , _orgName("")
  , _shapeTokens()
  {
  }

  VariableElement::VariableElement(const VariableElement& proto)
  : NCMLElement()
  {
    _name = proto._name;
    _type = proto._type;
    _shape = proto._shape;
    _orgName = proto._orgName;
    _shapeTokens = proto._shapeTokens;
  }

  VariableElement::~VariableElement()
  {
    // help clear up memory
    _shapeTokens.resize(0);
    _shapeTokens.clear();
  }

  const string&
  VariableElement::getTypeName() const
  {
    return _sTypeName;
  }

  VariableElement*
  VariableElement::clone() const
  {
    return new VariableElement(*this);
  }

  void
  VariableElement::setAttributes(const AttributeMap& attrs)
  {
    _name = NCMLUtil::findAttrValue(attrs, "name");
    _type = NCMLUtil::findAttrValue(attrs,"type", "");
    _shape = NCMLUtil::findAttrValue(attrs, "shape", "");
    _orgName = NCMLUtil::findAttrValue(attrs, "orgName", "");
  }

  void
  VariableElement::handleBegin(NCMLParser& p)
  {
    processBegin(p);
  }

  void
  VariableElement::handleContent(NCMLParser& /* p */, const string& content)
  {
    // Variables cannot have content like attribute.  It must be within a <values> element.
    if (!NCMLUtil::isAllWhitespace(content))
      {
        THROW_NCML_PARSE_ERROR("Got non-whitespace for element content and didn't expect it.  Element=" + toString() + " content=\"" +
            content + "\"");
      }
  }

  void
  VariableElement::handleEnd(NCMLParser& p)
  {
    processEnd(p);
  }

  string
  VariableElement::toString() const
  {
    return      "<" + _sTypeName +
                " name=\"" + _name + "\"" +
                " type=\"" + _type + "\"" +
                ((!_shape.empty())?(" shape=\"" + _shape + "\""):("")) +
                ((!_orgName.empty())?(" orgName=\"" + _orgName + "\""):("")) +
                ">";
  }


  ////////////////// NON PUBLIC IMPLEMENTATION

  void
  VariableElement::processBegin(NCMLParser& p)
  {
    BESDEBUG("ncml", "VariableElement::handleBegin called for " << toString() << endl);

    if (!p.withinNetcdf())
      {
        THROW_NCML_PARSE_ERROR("Got element " + toString() + " while not in <netcdf> node!");
      }

    // Can only specify variable globally or within a composite variable now.
    if (!(p.isScopeGlobal() || p.isScopeCompositeVariable()))
      {
        THROW_NCML_PARSE_ERROR("Got <variable> element while not within a <netcdf> or within variable container.  scope=" +
            p.getScopeString());
      }

    // Make sure we've done this at least once before we continue.
    p.processMetadataDirectiveIfNeeded();

    // If a request to rename the variable
    if (!_orgName.empty())
      {
        processRenameVariable(p);
      }
    else // Otherwise see if it's an existing or new variable _name at scope of p
      {
        // Lookup the variable in whatever the parser's current variable container is
        // this could be the DDS top level or a container (constructor) variable.
        BaseType* pVar = p.getVariableInCurrentVariableContainer(_name);
        if (!pVar)
          {
            processNewVariable(p);
          }
        else
          {
            processExistingVariable(p, pVar);
          }
      }
  }

  void
  VariableElement::processEnd(NCMLParser& p)
  {
    BESDEBUG("ncml", "VariableElement::handleEnd called at scope:" << p.getScopeString() << endl);
    if (!p.isScopeVariable())
      {
        THROW_NCML_PARSE_ERROR("VariableElement::handleEnd called when not parsing a variable element!  Scope=" + p.getTypedScopeString());
      }

    NCML_ASSERT_MSG(p.getCurrentVariable(), "Error: VariableElement::handleEnd(): Expected non-null parser.getCurrentVariable()!");

    // Pop up the stack from this variable
    exitScope(p);

    BaseType* pVar = p.getCurrentVariable();
    BESDEBUG("ncml", "Variable scope now with name: " << ((pVar)?(pVar->name()):("<NULL>")) << endl);
  }

  void
  VariableElement::processExistingVariable(NCMLParser& p, BaseType* pVar)
  {
    BESDEBUG("ncml", "VariableElement::processExistingVariable() called with name=" << _name << " at scope=" << p.getTypedScopeString() << endl);

    // If undefined, look it up
    if (!pVar)
      {
        pVar = p.getVariableInCurrentVariableContainer(_name);
      }

    // It better exist by now
    VALID_PTR(pVar);

    // Make sure the type matches.  NOTE:
    // We use "Structure" to mean Grid, Structure, Sequence!
    // Also type="" will match ANY type.
    // TODO This fails on Array as well due to NcML making arrays be a basic type with a non-zero dimension.
    // We're gonna ignore that until we allow addition of variables, but let's leave this check here for now
    if (!_type.empty() && !p.typeCheckDAPVariable(*pVar, p.convertNcmlTypeToCanonicalType(_type)))
      {
        THROW_NCML_PARSE_ERROR("Type Mismatch in variable element with name=" + _name +
            " at scope=" + p.getScopeString() +
            " Expected type=" + _type +
            " but found variable with type=" + pVar->type_name() +
                "  To match a variable of any type, please do not specify variable@type.");
      }

    // Use this variable as the new scope until we get a handleEnd()
    enterScope(p, pVar);
  }

  void
  VariableElement::processRenameVariable(NCMLParser& p)
  {
    BESDEBUG("ncml", "VariableElement::processRenameVariable() called on " + toString() << " at scope=" << p.getTypedScopeString() << endl);

    // First, make sure the data is valid
    NCML_ASSERT_MSG(!_name.empty(), "Can't have an empty variable@name if variable@orgName is specified!");
    NCML_ASSERT(!_orgName.empty()); // we shouldn't even call this if this is the case, but...

    // 1) Lookup _orgName, which must exist or throw
    BESDEBUG("ncml", "Looking up the existence of a variable with name=" << _orgName << "..." <<endl);
    BaseType* pOrgVar = p.getVariableInCurrentVariableContainer(_orgName);
    if (!pOrgVar)
      {
        THROW_NCML_PARSE_ERROR("Renaming variable failed for element=" + toString() + " since no variable with orgName=" + _orgName +
            " exists at current parser scope=" + p.getScopeString());
      }
    BESDEBUG("ncml", "Found variable with name=" << _orgName << endl);

    // 2) Lookup _name, which must NOT exist or throw
    BESDEBUG("ncml", "Making sure new name=" << _name << " does not exist at this scope already..." << endl);
    BaseType* pExisting = p.getVariableInCurrentVariableContainer(_name);
    if (pExisting)
      {
        THROW_NCML_PARSE_ERROR("Renaming variable failed for element=" + toString() +
            " since a variable with name=" + _name +
            " already exists at current parser scope=" + p.getScopeString());
      }
    BESDEBUG("ncml", "Success, new variable name is open at this scope."  << endl);

    // 3) Call set_name on the orgName variable.
    BESDEBUG("ncml", "Renaming variable " << _orgName << " to " << _name << endl);

    // Ugh, if we are parsing a DataDDS then we need to force the underlying data
    // to be read in before we rename it since otherwise the read() won't find the
    // renamed variables (unless we kept around the orgName, but that means changing
    // lots of libdap...)  HACK this is inefficient since we must load the entire dataset
    // even if we only want a small projection...
    if (p.parsingDataRequest() && !pOrgVar->read_p())
      {
        pOrgVar->read();
      }

    // BaseType::set_name fails for Vector (Array etc) subtypes since it doesn't
    // set the template's BaseType var's name as well.  This function does that until
    // a fix in libdap lets us call pOrgName->set_name(_name) directly.
    // pOrgVar->set_name(_name); // TODO  switch to this call when bug is fixed.
    NCMLUtil::setVariableNameProperly(pOrgVar, _name);

    // 4) Make sure we find the one we added.
    BaseType* pRenamedVar = p.getVariableInCurrentVariableContainer(_name);
    NCML_ASSERT_MSG(pRenamedVar == pOrgVar, "Renamed variable not found!  Logic error!");

    // 5) Change the scope for any nested calls
    enterScope(p, pRenamedVar);
    BESDEBUG("ncml", "Entering scope of the renamed variable.  Scope is now: " << p.getTypedScopeString() << endl);
  }

  void
  VariableElement::processNewVariable(NCMLParser& p)
  {
    BESDEBUG("ncml", "Entered VariableElement::processNewVariable..." << endl);

    // ASSERT: We know the variable with name doesn't exist, or we wouldn't call this function.

    // Type cannot be empty for a new variable!!
    if (_type.empty())
      {
        THROW_NCML_PARSE_ERROR("Must have non-empty variable@type when creating new variable=" + toString());
      }

    // Convert the type to the canonical type...
    string type = p.convertNcmlTypeToCanonicalType(_type);
    if (_type.empty())
      {
        THROW_NCML_PARSE_ERROR("Unknown type for new variable=" + toString());
      }

    // Tokenize the _shape string
    NCMLUtil::tokenize(_shape, _shapeTokens, NCMLUtil::WHITESPACE);

    // If it's a structure, go off and do that...
    if (_type == p.STRUCTURE_TYPE)
      {
        processNewStructure(p);
      }
    else if (_shape.empty()) // a scalar
      {
        processNewScalar(p, type);
      }
    else if (!_shape.empty())
      {
        processNewArray(p, type);
      }
    else
      {
        THROW_NCML_INTERNAL_ERROR("UNIMPLEMENTED METHOD: Cannot create non-scalar Array types yet.");
      }
  }

  void
  VariableElement::processNewStructure(NCMLParser& p)
  {
    // First, make sure we are at a parse scope that ALLOWS variables to be added!
    if (!(p.isScopeCompositeVariable() || p.isScopeGlobal()))
      {
        THROW_NCML_PARSE_ERROR("Cannot add a new Structure variable at current scope!  TypedScope=" + p.getTypedScopeString());
      }

    auto_ptr<BaseType> pNewVar = MyBaseTypeFactory::makeVariable("Structure", _name);
    NCML_ASSERT_MSG(pNewVar.get(), "VariableElement::processNewStructure: factory failed to make a new Structure variable for name=" + _name);

    // Add the copy, let auto_ptr clean up
    p.addCopyOfVariableAtCurrentScope(*pNewVar);

    // Lookup the variable we just added since it is added as a copy!
    BaseType* pActualVar = p.getVariableInCurrentVariableContainer(_name);
    VALID_PTR(pActualVar);
    // Make sure the copy mechanism did the right thing so we don't delete the var we just added.
    NCML_ASSERT(pActualVar != pNewVar.get());

    // Make it be the scope for any incoming new attributes.
    enterScope(p, pActualVar);
  }

  void
  VariableElement::processNewScalar(NCMLParser&p, const string& dapType)
  {
    addNewVariableAndEnterScope(p, dapType);
  }

  void
  VariableElement::processNewArray(NCMLParser& p, const std::string& dapType)
  {
    // For now, we can reuse the processNewScalar to make the variable and enter scope and all that.
    // Use the new template form for the Array so we get the NCMLArray<T> subclass that handles constraints.
    addNewVariableAndEnterScope(p, "Array<" + dapType + ">");

    // Now look up the added variable so we can set it's template and dimensionality.
    // it should be the current variable since we entered its scope above!
    Array* pNewVar = dynamic_cast<Array*>(p.getCurrentVariable());
    NCML_ASSERT_MSG(pNewVar, "VariableElement::processNewArray: Expected non-null getCurrentVariable() in parser but got NULL!");

    // Now make the template variable of the array entry type with the same name and add it
    auto_ptr<BaseType> pTemplateVar = MyBaseTypeFactory::makeVariable(dapType, _name);
    pNewVar->add_var(pTemplateVar.get());

    // For each dimension in the shape, append it to make an N-D array...
    for (unsigned int i=0; i<_shapeTokens.size(); ++i)
      {
        unsigned int dim = getSizeForDimension(p, _shapeTokens.at(i));
        string dimName = ((isDimensionNumericConstant(_shapeTokens.at(i)))?(""):(_shapeTokens.at(i)));
        BESDEBUG("ncml", "Appending dimension name=\"" << dimName << "\" of size=" << dim << " to the Array name=" << pNewVar->name() << endl);
        pNewVar->append_dim(dim, dimName);
      }

    // Make sure the size of the flattened Array in memory (product of dimensions) is within the DAP2 limit...
    if (getProductOfDimensionSizes(p) > static_cast<unsigned int>(DODS_MAX_ARRAY)) // actually the call itself will throw...
      {
        THROW_NCML_PARSE_ERROR("Product of dimension sizes for Array must be < (2^31-1).");
      }
  }

  void
  VariableElement::addNewVariableAndEnterScope(NCMLParser& p, const std::string& dapType)
  {
    // First, make sure we are at a parse scope that ALLOWS variables to be added!
    if (!(p.isScopeCompositeVariable() || p.isScopeGlobal()))
      {
        THROW_NCML_PARSE_ERROR("Cannot add a new scalar variable at current scope!  TypedScope=" + p.getTypedScopeString());
      }

    // Destroy it no matter what sicne add_var copies it
    auto_ptr<BaseType> pNewVar = MyBaseTypeFactory::makeVariable(dapType, _name);
    NCML_ASSERT_MSG(pNewVar.get(),  "VariableElement::addNewVariable: factory failed to make a new variable of type: " + dapType +
                                    " for element: " + toString());

    // Now that we have it, we need to add it to the parser at current scope
    // Internally, the add will copy the arg, not store it.
    p.addCopyOfVariableAtCurrentScope(*pNewVar);

    // Lookup the variable we just added since it is added as a copy!
    BaseType* pActualVar = p.getVariableInCurrentVariableContainer(_name);
    VALID_PTR(pActualVar);
    // Make sure the copy mechanism did the right thing so we don't delete the var we just added.
    NCML_ASSERT(pActualVar != pNewVar.get());

    // Make it be the scope for any incoming new attributes.
    enterScope(p, pActualVar);
  }

  void
  VariableElement::enterScope(NCMLParser& p, BaseType* pVar)
  {
    VALID_PTR(pVar);

    // Add the proper variable scope to the stack
    if (pVar->is_constructor_type())
      {
        p.enterScope(_name, ScopeStack::VARIABLE_CONSTRUCTOR);
      }
    else
      {
        p.enterScope(_name, ScopeStack::VARIABLE_ATOMIC);
      }

    // this sets the _pCurrentTable to the variable's table.
    p.setCurrentVariable(pVar);
  }

  void
  VariableElement::exitScope(NCMLParser& p)
  {
    // Set the new variable container to the parent of the current.
    // This could be NULL if we're a top level variable, making the DDS the variable container.
    p.setCurrentVariable(p.getCurrentVariable()->get_parent());
    p.exitScope();
    p.printScope();
  }

  bool
  VariableElement::isDimensionNumericConstant(const std::string& dimToken) const
  {
    // for now just test the first character is a number and assume it's a number
    return isdigit(dimToken.at(0));
  }

 unsigned int
 VariableElement::getSizeForDimension(NCMLParser& p, const std::string& dimToken) const
 {
   unsigned int dim = 0;
   // First, if the first char is a number, then assume it's an explicit non-negative integer
   if (isDimensionNumericConstant(dimToken))
     {
       stringstream token;
       token.str(dimToken);
       token >> dim;
       if (token.fail())
         {
           THROW_NCML_PARSE_ERROR( "Trying to get the dimension size in shape=" + _shape + " for token " + dimToken +
                                   " failed to parse the unsigned int!");
         }
     }
   else
     {
       const DimensionElement* pDim = p.getDimension(dimToken);
       if (pDim)
         {
           return pDim->getLengthNumeric();
         }
       else
         {
           THROW_NCML_PARSE_ERROR("Failed to find a dimension with name=" + dimToken +
               " for variable=" + toString() + " with dimension table= " + p.printDimensions() +
               " at scope=" + p.getScopeString());
         }
     }
   return dim;
 }

 unsigned int
 VariableElement::getProductOfDimensionSizes(NCMLParser& p) const
 {
   // If no shape, then it's size 0 (scalar)
   if (_shape.empty())
     {
       return 0;
     }

   // Otherwise compute it
   unsigned int product = 1;
   vector<string>::const_iterator endIt = _shapeTokens.end();
   vector<string>::const_iterator it;
   for (it = _shapeTokens.begin(); it != endIt; ++it)
     {
       const string& dimName = *it;
       unsigned int dimSize = getSizeForDimension(p, dimName); // might throw if not found...
       // if multiplying this in will cause over DODS_MAX_ARRAY, then error
       if (dimSize > (DODS_MAX_ARRAY/product))
         {
           THROW_NCML_PARSE_ERROR("Product of dimension sizes exceeds the maximum DAP2 size of 2147483647 (2^31-1)!");
         }
       // otherwise, multiply it in
       product *= dimSize;
     }
   return product;
 }
}
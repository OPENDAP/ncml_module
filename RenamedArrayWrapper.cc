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
#include "RenamedArrayWrapper.h"

#include <libdap/AttrTable.h>
#include <libdap/BaseType.h>
#include <libdap/DDS.h>
#include <libdap/dods-datatypes.h>
#include <libdap/Marshaller.h>
#include <libdap/UnMarshaller.h>
#include "NCMLDebug.h"
#include "NCMLUtil.h"
#include <sstream>
#include <vector>

using namespace libdap;
using std::ostringstream;
using std::string;


namespace ncml_module
{
  RenamedArrayWrapper::RenamedArrayWrapper()
  : Array("", 0)
  , _pArray(0)
  , _orgName("")
  {
  }

  RenamedArrayWrapper::RenamedArrayWrapper(const RenamedArrayWrapper& proto)
    : Array(proto) // I think we need to do this for constraints
    , _pArray(0)
    , _orgName(proto._orgName)
  {
    copyLocalRepFrom(proto);
  }

  RenamedArrayWrapper::RenamedArrayWrapper(libdap::Array* toBeWrapped)
  :Array(*toBeWrapped) // ugh, this will copy a lot of stuff we might not want to copy either, but seems safest.
  , _pArray(toBeWrapped)
  , _orgName("")
  {
    NCML_ASSERT_MSG(_pArray, "RenamedArrayWrapper(): expected non-null Array to wrap!!");
    _orgName = toBeWrapped->name();
    set_read_p(false); // force it to reload if we need to
  }

  RenamedArrayWrapper::~RenamedArrayWrapper()
  {
    destroy(); // local vars
  }

  RenamedArrayWrapper*
  RenamedArrayWrapper::ptr_duplicate()
  {
    return new RenamedArrayWrapper(*this);
  }

  RenamedArrayWrapper&
  RenamedArrayWrapper::operator=(const RenamedArrayWrapper& rhs)
  {
    if (&rhs == this)
      {
        return *this;
      }
    destroy();
    copyLocalRepFrom(rhs);
    return *this;
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////////
  /// Wrappers

  string
  RenamedArrayWrapper::toString()
  {
    return const_cast<const RenamedArrayWrapper*>(this)->toString();
  }

  string
  RenamedArrayWrapper::toString() const
  {
      ostringstream oss;
      oss << "RenamedArrayWrapper(" << this << "): " << endl;
      oss << "\t_pArray=" << ((_pArray)?(_pArray->toString()):("NULL")) << endl;
      return oss.str();
    }

  void
  RenamedArrayWrapper::dump(std::ostream &strm) const
  {
    strm << toString();
  }

  bool
  RenamedArrayWrapper::is_simple_type()
  {
    return _pArray->is_simple_type();
  }

  bool
  RenamedArrayWrapper::is_vector_type()
  {
    return _pArray->is_vector_type();
  }

  bool
  RenamedArrayWrapper::is_constructor_type()
  {
    return _pArray->is_constructor_type();
  }

  bool
  RenamedArrayWrapper::synthesized_p()
  {
    return _pArray->synthesized_p();
  }

  void
  RenamedArrayWrapper::set_synthesized_p(bool state)
  {
    // keep us in sync, why not.
    BaseType::set_synthesized_p(state);
    _pArray->set_synthesized_p(state);
  }

  int
  RenamedArrayWrapper::element_count(bool leaves /* = false */)
  {
    return _pArray->element_count(leaves);
  }

  bool
  RenamedArrayWrapper::read_p()
  {
    return _pArray->read_p();
  }

  void
  RenamedArrayWrapper::set_read_p(bool state)
  {
    BaseType::set_read_p(state);
    _pArray->set_read_p(state);
  }

  bool
  RenamedArrayWrapper::send_p()
  {
    return _pArray->send_p();
  }

  void
  RenamedArrayWrapper::set_send_p(bool state)
  {
    BaseType::set_send_p(state);
    _pArray->set_send_p(state);
  }

  /** We don't keep our own... */
  AttrTable&
  RenamedArrayWrapper::get_attr_table()
  {
    return _pArray->get_attr_table();
  }

  void
  RenamedArrayWrapper::set_attr_table(const AttrTable &at)
  {
    _pArray->set_attr_table(at);
  }

  bool
  RenamedArrayWrapper::is_in_selection()
  {
    return _pArray->is_in_selection();
  }

  void
  RenamedArrayWrapper::set_in_selection(bool state)
  {
    BaseType::set_in_selection(state);
    _pArray->set_in_selection(state);
  }

  /** Keep these in sync so it doesn't matter where we get the parent from... */
  void
  RenamedArrayWrapper::set_parent(BaseType *parent)
  {
    BaseType::set_parent(parent);
    _pArray->set_parent(parent);
  }

  BaseType*
  RenamedArrayWrapper::get_parent()
  {
    return _pArray->get_parent();
  }

  BaseType*
  RenamedArrayWrapper::var(const string &name /*  = "" */,
      bool exact_match /* = true */,
      btp_stack *s /* = 0 */)
  {
    return _pArray->var(name, exact_match, s);
  }

  BaseType*
  RenamedArrayWrapper::var(const string &name, btp_stack &s)
  {
    return _pArray->var(name, s);
  }

  void
  RenamedArrayWrapper::add_var(BaseType *bt, Part part /* = nil */)
  {
    _pArray->add_var(bt, part);
  }

  bool
  RenamedArrayWrapper::check_semantics(string &msg, bool all /* = false*/)
  {
    return _pArray->check_semantics(msg, all);
  }

  bool
  RenamedArrayWrapper::ops(BaseType *b, int op)
  {
    return _pArray->ops(b, op);
  }

  void
  RenamedArrayWrapper::print_decl(FILE *out,
      string space /* = "    "*/,
      bool print_semi /* = true*/,
      bool constraint_info /* = false*/,
      bool constrained /* = false */)
  {
    syncConstraints();
    withNewName();
    _pArray->print_decl(out, space, print_semi, constraint_info, constrained);
    withOrgName();
  }

  void
  RenamedArrayWrapper::print_decl(ostream &out,
      string space /* = "    "*/,
      bool print_semi /* = true*/,
      bool constraint_info /* = false*/,
      bool constrained /* = false*/)
  {
    syncConstraints();
    withNewName();
    _pArray->print_decl(out, space, print_semi, constraint_info, constrained);
    withOrgName();
  }

  void
  RenamedArrayWrapper::print_xml(FILE *out,
      string space /* = "    "*/,
      bool constrained /* = false */)
  {
    syncConstraints();
    withNewName();
    _pArray->print_xml(out, space, constrained);
    withOrgName();
  }

  void
  RenamedArrayWrapper::print_xml(ostream &out,
      string space /* = "    " */,
      bool constrained /* = false */)
  {
    syncConstraints();
    withNewName();
    _pArray->print_xml(out, space, constrained);
    withOrgName();
  }

  unsigned int
  RenamedArrayWrapper::width()
  {
    syncConstraints();
    return _pArray->width();
  }

  unsigned int
  RenamedArrayWrapper::buf2val(void **val)
  {
    syncConstraints();
    return _pArray->buf2val(val);
  }

  unsigned int
  RenamedArrayWrapper::val2buf(void *val, bool reuse /* = false */)
  {
    syncConstraints();
    return _pArray->val2buf(val, reuse);
  }


  void
  RenamedArrayWrapper::print_val(FILE *out,
      string space /* = ""*/,
      bool print_decl_p /* = true*/)
  {
    syncConstraints();
    withNewName();
    print_val(out, space, print_decl_p);
    withOrgName();
  }

  void
  RenamedArrayWrapper::print_val(ostream &out,
      string space /* = ""*/,
      bool print_decl_p /* = true*/)
  {
    syncConstraints();
    withNewName();
    print_val(out, space, print_decl_p);
    withOrgName();
  }

  bool
  RenamedArrayWrapper::set_value(dods_byte *val, int sz)
  {
    syncConstraints();
    return _pArray->set_value(val, sz);
  }

  bool
  RenamedArrayWrapper::set_value(vector<dods_byte> &val, int sz)
  {
    syncConstraints();
    return _pArray->set_value(val, sz);
  }

  bool
  RenamedArrayWrapper::set_value(dods_int16 *val, int sz)
  {
    syncConstraints();
    return _pArray->set_value(val, sz);
  }

  bool
  RenamedArrayWrapper::set_value(vector<dods_int16> &val, int sz)
  {
    syncConstraints();
    return _pArray->set_value(val, sz);
  }

  bool
  RenamedArrayWrapper::set_value(dods_uint16 *val, int sz)
  {
    syncConstraints();
    return _pArray->set_value(val, sz);
  }

  bool
  RenamedArrayWrapper::set_value(vector<dods_uint16> &val, int sz)
  {
    syncConstraints();
    return _pArray->set_value(val, sz);
  }

  bool
  RenamedArrayWrapper::set_value(dods_int32 *val, int sz)
  {
    syncConstraints();
    return _pArray->set_value(val, sz);
  }

  bool
  RenamedArrayWrapper::set_value(vector<dods_int32> &val, int sz)
  {
    syncConstraints();
    return _pArray->set_value(val, sz);
  }

  bool
  RenamedArrayWrapper::set_value(dods_uint32 *val, int sz)
  {
    syncConstraints();
    return _pArray->set_value(val, sz);
  }

  bool
  RenamedArrayWrapper::set_value(vector<dods_uint32> &val, int sz)
  {
    syncConstraints();
    return _pArray->set_value(val, sz);
  }

  bool
  RenamedArrayWrapper::set_value(dods_float32 *val, int sz)
  {
    syncConstraints();
    return _pArray->set_value(val, sz);
  }

  bool
  RenamedArrayWrapper::set_value(vector<dods_float32> &val, int sz)
  {
    syncConstraints();
    return _pArray->set_value(val, sz);
  }

  bool
  RenamedArrayWrapper::set_value(dods_float64 *val, int sz)
  {
    syncConstraints();
    return _pArray->set_value(val, sz);
  }

  bool
  RenamedArrayWrapper::set_value(vector<dods_float64> &val, int sz)
  {
    syncConstraints();
    return _pArray->set_value(val, sz);
  }

  bool
  RenamedArrayWrapper::set_value(string *val, int sz)
  {
    syncConstraints();
    return _pArray->set_value(val, sz);
  }

  bool
  RenamedArrayWrapper::set_value(vector<string> &val, int sz)
  {
    syncConstraints();
    return _pArray->set_value(val, sz);
  }

  void
  RenamedArrayWrapper::value(dods_byte *b) const
  {
    syncConstraints();
    _pArray->value(b);
  }

  void
  RenamedArrayWrapper::value(dods_int16 *b) const
  {
    syncConstraints();
    _pArray->value(b);
  }

  void
  RenamedArrayWrapper::value(dods_uint16 *b) const
  {
    syncConstraints();
    _pArray->value(b);
  }

  void
  RenamedArrayWrapper::value(dods_int32 *b) const
  {
    syncConstraints();
    _pArray->value(b);
  }

  void
  RenamedArrayWrapper::value(dods_uint32 *b) const
  {
    syncConstraints();
    _pArray->value(b);
  }

  void
  RenamedArrayWrapper::value(dods_float32 *b) const
  {
    syncConstraints();
    _pArray->value(b);
  }

  void
  RenamedArrayWrapper::value(dods_float64 *b) const
  {
    syncConstraints();
    _pArray->value(b);
  }

  void
  RenamedArrayWrapper::value(vector<string> &b) const
  {
    syncConstraints();
    _pArray->value(b);
  }

  void*
  RenamedArrayWrapper::value()
  {
    syncConstraints();
    return _pArray->value();
  }

  // DO THE REAL WORK
  bool
  RenamedArrayWrapper::read()
  {
    // Read using the old name....
    withOrgName();
    bool ret = _pArray->read();
    set_read_p(true); // get us too
    withNewName();
    return ret;
  }

  void
  RenamedArrayWrapper::intern_data(ConstraintEvaluator &eval, DDS &dds)
  {
    syncConstraints();

    // Force the correct read to be called, as with serialize...
    // If not read in, read it in with the orgName and these constraints.
    if (!_pArray->read_p())
      {
        withOrgName();
        _pArray->read();
        set_read_p(true);
      }

    // Now we're back to the new name for intern_data purposes.
    withNewName();
    _pArray->intern_data(eval, dds);
  }

  bool
  RenamedArrayWrapper::serialize(ConstraintEvaluator &eval, DDS &dds,
      Marshaller &m, bool ce_eval /* = true */)
  {
    BESDEBUG("ncml", "RenamedArrayWrapper::serialize(): Doing the magic for renamed read()!!" << endl);
    // Push them down if we need to.
    syncConstraints();

    // If not read in, read it in with the orgName and these constraints.
    if (!_pArray->read_p())
      {
        withOrgName();
        _pArray->read();
        set_read_p(true);
      }

    // Now we're back to the new name for printing purposes.
    withNewName();

    // So call the actual serialize, which should hopefully respect read_p() being set!!
    return _pArray->serialize(eval, dds, m, ce_eval);
  }

  bool
  RenamedArrayWrapper::deserialize(UnMarshaller &um, DDS *dds, bool reuse /* = false */)
  {
    // I *think* this should work
    syncConstraints();
    return _pArray->deserialize(um, dds, reuse);
  }


  ////////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////// PRIVATE IMPL

  void
  RenamedArrayWrapper::copyLocalRepFrom(const RenamedArrayWrapper& proto)
  {
    if (&proto == this)
      {
        return;
      }

    if (proto._pArray)
      {
        _pArray = dynamic_cast<libdap::Array*>(proto._pArray->ptr_duplicate());
      }
    _orgName = proto._orgName;
  }

  void
  RenamedArrayWrapper::destroy()
  {
    SAFE_DELETE(_pArray);
    _orgName = "";
  }

  void
  RenamedArrayWrapper::withNewName()
  {
    NCMLUtil::setVariableNameProperly(_pArray, name());
  }

  void
  RenamedArrayWrapper::withOrgName()
  {
    NCMLUtil::setVariableNameProperly(_pArray, _orgName);
  }

  void
  RenamedArrayWrapper::syncConstraints()
  {
    // Grr, I wish the _shape was protected or add_constraint was virtual...  This makes it harder...

    // First see if the number of dimensions is correct.  We may not need to bother with this,
    // just constraint propagation
    if (_pArray->dimensions() != dimensions())
      {
        THROW_NCML_INTERNAL_ERROR("RenamedArrayWrapper::syncConstraints(): dimensions() of this and wrapped array do not match!");
      }

    // If they match, iterate the shape and set make sure the values are set.
    Array::Dim_iter thisEndIt = dim_end();
    Array::Dim_iter thisIt, wrapIt;
    for (thisIt = dim_begin(), wrapIt = _pArray->dim_begin(); thisIt != thisEndIt; ++thisIt, ++wrapIt)
      {
         Array::dimension& thisDim = *thisIt;
         Array::dimension& wrapDim = *wrapIt;
         wrapDim = thisDim; // copy them!
      }
    _pArray->update_length(this->length());
    NCML_ASSERT_MSG(this->length() == _pArray->length(), "RenamedArrayWrapper::syncConstraints(): length() of this and wrapped do not match!!");
  }
}
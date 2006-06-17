// -*- C++ -*-

// Copyright (C) 2005, 2006 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the terms
// of the GNU General Public License as published by the Free Software
// Foundation; either version 2, or (at your option) any later
// version.

// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this library; see the file COPYING.  If not, write to
// the Free Software Foundation, 59 Temple Place - Suite 330, Boston,
// MA 02111-1307, USA.

// As a special exception, you may use this file as part of a free
// software library without restriction.  Specifically, if other files
// instantiate templates or use macros or inline functions from this
// file, or you compile this file and link it with other files to
// produce an executable, this file does not by itself cause the
// resulting executable to be covered by the GNU General Public
// License.  This exception does not however invalidate any other
// reasons why the executable file might be covered by the GNU General
// Public License.

// Copyright (C) 2004 Ami Tavory and Vladimir Dreizin, IBM-HRL.

// Permission to use, copy, modify, sell, and distribute this software
// is hereby granted without fee, provided that the above copyright
// notice appears in all copies, and that both that copyright notice
// and this permission notice appear in supporting documentation. None
// of the above authors, nor IBM Haifa Research Laboratories, make any
// representation about the suitability of this software for any
// purpose. It is provided "as is" without express or implied
// warranty.

/**
 * @file constructors_destructor_fn_imps.hpp
 * Contains an implementation for thin_heap_.
 */

PB_DS_CLASS_T_DEC
template<typename It>
void
PB_DS_CLASS_C_DEC::
copy_from_range(It first_it, It last_it)
{
  while (first_it != last_it)
    push(*(first_it++));

  PB_DS_DBG_ONLY(assert_valid();)
    }

PB_DS_CLASS_T_DEC
PB_DS_CLASS_C_DEC::
thin_heap_() :
  m_p_max(NULL)
{
  initialize();

  PB_DS_DBG_ONLY(assert_valid();)
    }

PB_DS_CLASS_T_DEC
PB_DS_CLASS_C_DEC::
thin_heap_(const Cmp_Fn& r_cmp_fn) :
  PB_DS_BASE_C_DEC(r_cmp_fn),
  m_p_max(NULL)
{
  initialize();

  PB_DS_DBG_ONLY(assert_valid();)
    }

PB_DS_CLASS_T_DEC
PB_DS_CLASS_C_DEC::
thin_heap_(const PB_DS_CLASS_C_DEC& other) :
  PB_DS_BASE_C_DEC(other)
{
  initialize();

  m_p_max = base_type::m_p_root;

  for (node_pointer p_nd = base_type::m_p_root; p_nd != NULL; p_nd = p_nd->m_p_next_sibling)
    if (Cmp_Fn::operator()(m_p_max->m_value, p_nd->m_value))
      m_p_max = p_nd;

  PB_DS_DBG_ONLY(assert_valid();)
    }

PB_DS_CLASS_T_DEC
void
PB_DS_CLASS_C_DEC::
swap(PB_DS_CLASS_C_DEC& other)
{
  PB_DS_DBG_ONLY(assert_valid();)

    base_type::swap(other);

  std::swap(m_p_max, other.m_p_max);

  PB_DS_DBG_ONLY(assert_valid();)
    }

PB_DS_CLASS_T_DEC
PB_DS_CLASS_C_DEC::
~thin_heap_()
{ }

PB_DS_CLASS_T_DEC
void
PB_DS_CLASS_C_DEC::
initialize()
{
  std::fill(m_a_aux, m_a_aux + max_rank, static_cast<node_pointer>(NULL));
}

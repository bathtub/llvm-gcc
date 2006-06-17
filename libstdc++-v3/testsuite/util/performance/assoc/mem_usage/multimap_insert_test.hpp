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
 * @file multimap_insert_test.hpp
 * Contains a generic multimap_insert_test test.
 */

#ifndef PB_DS_MULTIMAP_INSERT_TEST_HPP
#define PB_DS_MULTIMAP_INSERT_TEST_HPP

#include <ext/pb_ds/detail/type_utils.hpp>
#include <performance/mem/mem_track_allocator.hpp>
#include <performance/io/xml_formatter.hpp>
#include <common_type/assoc/string_form.hpp>
#include <iterator>

namespace pb_ds
{

  namespace test
  {

#define PB_DS_CLASS_T_DEC			\
    template<typename It, bool Native>

#define PB_DS_CLASS_C_DEC				\
    multimap_insert_test<				\
						It,	\
						Native>

    template<typename It, bool Native>
    class multimap_insert_test
    {
    public:
      multimap_insert_test(It ins_b, size_t ins_vn, size_t ins_vs, size_t ins_vm);

      template<typename Cntnr>
      void
      operator()(pb_ds::detail::type_to_type<Cntnr>);

    private:
      multimap_insert_test(const multimap_insert_test& );

      template<typename Cntnr>
      size_t
      insert(pb_ds::detail::type_to_type<Cntnr>, It ins_it_b, It ins_it_e, pb_ds::detail::true_type);

      template<typename Cntnr>
      size_t
      insert(pb_ds::detail::type_to_type<Cntnr>, It ins_it_b, It ins_it_e, pb_ds::detail::false_type);

    private:
      const It m_ins_b;

      const size_t m_ins_vn;
      const size_t m_ins_vs;
      const size_t m_ins_vm;
    };

    PB_DS_CLASS_T_DEC
    PB_DS_CLASS_C_DEC::
    multimap_insert_test(It ins_b, size_t ins_vn, size_t ins_vs, size_t ins_vm) :
      m_ins_b(ins_b),
      m_ins_vn(ins_vn),
      m_ins_vs(ins_vs),
      m_ins_vm(ins_vm)
    { }

    PB_DS_CLASS_T_DEC
    template<typename Cntnr>
    void
    PB_DS_CLASS_C_DEC::
    operator()(pb_ds::detail::type_to_type<Cntnr>)
    {
      xml_result_set_performance_formatter res_set_fmt(
						       string_form<Cntnr>::name(),
						       string_form<Cntnr>::desc());

      for (size_t size_i = 0; m_ins_vn + size_i*  m_ins_vs < m_ins_vm; ++size_i)
	{
	  const size_t ins_size = m_ins_vn + size_i*  m_ins_vs;

	  It ins_it_b = m_ins_b;
	  It ins_it_e = m_ins_b;
	  std::advance(ins_it_e, ins_size);

	  const size_t delta_mem = insert(pb_ds::detail::type_to_type<Cntnr>(),
					  ins_it_b,
					  ins_it_e,
					  pb_ds::detail::integral_constant<int,Native>());

	  res_set_fmt.add_res(ins_size, static_cast<double>(delta_mem));
	}
    }

    PB_DS_CLASS_T_DEC
    template<typename Cntnr>
    size_t
    PB_DS_CLASS_C_DEC::
    insert(pb_ds::detail::type_to_type<Cntnr>, It ins_it_b, It ins_it_e, pb_ds::detail::true_type)
    {
      mem_track_allocator<char> alloc;

      const size_t init_mem = alloc.get_total();

      Cntnr cntnr;

      for (It ins_it = ins_it_b; ins_it != ins_it_e; ++ins_it)
        cntnr.insert((typename Cntnr::const_reference)(*ins_it));

      const size_t final_mem = alloc.get_total();

      assert(final_mem > init_mem);

      return (final_mem - init_mem);
    }

    PB_DS_CLASS_T_DEC
    template<typename Cntnr>
    size_t
    PB_DS_CLASS_C_DEC::
    insert(pb_ds::detail::type_to_type<Cntnr>, It ins_it_b, It ins_it_e, pb_ds::detail::false_type)
    {
      mem_track_allocator<char> alloc;

      const size_t init_mem = alloc.get_total();

      Cntnr cntnr;

      for (It ins_it = ins_it_b; ins_it != ins_it_e; ++ins_it)
        cntnr[ins_it->first].insert(ins_it->second);

      const size_t final_mem = alloc.get_total();

      assert(final_mem > init_mem);

      return (final_mem - init_mem);
    }

#undef PB_DS_CLASS_T_DEC

#undef PB_DS_CLASS_C_DEC

  } // namespace test

} // namespace pb_ds

#endif // #ifndef PB_DS_MULTIMAP_INSERT_TEST_HPP

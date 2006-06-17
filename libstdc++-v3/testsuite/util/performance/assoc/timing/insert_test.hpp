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
 * @file insert_test.hpp
 * Contains an insert performance test.
 */

#ifndef PB_DS_INSERT_TEST_HPP
#define PB_DS_INSERT_TEST_HPP

#include <performance/time/timing_test_base.hpp>
#include <ext/pb_ds/detail/type_utils.hpp>
#include <performance/io/xml_formatter.hpp>
#include <common_type/assoc/string_form.hpp>
#include <iterator>

namespace pb_ds
{

  namespace test
  {

    namespace detail
    {

      template<typename It, class Cntnr>
      class insert_insert_functor
      {
      public:
        insert_insert_functor(It ins_it_b,  It ins_it_e) : m_ins_it_b(ins_it_b),
							   m_ins_it_e(ins_it_e)
	{ }

	void
        operator()(std::size_t resolution)
	{
	  for (std::size_t i = 0; i < resolution; ++i)
	    {
	      Cntnr cntnr;

	      for (It ins_it = m_ins_it_b; ins_it != m_ins_it_e; ++ins_it)
                cntnr.insert((typename Cntnr::const_reference)(*ins_it));
	    }
	}

      private:
	const It m_ins_it_b;
	const It m_ins_it_e;
      };

    } // namespace detail

#define PB_DS_CLASS_T_DEC			\
    template<typename It>

#define PB_DS_CLASS_C_DEC				\
    insert_test<					\
						It>

    template<typename It>
    class insert_test : private pb_ds::test::detail::timing_test_base
    {
    public:
      insert_test(It ins_b, size_t ins_vn, size_t ins_vs, size_t ins_vm);

      template<typename Cntnr>
      void
      operator()(pb_ds::detail::type_to_type<Cntnr>);

    private:
      insert_test(const insert_test& );

      template<typename Cntnr>
      void
      insert(pb_ds::detail::type_to_type<Cntnr>, It ins_it_b, It ins_it_e);

    private:
      const It m_ins_b;

      const size_t m_ins_vn;
      const size_t m_ins_vs;
      const size_t m_ins_vm;
    };

    PB_DS_CLASS_T_DEC
    PB_DS_CLASS_C_DEC::
    insert_test(It ins_b, size_t ins_vn, size_t ins_vs, size_t ins_vm) :
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
	  const size_t v = m_ins_vn + size_i*  m_ins_vs;

	  It ins_it_b = m_ins_b;
	  It ins_it_e = m_ins_b;
	  std::advance(ins_it_e, v);

	  pb_ds::test::detail::insert_insert_functor<It, Cntnr>
            fn(ins_it_b, ins_it_e);

	  const double res =
            pb_ds::test::detail::timing_test_base::operator()(fn);

	  res_set_fmt.add_res(v, res / v);
	}
    }

    PB_DS_CLASS_T_DEC
    template<typename Cntnr>
    void
    PB_DS_CLASS_C_DEC::
    insert(pb_ds::detail::type_to_type<Cntnr>, It ins_it_b, It ins_it_e)
    {
      Cntnr cntnr;

      for (It ins_it = ins_it_b; ins_it != ins_it_e; ++ins_it)
        cntnr.insert((typename Cntnr::const_reference)(*ins_it));
    }

#undef PB_DS_CLASS_T_DEC

#undef PB_DS_CLASS_C_DEC

  } // namespace test

} // namespace pb_ds

#endif // #ifndef PB_DS_INSERT_TEST_HPP

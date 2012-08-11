#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>
#include <functional>
#include <stdexcept>
#include <boost/variant.hpp>
#include <boost/variant/recursive_wrapper.hpp>

namespace unification
{

class type_variable;
class type_operator;

typedef boost::variant<
  type_variable,
  boost::recursive_wrapper<type_operator>
> type;

class type_variable
{
  public:
    inline type_variable()
      : m_id()
    {}

    inline type_variable(const std::size_t i)
      : m_id(i)
    {}

    inline std::size_t id() const
    {
      return m_id;
    } // end id()

    inline bool operator==(const type_variable &other) const
    {
      return id() == other.id();
    } // end operator==()

    inline bool operator!=(const type_variable &other) const
    {
      return !(*this == other);
    } // end operator!=()

    inline bool operator<(const type_variable &other) const
    {
      return id() < other.id();
    } // end operator<()

    inline operator std::size_t (void) const
    {
      return id();
    } // end operator size_t

  private:
    std::size_t m_id;
}; // end type_variable

class type_operator
  : private std::vector<type>
{
  public:
    typedef std::size_t kind_type;

  private:
    typedef std::vector<type> super_t;
    std::vector<type> m_types;

    kind_type m_kind;
    
  public:
    using super_t::begin;
    using super_t::end;
    using super_t::size;
    using super_t::operator[];

    inline type_operator(const kind_type &kind)
      : m_kind(kind)
    {}

    template<typename Iterator>
      type_operator(const kind_type &kind,
                    Iterator first,
                    Iterator last)
        : super_t(first, last),
          m_kind(kind)
    {}

    template<typename Range>
    inline type_operator(const kind_type &kind,
                         const Range &rng)
      : super_t(rng.begin(), rng.end()),
        m_kind(kind)
    {}

    inline type_operator(const kind_type &kind,
                         std::initializer_list<type> &&types)
      : super_t(types),
        m_kind(kind)
    {}

    inline type_operator(type_operator &&other)
      : super_t(std::move(other)),
        m_kind(std::move(other.m_kind))
    {}

    inline const kind_type &kind(void) const
    {
      return m_kind;
    } // end kind()

    inline bool compare_kind(const type_operator &other) const
    {
      return kind() == other.kind() && size() == other.size();
    } // end operator==()

    inline bool operator==(const type_operator &other) const
    {
      return compare_kind(other) & std::equal(begin(), end(), other.begin());
    } // end operator==()
}; // end type_operator

typedef std::pair<type, type> constraint;

struct type_mismatch
  : std::runtime_error
{
  inline type_mismatch(const type &xx, const type &yy)
    : std::runtime_error("type mismatch"),
      x(xx),
      y(yy)
  {}

  inline virtual ~type_mismatch(void) throw()
  {}

  type x;
  type y;
};

struct recursive_unification
  : std::runtime_error
{
  inline recursive_unification(const type &xx, const type &yy)
    : std::runtime_error("recursive unification"),
      x(xx),
      y(yy)
  {}

  inline virtual ~recursive_unification(void) throw()
  {}

  type x, y;
};

namespace detail
{

inline void replace(type &x, const type_variable &replace_me, const type &replacement)
{
  if(x.which())
  {
    auto &op = boost::get<type_operator>(x);
    auto f = std::bind(replace, std::placeholders::_1, replace_me, replacement);
    std::for_each(op.begin(), op.end(), f);
  } // end if
  else
  {
    auto &var = boost::get<type_variable>(x);
    if(var == replace_me)
    {
      x = replacement;
    } // end if
  } // end else
} // end replace()

inline bool occurs(const type &haystack, const type_variable &needle)
{
  bool result = false;
  if(haystack.which())
  {
    auto &op = boost::get<type_operator>(haystack);
    auto f = std::bind(occurs, std::placeholders::_1, needle);
    result = std::any_of(op.begin(), op.end(), f);
  } // end end if
  else
  {
    auto &var = boost::get<type_variable>(haystack);
    result = (var == needle);
  } // end else

  return result;
} // end occurs()

struct equals_variable
  : boost::static_visitor<bool>
{
  inline equals_variable(const type_variable &xx)
    : m_x(xx)
  {}

  inline bool operator()(const type_variable &y)
  {
    return m_x == y;
  } // end operator()()

  inline bool operator()(const type_operator &y)
  {
    return false;
  } // end operator()()

  const type_variable &m_x;
}; // end equals_variable

struct replacer
  : boost::static_visitor<>
{
  inline replacer(const type_variable &replace_me)
    : m_replace_me(replace_me)
  {}

  inline void operator()(type_variable &var, const type_variable &replacement)
  {
    if(var == m_replace_me)
    {
      var = replacement;
    } // end if
  } // end operator()()

  template<typename T>
    inline void operator()(type_operator &op, const T &replacement)
  {
    auto v = boost::apply_visitor(*this);
    auto f = std::bind(v, std::placeholders::_2, replacement);
    std::for_each(op.begin(), op.end(), f);
  } // end operator()()

  const type_variable &m_replace_me;
}; // end replacer

class unifier
  : public boost::static_visitor<>
{
  inline void eliminate(const type_variable &x, const type &y)
  {
    // replace all occurrances of x with y in the stack and the substitution
    for(auto i = m_stack.begin();
        i != m_stack.end();
        ++i)
    {
      replace(i->first, x, y);
      replace(i->second, x, y);
    } // end for i

    for(auto i = m_substitution.begin();
        i != m_substitution.end();
        ++i)
    {
      replace(i->second, x, y);
    } // end for i

    // add x = y to the substitution
    m_substitution[x] = y;
  } // end eliminate()

  std::vector<constraint>       m_stack;
  std::map<type_variable, type> &m_substitution;

  public:
    // apply_visitor requires that these functions be public
    inline void operator()(const type_variable &x, const type_variable &y)
    {
      if(x != y)
      {
        eliminate(x,y);
      } // end if
    } // end operator()()

    inline void operator()(const type_variable &x, const type_operator &y)
    {
      if(occurs(y,x))
      {
        throw recursive_unification(x,y);
      } // end if

      eliminate(x,y);
    } // end operator()()

    inline void operator()(const type_operator &x, const type_variable &y)
    {
      if(occurs(x,y))
      {
        throw recursive_unification(y,x);
      } // end if

      eliminate(y,x);
    } // end operator()()

    inline void operator()(const type_operator &x, const type_operator &y)
    {
      if(!x.compare_kind(y))
      {
        throw type_mismatch(x,y);
      } // end if

      // push (xi,yi) onto the stack
      for(auto xi = x.begin(), yi = y.begin();
          xi != x.end();
          ++xi, ++yi)
      {
        m_stack.push_back(std::make_pair(*xi, *yi));
      } // end for xi, yi
    } // end operator()()

    template<typename Iterator>
      inline unifier(Iterator first_constraint, Iterator last_constraint, std::map<type_variable,type> &substitution)
        : m_stack(first_constraint, last_constraint),
          m_substitution(substitution)
    {
      // add the current substitution to the stack
      // XXX this step might be unnecessary
      m_stack.insert(m_stack.end(), m_substitution.begin(), m_substitution.end());
      m_substitution.clear();
    } // end unifier()

    inline void operator()(void)
    {
      while(!m_stack.empty())
      {
        type x = std::move(m_stack.back().first);
        type y = std::move(m_stack.back().second);
        m_stack.pop_back();

        boost::apply_visitor(*this, x, y);
      } // end while
    } // end operator()()
}; // unifier()

} // end detail

template<typename Iterator>
  void unify(Iterator first_constraint, Iterator last_constraint, std::map<type_variable,type> &substitution)
{
  detail::unifier u(first_constraint, last_constraint, substitution);
  u();
} // end unify()

template<typename Range>
  void unify(const Range &rng, std::map<type_variable,type> &substitution)
{
  return unify(rng.begin(), rng.end(), substitution);
} // end unify()

// often our system has only a single constraint
void unify(const type &x, const type &y, std::map<type_variable,type> &substitution)
{
  auto c = constraint(x,y);
  return unify(&c, &c + 1, substitution);
} // end unify()

template<typename Range>
  std::map<type_variable,type>
    unify(const Range &rng)
{
  std::map<type_variable,type> solutions;
  unify(rng, solutions);
  return std::move(solutions);
} // end unify()

} // end unification


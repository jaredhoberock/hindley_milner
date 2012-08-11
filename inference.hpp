#pragma once

#include <map>
#include <set>
#include <algorithm>
#include <string>
#include <utility>
#include <boost/variant/static_visitor.hpp>
#include "unification.hpp"
#include "syntax.hpp"

namespace inference
{

using unification::type;
using unification::type_variable;
using unification::type_operator;

std::ostream &operator<<(std::ostream &os, const std::set<type_variable> &x)
{
  for(auto i = x.begin();
      i != x.end();
      ++i)
  {
    os << *i << " ";
  }

  return os;
}

std::ostream &operator<<(std::ostream &os, const std::map<type_variable,type_variable> &x)
{
  os << "{";
  for(auto i = x.begin();
      i != x.end();
      ++i)
  {
    os << i->first << " : " << i->second << ", ";
  }
  os << "}";

  return os;
}

namespace types
{

static const int integer  = 0;
static const int boolean  = 1;
static const int function = 2;
static const int pair     = 3;

}

inline type make_function(const type &arg,
                          const type &result)
{
  return type_operator(types::function, {arg, result});
}

inline type integer(void)
{
  return type_operator(types::integer);
}

inline type boolean(void)
{
  return type_operator(types::boolean);
}

inline type pair(const type &first,
                 const type &second)
{
  return type_operator(types::pair, {first, second});
}

inline type definitive(const std::map<type_variable,type> &substitution, const type_variable &x)
{
  type result = x;
 
  // iteratively follow type_variables in the substitution until we can't go any further
  type_variable *ptr = 0;
  while((ptr = boost::get<type_variable>(&result)) && substitution.count(*ptr))
  {
    result = substitution.find(*ptr)->second;
  } // end while
 
  return result;
}

class environment
  : public std::map<std::string, type>
{
  public:
    inline environment()
      : m_next_id(0)
    {}

    inline std::size_t unique_id()
    {
      return m_next_id++;
    }

  private:
    std::size_t m_next_id;
};

struct fresh_maker
  : boost::static_visitor<type>
{
  inline fresh_maker(environment &env,
                     const std::set<type_variable> &non_generic,
                     const std::map<type_variable,type> &substitution)
    : m_env(env),
      m_non_generic(non_generic),
      m_substitution(substitution)
  {}

  inline result_type operator()(const type_variable &var)
  {
    if(is_generic(var))
    {
      std::clog << var << " is generic" << std::endl;
      std::clog << "mappings: " << m_mappings << std::endl;
      if(!m_mappings.count(var))
      {
        std::clog << var << " is not in mappings" << std::endl;
        m_mappings[var] = type_variable(m_env.unique_id());
      } // end if

      return m_mappings[var];
    } // end if

    std::clog << var << " is not generic" << std::endl;

    return var;
  } // end operator()()

  inline result_type operator()(const type_operator &op)
  {
    std::vector<type> types(op.size());
    // make sure to pass a reference to this to maintain our state
    std::transform(op.begin(), op.end(), types.begin(), std::ref(*this));
    return type_operator(op.kind(), types);
  } // end operator()()

  inline result_type operator()(const type &x)
  {
    result_type result;
    // XXX hard-coded 0 here sucks
    if(x.which() == 0)
    {
      auto definitive_type = definitive(m_substitution, boost::get<type_variable>(x));
      result = boost::apply_visitor(*this, definitive_type);
    } // end if
    else
    {
      result = boost::apply_visitor(*this, x);
    } // end else

    return result;
  } // end operator()

  private:
    inline bool is_generic(const type_variable &var) const
    {
      bool occurs = false;

      std::clog << "is_generic: checking for " << var << std::endl;

      for(auto i = m_non_generic.begin();
          i != m_non_generic.end();
          ++i)
      {
        std::clog << "is_generic: checking in " << *i << std::endl;
        occurs = unification::detail::occurs(definitive(m_substitution, *i), var);

        std::clog << "is_generic: occurs: " << occurs << std::endl;

        if(occurs) break;
      } // end for i

      return !occurs;
    } // end is_generic()

    environment                           &m_env;
    const std::set<type_variable>         &m_non_generic;
    const std::map<type_variable,type>    &m_substitution;
    std::map<type_variable, type_variable> m_mappings;
}; // end fresh_maker

struct inferencer
  : boost::static_visitor<type>
{
  inline inferencer(const environment &env)
    : m_environment(env)
  {}

  inline result_type operator()(const syntax::integer_literal)
  {
    return integer();
  } // end operator()()

  inline result_type operator()(const syntax::identifier &id)
  {
    if(!m_environment.count(id.name()))
    {
      auto what = std::string("Undefined symbol ") + id.name();
      throw std::runtime_error(what);
    } // end if

    // create a fresh type
    std::clog << "inferencer(identifier): m_non_generic_variables: " << m_non_generic_variables << std::endl;
    std::clog << "inferencer(identifier): calling fresh_maker on " << id.name() << std::endl;
    auto freshen_me = m_environment[id.name()];
    auto v = fresh_maker(m_environment, m_non_generic_variables, m_substitution);
    return v(freshen_me);
  } // end operator()()

  inline result_type operator()(const syntax::apply &app)
  {
    std::clog << "inferencer(apply): m_non_generic_variables: " << std::endl;
    std::clog << m_non_generic_variables << std::endl;

    auto fun_type = boost::apply_visitor(*this, app.function());
    auto arg_type = boost::apply_visitor(*this, app.argument());

    std::clog << "inferencer(apply): calling unique_id" << std::endl;
    auto x = type_variable(m_environment.unique_id());
    auto lhs = make_function(arg_type, x);

    unification::unify(lhs, fun_type, m_substitution);

    return definitive(m_substitution,x);
  } // end operator()()

  inline result_type operator()(const syntax::lambda &lambda)
  {
    std::clog << "inferencer(lambda): calling unique_id" << std::endl;
    auto arg_type = type_variable(m_environment.unique_id());

    // introduce a scope with a non-generic variable
    auto s = scoped_non_generic_variable(this, lambda.parameter(), arg_type);

    // get the type of the body of the lambda
    std::clog << "inferencer(lambda): m_non_generic_variables: " << m_non_generic_variables << std::endl;
    auto body_type = boost::apply_visitor(*this, lambda.body());

    // x = (arg_type -> body_type)
    std::clog << "inferencer(lambda): calling unique_id" << std::endl;
    auto x = type_variable(m_environment.unique_id());
    unification::unify(x, make_function(arg_type, body_type), m_substitution);

    return definitive(m_substitution,x);
  } // end operator()()

  inline result_type operator()(const syntax::let &let)
  {
    auto defn_type = boost::apply_visitor(*this, let.definition());

    // introduce a scope with a generic variable
    auto s = scoped_generic(this, let.name(), defn_type);

    auto result = boost::apply_visitor(*this, let.body());

    return result;
  } // end operator()()

  inline result_type operator()(const syntax::letrec &letrec)
  {
    std::clog << "inferencer(letrec): calling unique_id" << std::endl;
    auto new_type = type_variable(m_environment.unique_id());

    // introduce a scope with a non generic variable
    auto s = scoped_non_generic_variable(this, letrec.name(), new_type);

    auto definition_type = boost::apply_visitor(*this, letrec.definition());

    // new_type = definition_type
    unification::unify(new_type, definition_type, m_substitution);

    auto result = boost::apply_visitor(*this, letrec.body());

    return result;
  }

  struct scoped_generic
  {
    inline scoped_generic(inferencer *inf,
                          const std::string &name,
                          const type &t)
      : m_environment(inf->m_environment)
    {
      auto iter = m_environment.find(name);

      if(iter != m_environment.end())
      {
        // the key already exists
        m_restore = std::make_tuple(true, iter, iter->second);
        iter->second = t;
      } // end if
      else
      {
        // the key does not exist
        auto kv = std::make_pair(name,t);
        m_restore = std::make_tuple(false, m_environment.insert(kv).first, type());
      } // end else
    } // end scoped_generic()

    inline ~scoped_generic()
    {
      using namespace std;

      if(get<0>(m_restore))
      {
        auto iter = get<1>(m_restore);
        auto val = get<2>(m_restore);

        iter->second = val;
      } // end if
      else
      {
        auto iter = get<1>(m_restore);
        m_environment.erase(iter);
      } // end else
    } // end ~scoped_generic()

    environment &m_environment;
    std::tuple<bool, environment::iterator, type> m_restore;
  };

  struct scoped_non_generic_variable
    : scoped_generic
  {
    inline scoped_non_generic_variable(inferencer *inf,
                                       const std::string &name,
                                       const type_variable &var)
      : scoped_generic(inf, name, var),
        m_non_generic(inf->m_non_generic_variables),
        m_erase_me(m_non_generic.insert(var))
    {}

    inline ~scoped_non_generic_variable()
    {
      if(m_erase_me.second)
      {
        m_non_generic.erase(m_erase_me.first);
      } // end if
    } // end ~scoped_non_generic_variable()

    std::set<type_variable>                           &m_non_generic;
    std::pair<std::set<type_variable>::iterator, bool> m_erase_me;
  };

  environment                         m_environment;
  std::set<type_variable>             m_non_generic_variables;
  std::map<type_variable,type>        m_substitution;
};

type infer_type(const syntax::node &node,
                const environment &env)
{
  auto v = inferencer(env);
  auto old = std::clog.rdbuf(0);
  auto result = boost::apply_visitor(v, node);
  std::clog.rdbuf(old);
  return result;
}

} // end inference


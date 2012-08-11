#pragma once

#include <string>
#include <iostream>
#include <boost/variant.hpp>

namespace syntax
{

class integer_literal
{
  public:
    inline integer_literal(const int v)
      : m_value(v)
    {}

    int value(void) const
    {
      return m_value;
    }

  private:
    int m_value;
};

inline std::ostream &operator<<(std::ostream &os, const integer_literal &il)
{
  return os << il.value();
} // end operator<<()


class identifier
{
  public:
    inline identifier(const std::string &name)
      : m_name(name)
    {}

    inline const std::string &name(void) const
    {
      return m_name;
    }

  private:
    std::string m_name;
};

inline std::ostream &operator<<(std::ostream &os, const identifier &i)
{
  return os << i.name();
}

class apply;
class lambda;
class let;
class letrec;

typedef boost::variant<
  integer_literal,
  identifier,
  boost::recursive_wrapper<apply>,
  boost::recursive_wrapper<lambda>,
  boost::recursive_wrapper<let>,
  boost::recursive_wrapper<letrec>
> node;

class apply
{
  public:
    inline apply(node &&fn,
                 node &&arg)
      : m_fn(std::move(fn)),
        m_arg(std::move(arg))
    {}

    inline apply(const node &fn,
                 const node &arg)
      : m_fn(fn),
        m_arg(arg)
    {}

    inline const node &function(void) const
    {
      return m_fn;
    }

    inline const node &argument(void) const
    {
      return m_arg;
    }

  private:
    node m_fn, m_arg;
};

inline std::ostream &operator<<(std::ostream &os, const apply &a)
{
  return os << "(" << a.function() << " " << a.argument() << ")";
}

class lambda
{
  public:
    inline lambda(const std::string &param,
                  node &&body)
      : m_param(param),
        m_body(std::move(body))
    {}

    inline lambda(const std::string &param,
                  const node &body)
      : m_param(param),
        m_body(body)
    {}

    inline const std::string &parameter(void) const
    {
      return m_param;
    }

    inline const node &body(void) const
    {
      return m_body;
    }

  private:
    std::string m_param;
    node m_body;
};

inline std::ostream &operator<<(std::ostream &os, const lambda &l)
{
  return os << "(fn " << l.parameter() << " => " << l.body() << ")";
}

class let
{
  public:
    inline let(const std::string &name,
               node &&def,
               node &&body)
      : m_name(name),
        m_definition(std::move(def)),
        m_body(std::move(body))
    {}

    inline let(const std::string &name,
               const node &def,
               const node &body)
      : m_name(name),
        m_definition(def),
        m_body(body)
    {}

    inline const std::string &name() const
    {
      return m_name;
    }

    inline const node &definition() const
    {
      return m_definition;
    }

    inline const node &body() const
    {
      return m_body;
    }

  private:
    std::string m_name;
    node m_definition, m_body;
};

inline std::ostream &operator<<(std::ostream &os, const let &l)
{
  return os << "(let " << l.name() << " = " << l.definition() << " in " << l.body() << ")";
}

class letrec
{
  public:
    inline letrec(const std::string &name,
                  node &&def,
                  node &&body)
      : m_name(name),
        m_definition(std::move(def)),
        m_body(std::move(body))
    {}

    inline letrec(const std::string &name,
                  const node &def,
                  const node &body)
      : m_name(name),
        m_definition(def),
        m_body(body)
    {}

    inline const std::string &name() const
    {
      return m_name;
    }

    inline const node &definition() const
    {
      return m_definition;
    }

    inline const node &body() const
    {
      return m_body;
    }

  private:
    std::string m_name;
    node m_definition, m_body;
};

inline std::ostream &operator<<(std::ostream &os, const letrec &l)
{
  return os << "(letrec " << l.name() << " = " << l.definition() << " in " << l.body() << ")";
}

} // end syntax


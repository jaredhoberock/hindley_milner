#include <sstream>
#include "unification.hpp"
#include "syntax.hpp"
#include "inference.hpp"

namespace types
{
  static const int integer  = 0;
  static const int boolean  = 1;
  static const int function = 2;
  static const int pair     = 3;
}

class pretty_printer
  : public boost::static_visitor<pretty_printer&>
{
  public:
    inline pretty_printer(std::ostream &os)
      : m_os(os),
        m_next_name('a')
    {}

    inline pretty_printer &operator()(const unification::type_variable &x)
    {
      if(!m_names.count(x))
      {
        std::ostringstream os;
        os << m_next_name++;
        m_names[x] = os.str();
      } // end if

      m_os << m_names[x];
      return *this;
    }

    inline pretty_printer &operator()(const unification::type_operator &x)
    {
      switch(x.kind())
      {
        case types::integer:
        {
          m_os << "int";
          break;
        } // end case
        case types::boolean:
        {
          m_os << "bool";
          break;
        } // end case
        case types::function:
        {
          m_os << "(";
          *this << x[0];
          m_os << " -> ";
          *this << x[1];
          m_os << ")";
          break;
        } // end case
        case types::pair:
        {
          m_os << "(";
          *this << x[0];
          m_os << " * ";
          *this << x[1];
          m_os << ")";
          break;
        } // end case
        default:
        {
        } // end default
      } // end switch

      return *this;
    }

    inline pretty_printer &operator<<(const unification::type &x)
    {
      return boost::apply_visitor(*this, x);
    }

    inline pretty_printer &operator<<(std::ostream & (*fp)(std::ostream &))
    {
      fp(m_os);
      return *this;
    }

    template<typename T>
    inline pretty_printer &operator<<(const T &x)
    {
      m_os << x;
      return *this;
    }

  private:
    std::ostream &m_os;

    std::map<unification::type_variable, std::string> m_names;
    char m_next_name;
};

namespace unification
{

inline std::ostream &operator<<(std::ostream &os, const type_variable &x)
{
  pretty_printer pp(os);
  pp << type(x);
  return os;
}

inline std::ostream &operator<<(std::ostream &os, const type_operator &x)
{
  pretty_printer pp(os);
  pp << type(x);
  return os;
}

}

struct try_to_infer
{
  inline try_to_infer(const inference::environment &e)
    : env(e)
  {}

  inline void operator()(const syntax::node &n) const
  {
    try
    {
      auto result = inference::infer_type(n, env);

      std::cout << n << " : ";
      pretty_printer pp(std::cout);
      pp << result << std::endl;
    } // end try
    catch(const unification::recursive_unification &e)
    {
      std::cerr << n << " : ";
      pretty_printer pp(std::cerr);
      pp << e.what() << ": " << e.x << " in " << e.y << std::endl;
    } // end catch
    catch(const unification::type_mismatch &e)
    {
      std::cerr << n << " : ";
      pretty_printer pp(std::cerr);
      pp << e.what() << ": " << e.x << " != " << e.y << std::endl;
    } // end catch
    catch(const std::runtime_error &e)
    {
      std::cerr << n << " : " << e.what() << std::endl;
    } // end catch
  } // end operator()

  const inference::environment &env;
};

int main()
{
  using namespace unification;
  using namespace syntax;
  using namespace inference;

  environment env;
  std::vector<node> examples;

  auto var1 = type_variable(env.unique_id());
  auto var2 = type_variable(env.unique_id());
  auto var3 = type_variable(env.unique_id());

  env["pair"] = make_function(var1, inference::make_function(var2, pair(var1, var2)));
  env["true"] = boolean();
  env["cond"] = make_function(
                 boolean(),
                  make_function(
                    var3, make_function(
                      var3, var3
                    )
                  )
                );
  env["zero"] = make_function(integer(), boolean());
  env["pred"] = make_function(integer(), integer());
  env["times"] = make_function(
                   integer(), make_function(
                     integer(), integer()
                   )
                 );

  auto pair = apply(apply(identifier("pair"), apply(identifier("f"), integer_literal(4))), apply(identifier("f"), identifier("true")));

  // factorial
  {
    auto example =
      letrec("factorial",
        lambda("n",
          apply(
            apply(
              apply(identifier("cond"),
                apply(identifier("zero"), identifier("n"))
              ),
              integer_literal(1)
            ),
            apply(
              apply(identifier("times"), identifier("n")),
              apply(identifier("factorial"),
                apply(identifier("pred"), identifier("n"))
              )
            )
          )
        ),
        apply(identifier("factorial"), integer_literal(5))
      );
    examples.push_back(example);
  }
  
  // fn x => (pair(x(3) (x(true)))
  {
    auto example = lambda("x",
      apply(
        apply(identifier("pair"),
          apply(identifier("x"), integer_literal(3))),
        apply(identifier("x"), identifier("true"))));
    examples.push_back(example);
  }

  // pair(f(3), f(true))
  {
    auto example =
      apply(
        apply(identifier("pair"), apply(identifier("f"), integer_literal(4))),
        apply(identifier("f"), identifier("true"))
      );
    examples.push_back(example);
  }

  // let f = (fn x => x) in ((pair (f 4)) (f true))
  {
    auto example = let("f", lambda("x", identifier("x")), pair);
    examples.push_back(example);
  }

  // fn f => f f (fail)
  {
    auto example = lambda("f", apply(identifier("f"), identifier("f")));
    examples.push_back(example);
  }

  // let g = fn f => 5 in g g
  {
    auto example = let("g",
                       lambda("f", integer_literal(5)),
                       apply(identifier("g"), identifier("g")));
    examples.push_back(example);
  }
  
  // example that demonstrates generic and non-generic variables
  // fn g => let f = fn x => g in pair (f 3, f true)
  {
    auto example =
      lambda("g",
        let("f",
          lambda("x", identifier("g")),
          apply(
            apply(identifier("pair"),
              apply(identifier("f"), integer_literal(3))
            ),
            apply(identifier("f"), identifier("true"))
          )
        )
      );
    examples.push_back(example);
  }

  // function composition
  // fn f (fn g (fn arg (f g arg)))
  {
    auto example = lambda("f", lambda("g", lambda("arg", apply(identifier("g"), apply(identifier("f"), identifier("arg"))))));
    examples.push_back(example);
  }

  // fn f => f 5
  {
    auto example = lambda("f", apply(identifier("f"), integer_literal(5)));
    examples.push_back(example);
  }

  // f = fn x => 1
  // g = fn y => y 1
  // (g f)
  {
    auto return_one = lambda("x", integer_literal(1));
    auto apply_one  = lambda("y", apply(identifier("y"), integer_literal(1)));
    auto example = apply(apply_one, return_one);
    examples.push_back(example);
  }

  auto f = try_to_infer(env);
  std::for_each(examples.begin(), examples.end(), f);

  return 0;
}


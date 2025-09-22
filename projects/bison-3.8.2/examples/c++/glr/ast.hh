/*
  Copyright (C) 2020-2021 Free Software Foundation, Inc.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <cstddef> // nullptr_t
#include <iostream>
#include <memory>

// Type erasure 101 <https://stackoverflow.com/a/26199467/1353549>.
class NodeInterface;

class Node
{
public:
  Node (const Node& node) = default;
  Node (Node&& node) = default;
  Node () = default;
  ~Node () = default;

  template <typename T,
            // SFINAE block using this ctor as a copy/move ctor:
            std::enable_if_t<!std::is_same<Node, std::decay_t<T>>::value, int>* = nullptr>
  Node (T&& t);

  Node& operator= (const Node& node) = default;
  Node& operator= (Node&& node) = default;

  explicit operator bool () const
  {
    return impl_ != nullptr;
  }

  std::ostream& print (std::ostream& o) const;

  std::shared_ptr<NodeInterface> impl_;
};

static std::ostream&
operator<< (std::ostream& o, const Node &node)
{
  return node.print (o);
}

class NodeInterface
{
public:
  virtual ~NodeInterface () = default;
  virtual std::ostream& print (std::ostream& o) const = 0;
};


std::ostream& Node::print (std::ostream& o) const
{
  if (impl_)
    impl_->print (o);
  return o;
}


template <typename T,
          std::enable_if_t<!std::is_same<std::nullptr_t, std::decay_t<T>>::value, int>* = nullptr>
struct NodeImpl : public NodeInterface
{
  template <typename U>
  explicit NodeImpl (U&& u)
    : t{std::forward<U> (u)}
  {}
  virtual ~NodeImpl () = default;
  virtual std::ostream& print (std::ostream& o) const
  {
    return o << t;
  }

  T t;
};


template <typename T,
          std::enable_if_t<!std::is_same<Node, std::decay_t<T>>::value, int>*>
Node::Node (T&& t)
  : impl_ (new NodeImpl<std::decay_t<T>>{std::forward<T> (t)})
{}

class Nterm
{
public:
  Nterm (std::string form,
         Node child0 = Node (), Node child1 = Node (), Node child2 = Node ())
    : form_ (std::move (form))
  {
    children_[0] = child0;
    children_[1] = child1;
    children_[2] = child2;
  }

  friend std::ostream& operator<< (std::ostream& o, const Nterm& t)
  {
    o << t.form_;
    if (t.children_[0])
      {
        o << '(' << t.children_[0];
        if (t.children_[1])
          o << ", " << t.children_[1];
        if (t.children_[2])
          o << ", " << t.children_[2];
        o << ')';
      }
    return o;
  }

private:
  std::string form_;
  Node children_[3];
};



class Term
{
public:
  Term (std::string text)
    : text_ (std::move (text))
  {}

  friend std::ostream& operator<< (std::ostream& o, const Term& t)
  {
    return o << t.text_;
  }

private:
  std::string text_;
};

#ifdef TEST
int main ()
{
  Node n0;
  std::cout << n0 << '\n';

  Node n;
  n = n0;
  std::cout << n0 << '\n';

  Term t1 = Term ("T");
  std::cout << t1 << '\n';

  n = t1;
  std::cout << n << '\n';
  std::cout << Nterm ("+", t1, t1) << '\n';

  auto n1
    = Nterm ("<OR>",
             Nterm ("<declare>", Term ("T"), Term ("x")),
             Nterm ("<cast>", Term ("x"), Term ("T")));
  std::cout << n1 << '\n';

  n = n1;
  std::cout << n1 << '\n';
}
#endif

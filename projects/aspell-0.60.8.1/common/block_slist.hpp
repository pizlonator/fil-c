// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef autil__block_slist_hh
#define autil__block_slist_hh

// BlockSList provided a pool of nodes which can be used for singly
//   linked lists.  Nodes are allocated in large chunks instead of
//   one at a time which means that getting a new node is very fast.
//   Nodes returned are not initialized in any way.  A placement new
//   must be used to construct the data member and the destructor
//   must explicitly be called.

namespace acommon {

  template <typename T>
  class BlockSList {

  public:

    struct Node {
      Node * next;
      T      data;
    };

  private:

    void * first_block;
    Node * first_available;

    BlockSList(const BlockSList &);
    BlockSList & operator= (const BlockSList &);
  
  public:

    BlockSList() 
      // Default constructor.
      // add_block must be called before any nodes are available
      : first_block(0), first_available(0) {};

    Node * new_node() 
      // return a new_node if one is available or null otherwise
      // Note: the node's data member is NOT initialized in any way.
    {
      Node * n = first_available;
      if (n != 0)
	first_available = first_available->next;
      return n;
    }

    void remove_node(Node * n) 
      // marks the node as available
      // Note: the node's data member destructor is NOT called
    {
      n->next = first_available;
      first_available = n;
    }

    void add_block(unsigned int num);
    // allocate enough memory for an additional num nodes

    void clear();
    // free all memory.  add_block must then be called before any nodes
    // are available
    // Note: the node's data member destructor is NOT called
    

    ~BlockSList() {clear();}
  
  };

}
#endif

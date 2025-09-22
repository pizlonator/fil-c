/* Copyright (C) 2001, 2002, 2003 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2001.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifndef LIST_H
#define LIST_H	1

/* Add element to the end of a circular, double-linked list.  */
#define CDBL_LIST_ADD_REAR(first, newp) \
  do {									      \
    __typeof (newp) _newp = (newp);					      \
    assert (_newp->next == NULL);					      \
    assert (_newp->previous == NULL);					      \
    if (unlikely ((first) == NULL))					      \
      (first) = _newp->next = _newp->previous = _newp;			      \
    else								      \
      {									      \
	_newp->next = (first);						      \
	_newp->previous = (first)->previous;				      \
	_newp->previous->next = _newp->next->previous = _newp;		      \
      }									      \
  } while (0)

/* Remove element from circular, double-linked list.  */
#define CDBL_LIST_DEL(first, elem) \
  do {									      \
    __typeof (elem) _elem = (elem);					      \
    /* Check whether the element is indeed on the list.  */		      \
    assert (first != NULL && _elem != NULL				      \
	    && (first != elem						      \
		|| ({ __typeof (elem) _runp = first->next;		      \
		      while (_runp != first)				      \
			if (_runp == _elem)				      \
			  break;					      \
			else						      \
		          _runp = _runp->next;				      \
		      _runp == _elem; })));				      \
    if (unlikely (_elem->next == _elem))				      \
      first = NULL;							      \
    else								      \
      {									      \
	_elem->next->previous = _elem->previous;			      \
	_elem->previous->next = _elem->next;				      \
	if (unlikely (first == _elem))					      \
	  first = _elem->next;						      \
      }									      \
     assert ((_elem->next = _elem->previous = NULL, 1));		      \
  } while (0)


/* Add element to the front of a single-linked list.  */
#define SNGL_LIST_PUSH(first, newp) \
  do {									      \
    __typeof (newp) _newp = (newp);					      \
    assert (_newp->next == NULL);					      \
    _newp->next = first;						      \
    first = _newp;							      \
  } while (0)


/* Add element to the rear of a circular single-linked list.  */
#define CSNGL_LIST_ADD_REAR(first, newp) \
  do {									      \
    __typeof (newp) _newp = (newp);					      \
    assert (_newp->next == NULL);					      \
    if (unlikely ((first) == NULL))					      \
      (first) = _newp->next = _newp;					      \
    else								      \
      {									      \
	_newp->next = (first)->next;					      \
	(first) = (first)->next = _newp;				      \
      }									      \
  } while (0)


#endif	/* list.h */

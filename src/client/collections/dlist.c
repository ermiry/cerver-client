#include <stdlib.h>
#include <stdbool.h>

#include <pthread.h>

#include "client/collections/dlist.h"

static inline void list_element_delete (ListElement *le);

#pragma region internal

static ListElement *list_element_new (void) {

	ListElement *le = (ListElement *) malloc (sizeof (ListElement));
	if (le) {
		le->next = le->prev = NULL;
		le->data = NULL;
	}

	return le;

}

static inline void list_element_delete (ListElement *le) { if (le) free (le); }

static DoubleList *dlist_new (void) {

	DoubleList *dlist = (DoubleList *) malloc (sizeof (DoubleList));
	if (dlist) {
		dlist->size = 0;
		dlist->start = NULL;
		dlist->end = NULL;
		dlist->destroy = NULL;
		dlist->compare = NULL;

		dlist->mutex = NULL;
	}

	return dlist;

}

static int dlist_internal_insert_before (
	DoubleList *dlist, ListElement *element, const void *data
) {

	int retval = 1;

	ListElement *le = list_element_new ();
	if (le) {
		le->data = (void *) data;

		if (element == NULL || !element->prev) {
			if (dlist->size == 0) dlist->end = le;
			else dlist->start->prev = le;
		
			le->next = dlist->start;
			le->prev = NULL;
			dlist->start = le;
		}

		else {
			element->prev->next = le;
			le->next = element;
			le->prev = element->prev;
			element->prev = le;
		}

		dlist->size++;

		retval = 0;
	}

	return retval;

}

static void dlist_internal_insert_after_actual (
	DoubleList *dlist, ListElement *element, ListElement *le
) {

	if (element == NULL) {
		if (dlist->size == 0) dlist->end = le;
		else dlist->start->prev = le;
	
		le->next = dlist->start;
		le->prev = NULL;
		dlist->start = le;
	}

	else {
		if (element->next == NULL) dlist->end = le;

		le->next = element->next;
		le->prev = element;
		element->next = le;
	}

}

static int dlist_internal_insert_after (
	DoubleList *dlist, ListElement *element, const void *data
) {

	int retval = 1;

	ListElement *le = list_element_new ();
	if (le) {
		le->data = (void *) data;

		dlist_internal_insert_after_actual (
			dlist, element, le
		);

		dlist->size++;

		retval = 0;
	}

	return retval;

}

static ListElement *dlist_internal_remove_element_actual (
	DoubleList *dlist, ListElement *element, void **data
) {

	ListElement *old = NULL;

	if (element == NULL) {
		*data = dlist->start->data;
		old = dlist->start;
		dlist->start = dlist->start->next;
		if (dlist->start != NULL) dlist->start->prev = NULL;
	}

	else {
		*data = element->data;
		old = element;

		ListElement *prevElement = element->prev;
		ListElement *nextElement = element->next;

		if (prevElement != NULL && nextElement != NULL) {
			prevElement->next = nextElement;
			nextElement->prev = prevElement;
		}

		else {
			// we are at the start of the dlist
			if (prevElement == NULL) {
				if (nextElement != NULL) nextElement->prev = NULL;
				dlist->start = nextElement;
			}

			// we are at the end of the dlist
			if (nextElement == NULL) {
				if (prevElement != NULL) prevElement->next = NULL;
				dlist->end = prevElement;
			}
		}
	}

	return old;

}

static void *dlist_internal_remove_element (
	DoubleList *dlist, ListElement *element
) {

	void *data = NULL;
	if (dlist->size > 0) {
		list_element_delete (
			dlist_internal_remove_element_actual (
				dlist, element, &data
			)
		);

		dlist->size--;

		if (dlist->size == 0) {
			dlist->start = NULL;
			dlist->end = NULL;
		}
	}

	return data;

}

// moves the list element from one list to the other
static void dlist_internal_move_element (
	DoubleList *source, DoubleList *dest,
	ListElement *element_to_remove,
	ListElement *insert_after_this
) {

	// remove list element from source dlist
	void *dummy_data = NULL;
	ListElement *removed = dlist_internal_remove_element_actual (
		source, element_to_remove, &dummy_data
	);

	source->size--;

	if (source->size == 0) {
		source->start = NULL;
		source->end = NULL;
	}

	// insert list element into dest dlist
	dlist_internal_insert_after_actual (
		dest, insert_after_this, removed
	);

	dest->size++;

}

static void dlist_internal_move_matches (
	DoubleList *dlist, DoubleList *matches,
	bool (*compare)(const void *one, const void *two),
	const void *match
) {

	size_t count = 0;
	size_t original_size = dlist->size;
	ListElement *le = dlist->start;
	ListElement *next = NULL;
	while (count < original_size) {
		next = le->next;

		if (compare (le->data, match)) {
			// move the list element from one list to the other
			dlist_internal_move_element (
				dlist, matches,
				le, matches->end
			);
		}

		le = next;
		count += 1;
	}

}

static void dlist_internal_merge_two (
	DoubleList *one, DoubleList *two
) {

	if (one->size) {
		if (two->size) {
			one->end->next = two->start;
			one->end = two->end;
		}
		
		else one->end->next = NULL;
	}

	else {
		one->start = two->start;
		one->end = two->end;
	}

	one->size += two->size;

	two->start = two->end = NULL;
	two->size = 0;

}

static void dlist_internal_remove_elements (DoubleList *dlist) {

	void *data = NULL;
	while (dlist->size > 0) {
		data = dlist_internal_remove_element (dlist, NULL);
		if (data) {
			if (dlist->destroy) dlist->destroy (data);
		}
	}

}

static void dlist_internal_delete (DoubleList *dlist) {

	dlist_internal_remove_elements (dlist);

	(void) pthread_mutex_unlock (dlist->mutex);
	pthread_mutex_destroy (dlist->mutex);
	free (dlist->mutex);

	free (dlist);

}

#pragma endregion

void dlist_set_compare (
	DoubleList *dlist, int (*compare)(const void *one, const void *two)
) { 
	
	if (dlist) dlist->compare = compare;
	
}

void dlist_set_destroy (
	DoubleList *dlist, void (*destroy)(void *data)
) { 
	
	if (dlist) dlist->destroy = destroy;
	
}

size_t dlist_size (const DoubleList *dlist) {

	size_t retval = 0;

	if (dlist) {
		(void) pthread_mutex_lock (dlist->mutex);

		retval = dlist->size;

		(void) pthread_mutex_unlock (dlist->mutex);
	}

	return retval;

}

bool dlist_is_empty (const DoubleList *dlist) { 
	
	bool retval = true;

	if (dlist) {
		(void) pthread_mutex_lock (dlist->mutex);

		retval = (dlist->size == 0);

		(void) pthread_mutex_unlock (dlist->mutex);
	}

	return retval;
	
}

bool dlist_is_not_empty (const DoubleList *dlist) {

	bool retval = false;

	if (dlist) {
		(void) pthread_mutex_lock (dlist->mutex);

		retval = (dlist->size > 0);

		(void) pthread_mutex_unlock (dlist->mutex);
	}

	return retval;

}

void dlist_delete (void *dlist_ptr) {

	if (dlist_ptr) {
		DoubleList *dlist = (DoubleList *) dlist_ptr;

		(void) pthread_mutex_lock (dlist->mutex);

		dlist_internal_delete (dlist);
	}

}

// only deletes the list if its empty (size == 0)
// returns 0 on success, 1 on NOT deleted
int dlist_delete_if_empty (void *dlist_ptr) {

	int retval = 1;

	if (dlist_ptr) {
		// if (((DoubleList *) dlist_ptr)->size == 0) dlist_delete (dlist_ptr);

		DoubleList *dlist = (DoubleList *) dlist_ptr;

		(void) pthread_mutex_lock (dlist->mutex);

		if (dlist->size == 0) {
			dlist_internal_delete (dlist);

			retval = 0;
		}

		else {
			(void) pthread_mutex_unlock (dlist->mutex);
		}
	}

	return retval;

}

// only deletes the list if its NOT empty (size > 0)
// returns 0 on success, 1 on NOT deleted
int dlist_delete_if_not_empty (void *dlist_ptr) {

	int retval = 1;

	if (dlist_ptr) {
		// if (((DoubleList *) dlist_ptr)->size > 0) dlist_delete (dlist_ptr);

		DoubleList *dlist = (DoubleList *) dlist_ptr;

		(void) pthread_mutex_lock (dlist->mutex);

		if (dlist->size > 0) {
			dlist_internal_delete (dlist);

			retval = 0;
		}

		else {
			(void) pthread_mutex_unlock (dlist->mutex);
		}
	}

	return retval;

}

DoubleList *dlist_init (
	void (*destroy)(void *data),
	int (*compare)(const void *one, const void *two)
) {

	DoubleList *dlist = dlist_new ();

	if (dlist) {
		dlist->destroy = destroy;
		dlist->compare = compare;

		dlist->mutex = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
		pthread_mutex_init (dlist->mutex, NULL);
	}

	return dlist;

}

// destroys all of the dlist's elements and their data but keeps the dlist
void dlist_reset (DoubleList *dlist) {

	if (dlist) {
		(void) pthread_mutex_lock (dlist->mutex);

		if (dlist->size > 0) {
			void *data = NULL;
			while (dlist->size > 0) {
				data = dlist_internal_remove_element (dlist, NULL);
				if (data != NULL && dlist->destroy != NULL) dlist->destroy (data);
			}
		}

		dlist->start = NULL;
		dlist->end = NULL;
		dlist->size = 0;

		(void) pthread_mutex_unlock (dlist->mutex);
	}

}

// only gets rid of the list elements, but the data is kept
// this is usefull if another dlist or structure points to the same data
void dlist_clear (void *dlist_ptr) {

	if (dlist_ptr) {
		DoubleList *dlist = (DoubleList *) dlist_ptr;

		(void) pthread_mutex_lock (dlist->mutex);

		while (dlist->size > 0) 
			(void) dlist_internal_remove_element (dlist, NULL);

		(void) pthread_mutex_unlock (dlist->mutex);
	}

}

// clears the dlist - only gets rid of the list elements, but the data is kept
// and then deletes the dlist
void dlist_clear_and_delete (void *dlist_ptr) {

	if (dlist_ptr) {
		dlist_clear (dlist_ptr);
		dlist_delete (dlist_ptr);
	}

}

// clears the dlist if it is NOT empty
// deletes the dlist if it is EMPTY
void dlist_clear_or_delete (void *dlist_ptr) {

	if (dlist_ptr) {
		if (((DoubleList *) dlist_ptr)->size > 0) dlist_clear (dlist_ptr);
		else dlist_delete (dlist_ptr);
	}

}

/*** insert ***/

// inserts the data in the double list BEFORE the specified element
// if element == NULL, data will be inserted at the start of the list
// returns 0 on success, 1 on error
int dlist_insert_before (
	DoubleList *dlist,
	ListElement *element, const void *data
) {

	int retval = 1;

	if (dlist && data) {
		(void) pthread_mutex_lock (dlist->mutex);

		retval = dlist_internal_insert_before (
			dlist, element, data
		);

		(void) pthread_mutex_unlock (dlist->mutex);
	}

	return retval;

}

// works as dlist_insert_before ()
// this method is NOT thread safe
// returns 0 on success, 1 on error
int dlist_insert_before_unsafe (
	DoubleList *dlist,
	ListElement *element, const void *data
) {

	return (dlist && data) ? dlist_internal_insert_before (
		dlist, element, data
	) : 1;

}

// inserts the data in the double list AFTER the specified element
// if element == NULL, data will be inserted at the start of the list
// returns 0 on success, 1 on error
int dlist_insert_after (
	DoubleList *dlist,
	ListElement *element, const void *data
) {

	int retval = 1;

	if (dlist && data) {
		(void) pthread_mutex_lock (dlist->mutex);

		retval = dlist_internal_insert_after (
			dlist, element, data
		);

		(void) pthread_mutex_unlock (dlist->mutex);
	}

	return retval;

}

// works as dlist_insert_after ()
// this method is NOT thread safe
// returns 0 on success, 1 on error
int dlist_insert_after_unsafe (
	DoubleList *dlist,
	ListElement *element, const void *data
) {

	return (dlist && data) ? dlist_internal_insert_after (
		dlist, element, data
	) : 1;

}

// inserts the data in the double list in the specified pos (0 indexed)
// if the pos is greater than the current size, it will be added at the end
// returns 0 on success, 1 on error
int dlist_insert_at (
	DoubleList *dlist,
	const void *data, const unsigned int pos
) {

	int retval = 1;

	if (dlist && data) {
		// insert at the start of the list
		if (pos == 0) retval = dlist_insert_before (dlist, NULL, data);
		else if (pos > dlist->size) {
			// insert at the end of the list
			retval = dlist_insert_after (dlist, dlist_end (dlist), data);
		}

		else {
			unsigned int count = 0;
			ListElement *le = dlist_start (dlist);
			while (le) {
				if (count == pos) break;
				
				count++;
				le = le->next;
			}

			retval = dlist_insert_before (dlist, le, data);
		}
	}

	return retval;

}

// inserts at the start of the dlist, before the first element
// returns 0 on success, 1 on error
int dlist_insert_at_start (
	DoubleList *dlist, const void *data
) {

	int retval = 1;

	if (dlist && data) {
		(void) pthread_mutex_lock (dlist->mutex);

		retval = dlist_internal_insert_before (
			dlist, NULL, data
		);

		(void) pthread_mutex_unlock (dlist->mutex);
	}

	return retval;

}

// inserts at the start of the dlist, before the first element
// this method is NOT thread safe
// returns 0 on success, 1 on error
int dlist_insert_at_start_unsafe (
	DoubleList *dlist, const void *data
) {

	return (dlist && data) ? dlist_internal_insert_before (
		dlist, NULL, data
	) : 1;

}

// inserts at the end of the dlist, after the last element
// returns 0 on success, 1 on error
int dlist_insert_at_end (
	DoubleList *dlist, const void *data
) {

	int retval = 1;

	if (dlist && data) {
		(void) pthread_mutex_lock (dlist->mutex);

		retval = dlist_internal_insert_after (
			dlist, dlist->end, data
		);

		(void) pthread_mutex_unlock (dlist->mutex);
	}

	return retval;

}

// inserts at the end of the dlist, after the last element
// this method is NOT thread safe
// returns 0 on success, 1 on error
int dlist_insert_at_end_unsafe (
	DoubleList *dlist, const void *data
) {

	return (dlist && data) ? dlist_internal_insert_after (
		dlist, dlist->end, data
	) : 1;

}

static inline int dlist_insert_in_order_actual (
	DoubleList *dlist,
	bool first, ListElement *le,
	const void *data
) {

	int retval = 1;

	switch (dlist->compare (le->data, data)) {
		case -1:
		case 0: {
			if (le == dlist->end) {
				retval = dlist_internal_insert_after (
					dlist, le, data
				);
			}
		} break;

		case 1: {
			retval = dlist_internal_insert_before (
				dlist, first ? NULL : le, data
			);
		} break;
	}

	return retval;

}

// uses de dlist's comparator method to insert new data in the correct position
// this method is thread safe
// returns 0 on success, 1 on error
int dlist_insert_in_order (
	DoubleList *dlist, const void *data
) {
	
	int retval = 1;

	if (dlist && data) {
		if (dlist->compare) {
			(void) pthread_mutex_lock (dlist->mutex);

			if (dlist->size) {
				bool first = true;
				ListElement *le = dlist->start;
				while (le) {
					if (!dlist_insert_in_order_actual (
						dlist, first, le, data
					)) {
						retval = 0;
						break;
					}

					le = le->next;
					first = false;
				}
			}

			else {
				retval = dlist_internal_insert_after (
					dlist, NULL, data
				);
			}

			(void) pthread_mutex_unlock (dlist->mutex);
		}
	}

	return retval;

}

/*** remove ***/

// finds the data using the query and the list comparator and the removes it from the list
// and returns the list element's data
// option to pass a custom compare method for searching, if NULL, dlist's compare method will be used
void *dlist_remove (
	DoubleList *dlist,
	const void *query, int (*compare)(const void *one, const void *two)
) {

	void *retval = NULL;

	if (dlist && query) {
		int (*comp)(const void *one, const void *two) = compare ? compare : dlist->compare;

		if (comp) {
			(void) pthread_mutex_lock (dlist->mutex);

			ListElement *ptr = dlist_start (dlist);

			bool first = true;
			while (ptr != NULL) {
				if (!comp (ptr->data, query)) {
					// remove the list element
					void *data = NULL;
					if (first) data = dlist_internal_remove_element (dlist, NULL);
					else data = dlist_internal_remove_element (dlist, ptr);
					if (data) {
						// if (dlist->destroy) dlist->destroy (data);
						// else free (data);

						retval = data;
					}

					break;
				}

				ptr = ptr->next;
				first = false;
			}

			(void) pthread_mutex_unlock (dlist->mutex);
		}
	}

	return retval;

}

// removes the dlist element from the dlist and returns the data
// NULL for the start of the list
void *dlist_remove_element (DoubleList *dlist, ListElement *element) {

	void *data = NULL;

	if (dlist) {
		(void) pthread_mutex_lock (dlist->mutex);

		data = dlist_internal_remove_element (dlist, element);

		(void) pthread_mutex_unlock (dlist->mutex);
	}

	return data;

}

// works as dlist_remove_element ()
// this method is NOT thread safe
void *dlist_remove_element_unsafe (DoubleList *dlist, ListElement *element) {

	return dlist ? dlist_internal_remove_element (dlist, element) : NULL;

}

// removes the element at the start of the dlist
// returns the element's data
void *dlist_remove_start (DoubleList *dlist) {

	void *data = NULL;

	if (dlist) {
		(void) pthread_mutex_lock (dlist->mutex);

		data = dlist_internal_remove_element (dlist, NULL);

		(void) pthread_mutex_unlock (dlist->mutex);
	}

	return data;

}

// works as dlist_remove_start ()
// this method is NOT thread safe
void *dlist_remove_start_unsafe (DoubleList *dlist) {

	return dlist ? dlist_internal_remove_element (dlist, NULL) : NULL;

}

// removes the element at the end of the dlist
// returns the element's data
void *dlist_remove_end (DoubleList *dlist) {

	void *data = NULL;

	if (dlist) {
		(void) pthread_mutex_lock (dlist->mutex);

		data = dlist_internal_remove_element (dlist, dlist->end);

		(void) pthread_mutex_unlock (dlist->mutex);
	}

	return data;

}

// works as dlist_remove_end ()
// this method is NOT thread safe
void *dlist_remove_end_unsafe (DoubleList *dlist) {

	return dlist ? dlist_internal_remove_element (dlist, dlist->end) : NULL;

}

// removes the dlist element from the dlist at the specified index 
// returns the data or NULL if index was invalid
void *dlist_remove_at (DoubleList *dlist, const unsigned int idx) {

	void *retval = NULL;

	if (dlist) {
		if (idx < dlist->size) {
			(void) pthread_mutex_lock (dlist->mutex);

			bool first = true;
			unsigned int i = 0;
			ListElement *ptr = dlist_start (dlist);
			while (ptr != NULL) {
				if (i == idx) {
					// remove the list element
					void *data = NULL;
					if (first) data = dlist_internal_remove_element (dlist, NULL);
					else data = dlist_internal_remove_element (dlist, ptr);
					if (data) {
						retval = data;
					}

					break;
				}

				first = false;
				ptr = ptr->next;
				i++;
			}

			(void) pthread_mutex_unlock (dlist->mutex);
		}
	}

	return retval;

}

// removes all the elements that match the query using the comparator method
// option to delete removed elements data when delete_data is set to TRUE
// comparator must return TRUE on match (item will be removed from the dlist)
// this methods is thread safe
// returns the number of elements that we removed from the dlist
unsigned int dlist_remove_by_condition (
	DoubleList *dlist,
	bool (*compare)(const void *one, const void *two),
	const void *match,
	bool delete_data
) {

	unsigned int matches = 0;

	if (dlist && compare) {
		(void) pthread_mutex_lock (dlist->mutex);

		size_t original_size = dlist->size;
		size_t count = 0;
		ListElement *le = dlist->start;
		ListElement *next = NULL;
		while (count < original_size) {
			next = le->next;

			if (compare (le->data, match)) {
				// remove element from the dlist
				void *removed_data = dlist_internal_remove_element (dlist, le);
				if (delete_data) {
					if (dlist->destroy) {
						dlist->destroy (removed_data);
					}
				}

				matches += 1;
			}

			le = next;
			count += 1;
		}

		(void) pthread_mutex_unlock (dlist->mutex);
	}

	return matches;

}

/*** traverse --- search ***/

// traverses the dlist and for each element, calls the method by passing the list element data and the method args as both arguments
// this method is thread safe
// returns 0 on success, 1 on error
int dlist_traverse (
	const DoubleList *dlist,
	void (*method)(void *list_element_data, void *method_args), void *method_args
) {

	int retval = 1;

	if (dlist && method) {
		(void) pthread_mutex_lock (dlist->mutex);

		for (ListElement *le = dlist_start (dlist); le; le = le->next) {
			method (le->data, method_args);
		}

		(void) pthread_mutex_unlock (dlist->mutex);

		retval = 0;
	}
	
	return retval;

}

// uses the list comparator to search using the data as the query
// option to pass a custom compare method for searching, if NULL, dlist's compare method will be used
// returns the double list's element data
void *dlist_search (
	const DoubleList *dlist,
	const void *data,
	int (*compare)(const void *one, const void *two)
) {

	if (dlist && data) {
		int (*comp)(const void *one, const void *two) = compare ? compare : dlist->compare;

		if (comp) {
			ListElement *ptr = dlist_start (dlist);

			while (ptr != NULL) {
				if (!comp (ptr->data, data)) return ptr->data;
				ptr = ptr->next;
			}
		}
	}

	return NULL;    

}

// searches the dlist and returns the dlist element associated with the data
// option to pass a custom compare method for searching, if NULL, dlist's compare method will be used
ListElement *dlist_get_element (
	const DoubleList *dlist, const void *data, 
	int (*compare)(const void *one, const void *two)
) {

	if (dlist && data) {
		int (*comp)(const void *one, const void *two) = compare ? compare : dlist->compare;

		if (comp) {
			ListElement *ptr = dlist_start (dlist);
			while (ptr != NULL) {
				if (!dlist->compare (ptr->data, data)) return ptr;
				ptr = ptr->next;
			}
		}
	}

	return NULL;

}

// traverses the dlist and returns the list element at the specified index
ListElement *dlist_get_element_at (const DoubleList *dlist, const unsigned int idx) {

	if (dlist) {
		if (idx < dlist->size) {
			unsigned int i = 0;
			ListElement *ptr = dlist_start (dlist);
			while (ptr != NULL) {
				if (i == idx) return ptr;

				ptr = ptr->next;
				i++;
			}
		}
	}

	return NULL;

}

// traverses the dlist and returns the data of the list element at the specified index
void *dlist_get_at (const DoubleList *dlist, const unsigned int idx) {

	if (dlist) {
		if (idx < dlist->size) {
			ListElement *le = dlist_get_element_at (dlist, idx);
			return le ? le->data : NULL;
		}
	}

	return NULL;

}

/*** sort ***/

// Split a doubly linked dlist (DLL) into 2 DLLs of half sizes 
static ListElement *dllist_merge_sort_split (ListElement *head) { 

	ListElement *fast = head, *slow = head; 

	while (fast->next && fast->next->next) { 
		fast = fast->next->next; 
		slow = slow->next; 
	} 

	ListElement *temp = slow->next; 
	slow->next = NULL; 

	return temp; 

}  

// Function to merge two linked lists 
static ListElement *dllist_merge_sort_merge (
	int (*compare)(const void *one, const void *two), 
	ListElement *first, ListElement *second
) {

	// If first linked dlist is empty 
	if (!first) return second; 
  
	// If second linked dlist is empty 
	if (!second) return first; 

	// Pick the smallest value 
	if (compare (first->data, second->data) <= 0) {
		first->next = dllist_merge_sort_merge (compare, first->next, second); 
		first->next->prev = first; 
		first->prev = NULL; 
		return first; 
	}

	else {
		second->next = dllist_merge_sort_merge (compare, first, second->next); 
		second->next->prev = second; 
		second->prev = NULL; 
		return second; 
	}

} 

// merge sort
static ListElement *dlist_merge_sort (
	ListElement *head, 
	int (*compare)(const void *one, const void *two)
) {

	if (!head || !head->next) return head;

	ListElement *second = dllist_merge_sort_split (head);

	// recursivly sort each half
	head = dlist_merge_sort (head, compare);
	second = dlist_merge_sort (second, compare);

	// merge the two sorted halves 
	return dllist_merge_sort_merge (compare, head, second);

}

// uses merge sort to sort the list using the comparator
// option to pass a custom compare method for searching, if NULL, dlist's compare method will be used
// return 0 on succes 1 on error
int dlist_sort (
	DoubleList *dlist, int (*compare)(const void *one, const void *two)
) {

	int retval = 1;

	if (dlist && dlist->compare) {
		if (dlist->size > 1) {
			int (*comp)(const void *one, const void *two) = compare ? compare : dlist->compare;

			if (comp) {
				(void) pthread_mutex_lock (dlist->mutex);

				dlist->start = dlist_merge_sort (dlist->start, comp);
				retval = 0;

				(void) pthread_mutex_unlock (dlist->mutex);
			}
		}

		else {
			retval = 0;
		}
	}

	return retval;

}

/*** other ***/

// returns a newly allocated array with the list elements inside it
// data will not be copied, only the pointers, so the list will keep the original elements
void **dlist_to_array (const DoubleList *dlist, size_t *count) {

	void **array = NULL;

	if (dlist) {
		array = (void **) calloc (dlist->size, sizeof (void *));
		if (array) {
			unsigned int idx = 0;
			ListElement *ptr = dlist_start (dlist);
			while (ptr != NULL) {
				array[idx] = ptr->data;

				ptr = ptr->next;
				idx++;
			}

			if (count) *count = dlist->size;
		}
	}

	return array;

}

// returns a exact copy of the dlist
// creates the dlist's elements using the same data pointers as in the original dlist
// be carefull which dlist you delete first, as the other should use dlist_clear first before delete
// the new dlist's delete and comparator methods are set from the original
DoubleList *dlist_copy (const DoubleList *dlist) {

	DoubleList *copy = NULL;

	if (dlist) {
		copy = dlist_init (dlist->destroy, dlist->compare);

		for (ListElement *le = dlist_start (dlist); le; le = le->next) {
			dlist_insert_after (
				copy,
				dlist_end (copy),
				le->data
			);
		}
	}

	return copy;

}

// returns a exact clone of the dlist
// the element's data are created using your clone method
// which takes as the original each element's data of the dlist
// and should return the same structure type as the original method that can be safely deleted
// with the dlist's delete method
// the new dlist's delete and comparator methods are set from the original
DoubleList *dlist_clone (
	const DoubleList *dlist, void *(*clone) (const void *original)
) {

	DoubleList *dlist_clone = NULL;

	if (dlist && clone) {
		dlist_clone = dlist_init (dlist->destroy, dlist->compare);

		for (ListElement *le = dlist_start (dlist); le; le = le->next) {
			dlist_insert_after (
				dlist_clone,
				dlist_end (dlist_clone),
				clone (le->data)
			);
		}
	}

	return dlist_clone;

}

// splits the original dlist into two halfs
// if dlist->size is odd, extra element will be left in the first half (dlist)
// both lists can be safely deleted
// the new dlist's delete and comparator methods are set from the original
DoubleList *dlist_split_half (DoubleList *dlist) {

	DoubleList *half = NULL;

	if (dlist) {
		if (dlist->size > 1) {
			half = dlist_init (dlist->destroy, dlist->compare);

			(void) pthread_mutex_lock (dlist->mutex);

			size_t carry = dlist->size % 2;
			size_t half_count = dlist->size / 2;
			half_count += carry;
			size_t count = 0;
			for (ListElement *le = dlist_start (dlist); le; le = le->next) {
				if (count == half_count) {
					dlist->end = le->prev;
					le->prev->next = NULL;
					le->prev = NULL;

					half->start = le;

					half->size = dlist->size - half_count;
					dlist->size = half_count;
					break;
				}

				count++;
			}

			(void) pthread_mutex_unlock (dlist->mutex);
		}
	}

	return half;

}

// creates a new dlist with all the elements that matched the comparator method
// elements are removed from the original list and inserted directly into the new one
// if no matches, dlist will be returned with size of 0
// comparator must return TRUE on match (item will be moved to new dlist)
// this methods is thread safe
// returns a newly allocated dlist with the same detsroy comprator methods
DoubleList *dlist_split_by_condition (
	DoubleList *dlist,
	bool (*compare)(const void *one, const void *two),
	const void *match
) {

	DoubleList *matches = NULL;

	if (dlist && compare) {
		matches = dlist_init (dlist->destroy, dlist->compare);
		if (matches) {
			(void) pthread_mutex_lock (dlist->mutex);

			dlist_internal_move_matches (
				dlist, matches,
				compare,
				match
			);

			(void) pthread_mutex_unlock (dlist->mutex);
		}
	}

	return matches;

}

// merges elements from two into one
// moves list elements from two into the end of one
// two can be safely deleted after this operation
// one should be of size = one->size + two->size
void dlist_merge_two (DoubleList *one, DoubleList *two) {

	if (one && two) {
		if (one->size || two->size) {
			dlist_internal_merge_two (one, two);
		}
	}

}

// creates a new dlist with all the elements from both dlists
// that match the specified confition
// elements from original dlists are moved directly to the new list
// returns a newly allocated dlist with all the matches
DoubleList *dlist_merge_two_by_condition (
	DoubleList *one, DoubleList *two,
	bool (*compare)(const void *one, const void *two),
	const void *match
) {

	DoubleList *merge = NULL;

	if (one && two) {
		merge = dlist_init (one->destroy, one->compare);
		if (merge) {
			dlist_internal_move_matches (
				one, merge,
				compare, match
			);

			dlist_internal_move_matches (
				two, merge,
				compare, match
			);
		}
	}

	return merge;

}

// expects a dlist of dlists and creates a new dlist with all the elements
// elements from original dlists are moved directly to the new list
// the original dlists can be deleted after this operation
// returns a newly allocated dlist with all the elements
DoubleList *dlist_merge_many (DoubleList *many_dlists) {

	DoubleList *merge = NULL;

	if (many_dlists) {
		if (many_dlists->size) {
			DoubleList *first = (DoubleList *) many_dlists->start->data;

			merge = dlist_init (first->destroy, first->compare);
			if (merge) {
				ListElement *le = NULL;
				dlist_for_each (many_dlists, le) {
					dlist_internal_merge_two (
						merge, (DoubleList *) le->data
					);
				}
			}
		}

	}

	return merge;

}
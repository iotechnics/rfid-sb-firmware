/*
*  Hashset implementation
*/

#ifndef HASHSET_H_
#define HASHSET_H_

#include <stdint.h>

// Hashset version
#define HASHSET_VERSION 1.0.0

// Hashset result codes
#define HASHSET_OK 0
#define HASHSET_ITEM_EXISTS 1
#define HASHSET_TABLE_FULL 2

#ifdef __cplusplus
extern "C" {
#endif

// Hashset
typedef struct _hashset {
	uint16_t itemSize;
	uint16_t tableSize;
	uint16_t length;
	uint8_t *table;
	uint8_t *tableIndex;
} hashset;

// Iterator
typedef struct _hashset_iterator {
	hashset *h;
	uint16_t index;
	uint8_t *item;
} hashset_iterator;

// Initialise a hashset
// Parameters:
//   h: Pointer to a hashset
//   tableSize: Maximum number of items to store
//   itemSize: Size of each item, in bytes
void hashset_init(hashset* h, uint16_t tableSize, uint16_t itemSize);

// Add an item to the hashset
// Parameters:
//   h: Pointer to hashset to add to
//   item: The item data to add
// Returns: HASHSET_OK on success, 
//          HASHSET_ITEM_EXISTS if duplicate item found,
//          HASHSET_TABLE_FULL: if the hashset is full
uint8_t hashset_add(hashset* h, uint8_t* item);

// Empties all items from a hashset
// Parameters:
//    h: Pointer to hashset to empty
void hashset_reset(hashset* h);

// Frees all resources used by a hashset
// Parameters:
//   h: Pointer to hashset to destroy
void hashset_destroy(hashset *h);

// Initialise an iterator over the unique items in a hashset
// Parameters:
//   h: Pointer to the hashset to iterate
//   it: Pointer to a hashset iterator
void hashset_initIterator(hashset* h, hashset_iterator* it);

// Iterate to the next unique item
// Parameters:
//   it: Pointer to the hashset iterator
// Returns: 0 if no items left, 1 if successful
uint8_t hashset_iterate(hashset_iterator* it);

#ifdef __cplusplus
}
#endif

#endif // HASHSET_H_

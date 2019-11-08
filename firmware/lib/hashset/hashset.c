#include "hashset.h"

#include <stdint.h>
#include <stdlib.h>

// Checks if a hashset slot is occupied
static int8_t isSlotFree(hashset* h, uint16_t index) {
	if ((h->tableIndex[index / 8] & (1 << (index % 8))) != 0) {
		// Slot occupied
		return 0;
	}
	return 1;
}

// Jenkins one-at-a-time hash function
static inline unsigned hashset_hash(void *key, int len) {
	unsigned char *p = key;
	unsigned h = 0;
	int i;

	for (i = 0; i < len; i++) {
		h += p[i];
		h += (h << 10);
		h ^= (h >> 6);
	}

	h += (h << 3);
	h ^= (h >> 11);
	h += (h << 15);

	return h;
}

// Checks whether an item matches an entry in the hashset
static inline int8_t isEqual(hashset* h, uint16_t index, uint8_t* item) {
	uint8_t *p = h->table + (index * h->itemSize);
	for (uint16_t i = 0; i < h->itemSize; ++i) {
		if (*p++ != *item++) {
			return 0;
		}
	}
	return 1;
}

// Sets a hashset item
static inline void setItem(hashset* h, uint16_t index, uint8_t* item) {
	h->tableIndex[index / 8] |= 1 << (index % 8);
	uint8_t *p = h->table + (index * h->itemSize);
	for (uint16_t i = 0; i < h->itemSize; ++i) {
		*p++ = *item++;
	}
}

// Initialise a hashset
// Parameters:
//   h: Pointer to a hashset
//   tableSize: Maximum number of items to store
//   itemSize: Size of each item, in bytes
void hashset_init(hashset *h, uint16_t tableSize, uint16_t itemSize) {
	h->itemSize = itemSize;
	h->tableSize = tableSize;
	h->length = 0;

	h->table = (uint8_t*)malloc(tableSize * itemSize * sizeof(uint8_t));
	h->tableIndex = tableSize % 8 == 0 
		? (uint8_t*)calloc(tableSize / 8, sizeof(uint8_t))
		: (uint8_t*)calloc((tableSize / 8) + 1, sizeof(uint8_t));
}

// Frees all resources used by a hashset
// Parameters:
//   h: Pointer to hashset to destroy
void hashset_destroy(hashset *h) {
	free(h->table);
	free(h->tableIndex);
}

// Add an item to the hashset
// Parameters:
//   h: Pointer to hashset to add to
//   item: The item data to add
// Returns: HASHSET_OK on success, 
//          HASHSET_ITEM_EXISTS if duplicate item found,
//          HASHSET_TABLE_FULL: if the hashset is full
uint8_t hashset_add(hashset* h, uint8_t* item) {
	if (h->length >= h->tableSize) {
		// Table full, escape quickly
		return 2;
	}
	
	uint16_t i;
	uint16_t index = hashset_hash(item, h->itemSize) % h->tableSize;

	// Slot is occupied, linear probe
	for (i = 0; i < (h->tableSize - 1); ++i) {
		if (isSlotFree(h, index)) {
			// Slot is free, add
			setItem(h, index, item);
			++h->length;
			return 0;
		} else if (isEqual(h, index, item)) {
			// Item already exists
			return 1;
		}
		index = (index + 1) % h->tableSize;
	}

	// Item not added
	return 2;
}

// Initialise an iterator over the unique items in a hashset
// Parameters:
//   h: Pointer to the hashset to iterate
//   it: Pointer to a hashset iterator
void hashset_initIterator(hashset* h, hashset_iterator* it) {
	it->h = h;
	it->index = 0;
	it->item = 0;
}

// Iterate to the next unique item
// Parameters:
//   it: Pointer to the hashset iterator
// Returns: 0 if no items left, 1 if successful
uint8_t hashset_iterate(hashset_iterator* it) {
	it->item = 0;
	if (it->index > it->h->tableSize) {
		return 0;
	}
	while (isSlotFree(it->h, it->index)) {
		++it->index;
		if (it->index > it->h->tableSize) {
			return 0;
		}
	}
	it->item = it->h->table + (it->index * it->h->itemSize);
	++it->index;
	return 1;
}

// Empties all items from a hashset
// Parameters:
//    h: Pointer to hashset to empty
void hashset_reset(hashset* h) {
	uint16_t len = h->tableSize % 8 == 0 
		? h->tableSize / 8
		: (h->tableSize / 8) + 1;
	
	for (uint16_t i = 0; i < len; ++i) {
		h->tableIndex[i] = 0;
	}

	h->length = 0;
}

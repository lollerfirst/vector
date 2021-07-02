## vector implementation - unix only

### features:
* Thread-safe functions;
* System that hints the OS on which block can be swapped based on memory-access statistics;
* Push-back, pop-back, push-front, pop-front functions;
* Pushing/Popping more elements at once;
* Read/Store at specific locations;
* Reading/Storing more elements at once;
* [[not THREAD-SAFE]] Get the address of an element at a specific index (useful for strings and very long datatypes)
* Popf does not immediately recompact the whole vector, but has a backlog up to 48 elements before recompacting
* Pushf checks if there is space left over from Popf before moving the whole vector to make space for 1 element.

### planning:
* extensive testing to find out corner cases, bugs etc... ;
* adding a system that detects small-sized vectors and proceeds to allocate heap memory with sbrk or malloc instead of allocating a huge 4KB chunk of memory;
* speed tests;

### functions table:

|Function|Macro|Description|Return value|
|--------|-----|-----------|------------|
|vector_popb(vec, dest)|yes|Pop an element from the back of the vector into dest. (vector_popn for a more efficient call)|0 success, -1 failure|
|vector_pushb(vec, src)|yes|Push an element to the back of the vector from src. (vector_pushn for a more efficient call)|0 success, -1 failure|
|vector_read(vec, dest, index)|yes|Read an element at the specified vector zero addressed index. (vector_readn for a more efficient call)|void|
|vector_store(vec, src)|yes|Store and element at the specified vector zero addressed index. (vector_storen for a more efficient call)|void|
|vector_readn(vec, dest, index, element_count)|no|Read element_count elements starting from index (ATTENTION: this can still segfault)|void|
|vector_storen(vec, src, index, element_count|no|Store element_count elements starting from index (ATTENTION: this can still segfault)|void|
|vector_init(vec, initial_size, type_size)|no|Initialize the vector with specified size and of the type type_size|0 success, -1 failure|
|vector_pushn(vec, src, element_count)|no|Push element_count elements to the back of the vector from src|0 success, -1 failure|
|vector_pushf(vec, src)|no|Push an element to the front of the vector from src|0 success, -1 failure|
|vector_popn(vec, dest, element_count)|no|Pop element_count elements from the back of the vector into dest|0 success, -1 failure|
|vector_popf(vec, dest)|no|Pop an element from the front of the vector into dest|0 success, -1 failure|
|vector_at(vec, index)|no|Returns the address of an element at the specified zero addressed index|address pointer on success, 0xFFFFFFFFFFFFFFFF on failure|
|vector_init_arr(vec, src, element_count, type_size)|hybrid|Initializes the vector and populates it with the specified array|0 success, -1 failure|
|vector_init_list(vec, type_size, ...)|hybrid|Initializes the vector and populates it with the arguments provided after _typesize_ (must be pointers, must not exceed 64 arguments)|0 success, -1 failure \



# ArtMemoryPool
_Wonderful memory pool is here to boost your software and reduce memory fragmentation_


## Features
* Suitable for **fast** allocating and freeing big amount of objects
* Simple and small single-header library which is easy to integrate
* Written in C++ (with GNU extensions, indirect use of **_Alignof**)
* Minimal dependencies (**libc only**, no use of STL)
* Automatic pointer alignment and new memory block allocation
* Flexible (tunable memory block size grow coefficient...)
* Single-threaded and not synchronized (may be fixed in future release)
* Memory-safe (auto cleanup in class destructor, RAII design pattern)


## Description
It is detailed description on how MemoryPool works under the hood.

### Terminology
Memory chunk is a smallest piece of memory. It's size is always equal to the size of object MemoryPool instance need to allocate.
Memory block is a big piece of memory that is usually allocated with 'malloc' function call from libc.  Blocks are **immediately** divided into chunks while **reservation** and **lazily** while **allocation**.

### The logic behind the MemoryPool
The MemoryPool class has next behaviour: it can **allocate** memory chunk, **deallocate** chunk (store it in a singly-linked list for future reuse), **reserve** memory for some amount of objects.

#### Allocation
Next simplified preudo-code is the best explanation ever on how allocation works:

```
member function **allocate** that returns a pointer (or memory chunk)
{
    if chunk list is not empty (it is possible to reuse previously deallocated chunk)
        pop value from list and return it

    else if it is possible to create a new chunk from previously allocated big block of memory
        create chunk from that memory and return chunk

    otherwise
        allocate a new big block of memory and store it in block list (to be freed later in destructor)
        create a new chunk from memory block and return that chunk
}
```

Every time new block is allocated, size of next allocation is multiplied by allocation size grow coefficient.
Check the code, it is almost as simple as the pseudo-code above.

#### Deallocation
Deallocation is simple: method takes a pointer and pushes it to the singly-linked list called chunk list for future reuse.

#### Reservation
Reservation is allocation of memory block that will be divided into chunks immediately.
Next pseudo-code is good explanation:

```
member function **reserve** that takes integral parameter **n** and returns nothing (void)
{
    allocate memory block big enough to create **n** memory chunks

    repeat **n** times
        create a new chunk from memory block allocated before and push it to chunk list
}
```

When new chunk is created, pointer to allocated memory block is adjusted (because some memory is owned by chunk)

#### Construction / Destruction
MemoryPool class constructor body is almost empty. It initializes some fields and does assertions.
Destructor calls 'free' function from libc on all allocated by 'malloc' function memory pointers (which are stored in block list).

### Different information for advanced users
MemoryPool is a class which is non-copyable (if you will try to copy MemoryPool instance then it must not even compile, deep copying is not supported).
The minimal possible size of object that MemoryPool instance will allocate is **sizeof(void*)**. This is because of use of a singly-linked list where every list node points to a next list node.
All methods (except dtors) have qualifier **noexcept** because they must never throw.
Use of **alignof** from cstddef or indirect use of **_Alignof** which is a GNU extension. It is used to calculate a proper pointer alignment while compiling (alignment is different for different classes).


## Notes
* Everything is inside the **art** namespace (it prevents name collisions)
* **#define ART_MEMORY_POOL_IMPL** to add implementation while including ArtMemoryPool.hpp (By default, only interfaces and prototypes are included)
* **#define NDEBUG** to disable all assertions (make it in release version only)


## Usage
_The best way to use MemoryPool is to overload **new** and **delete** operators for your objects and use MemoryPool under the hood._

### Minimal example
```cpp
#define ART_MEMORY_POOL_IMPL  // Add implementation too, not only interface
#include "ArtMemoryPool.hpp"


struct MyObject { double d; int i; char c; };

int main()
{
    art::MemoryPool<MyObject> memoryPool;  // Default constructor is OK

    MyObject* object = memoryPool.allocate();  // Allocation of one object

    memoryPool.deallocate(object);  // Deallocation of one object
}

```

### Real-world example
```cpp
#define ART_MEMORY_POOL_IMPL
#include "ArtMemoryPool.hpp"


class MyObject
{
    public:
            static void* operator new(size_t count)  // Overload 'new'
            {
                assert(count == sizeof(MyObject));  // Should never fail
                return static_cast<void*>(memoryPool.allocate());
            }

            static void operator delete(void* pointer)  // Overload 'delete'
            {
                memoryPool.deallocate(static_cast<MyObject*>(pointer));
            }


    private:
            static art::MemoryPool<MyObject> memoryPool;

            // Encapsulate some random data
            char msg[123];
            double value;
};

art::MemoryPool<MyObject> MyObject::memoryPool;  // Initialize static field

int main()  // How to allocate and deallocate one object and even array
{
    MyObject* object = new MyObject;  // Allocation of one object
    delete object;  // Deallocation of one object

    // Creating array of pointers to MyObject
    constexpr size_t size = 1000;
    MyObject* array[size];

    // Allocating array of MyObject using 'new'
    for (size_t i = 0; i != size; ++i)
        array[i] = new MyObject;

    // Deallocating array of MyObject using 'delete'
    for (size_t i = 0; i != size; ++i)
        delete array[i];
}

```


## Bug reporting
If a **bug**, **assertion failed**, **segmentation fault** or weird behaviour occured, create an issue and you definitely will get a help from me.


## License
**MIT** License. See MemoryPool.hpp heading.


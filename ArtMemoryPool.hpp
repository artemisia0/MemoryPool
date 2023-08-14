/*
MIT License

Copyright (c) 2023 artemisia0

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#ifndef ART_MEMORY_POOL_HPP
#define ART_MEMORY_POOL_HPP

#include <cstddef>
#include <cstdlib>
#include <cassert>


namespace art
{
    namespace internal
    {
        // The only function in this lonely world full of classes...
        inline static char* align(char*, size_t) noexcept;

        class NonCopyable  // Copying is prohibited, moving is allowed.
        {
            public:
                    NonCopyable()                               = default;
                    NonCopyable(const NonCopyable&)             = delete;
                    NonCopyable& operator=(const NonCopyable&)  = delete;
        };

        // It is really unsafe to instantiate this class...
        class PointerList  // Singly-linked list for pointers (char*)
        {
            private:
                    struct ListNode { ListNode* next; };


            public:
                    inline                  PointerList()   noexcept;
                    inline                  ~PointerList() = default;

                    inline void             push(char*)     noexcept;
                    inline char*            pop()           noexcept;

                    inline                  operator bool() noexcept;


            private:
                    ListNode*               mList;
        };
    }

    template <typename T> class MemoryPool : private internal::NonCopyable
    {
        public:
                inline                      MemoryPool(float allocSizeGrow=2.f)
                                                            noexcept;
                inline                      ~MemoryPool();

                inline T*                   allocate()      noexcept;
                inline void                 deallocate(T*)  noexcept;
                inline void                 reserve(size_t) noexcept;


        private:
                inline void                 allocBlock()    noexcept;
                inline T*                   createChunk()   noexcept;
                inline size_t               bytesToAlloc()  noexcept;
                inline char*                callMalloc()    noexcept;
                inline char*                alignedMalloc() noexcept;
 

        private:
                internal::PointerList       mBlockList;
                internal::PointerList       mChunkList;
                size_t                      mNextAllocSize;
                float                       mAllocSizeGrow;
                char*                       mCurrentBlock;
                char*                       mCurrentBlockEnd;
    };
}

#ifdef ART_MEMORY_POOL_IMPL

namespace art
{
    namespace internal
    {
        char* align(char* ptr, size_t alignment) noexcept
        {
            ptr += alignment
                - (reinterpret_cast<size_t>(ptr) & ((alignment<<1UL)-1UL));

            // May fail if alignment is not a power of two.
            assert((size_t)ptr % alignment == 0);

            return ptr;
        }

        PointerList::PointerList() noexcept
        : mList(nullptr)
        {
        }

        void PointerList::push(char* ptr) noexcept
        {
            // ptr must point to memory chunk of size
            // at least sizeof(void*) bytes.
            // Otherwise, behaviour is undefined.
            ListNode* node = reinterpret_cast<ListNode*>(ptr);
            node->next = mList;
            mList = node;
        }

        char* PointerList::pop() noexcept
        {
            assert(mList);  // Failed to pop if list is empty

            char* returnValue = reinterpret_cast<char*>(mList);
            mList = mList->next;

            return returnValue;
        }

        PointerList::operator bool() noexcept  // Check mList for emptiness
        {
            return static_cast<bool>(mList);
        }
    }

    template <typename T>
    MemoryPool<T>::MemoryPool(float allocSizeGrow) noexcept
    : mBlockList()
    , mChunkList()
    , mNextAllocSize(1024UL)  // Measure unit is sizeof(T), not bytes
    , mAllocSizeGrow(allocSizeGrow)
    , mCurrentBlock(nullptr)
    , mCurrentBlockEnd(nullptr)
    {
        // Size of T instance must be >= sizeof(void*).
        // This is not a disadvantage but implementation detail.
        assert(sizeof(T) >= sizeof(void*));

        // It is really weird for allocSizeGrow to be > 2.0 or < 1.0
        assert(1.f <= mAllocSizeGrow and mAllocSizeGrow <= 2.f);
    }

    template <typename T>
    MemoryPool<T>::~MemoryPool()
    {
        while (mBlockList)
            ::free(mBlockList.pop());
    }

    template <typename T>
    T* MemoryPool<T>::allocate() noexcept
    {
        if (mChunkList)
            return reinterpret_cast<T*>(mChunkList.pop());

        // Lazy chunk creation if there is memory enough
        if (mCurrentBlock + sizeof(T) <= mCurrentBlockEnd)
            return createChunk();

        allocBlock();
        return createChunk();
    }

    template <typename T>
    void MemoryPool<T>::deallocate(T* ptr) noexcept
    {
        assert(ptr);

        mChunkList.push(reinterpret_cast<char*>(ptr));
    }

    template <typename T>
    void MemoryPool<T>::reserve(size_t n) noexcept
    {
        // Immediate creation of n memory chunks of size sizeof(T).

        assert(n);  // Reservation of zero chunks is probable bug.

        mNextAllocSize = n;
        const size_t allocSize = mNextAllocSize;
        char* memory = alignedMalloc();
        const char* memoryEnd = memory + allocSize*sizeof(T);
        if (not memory)  // Failed to allocate memory enough. Silent abort.
            return;

        while (memory != memoryEnd)
        {
            assert(memory < memoryEnd);  // Must never fail
            char* chunk = memory;
            assert((size_t)chunk % alignof(T) == 0);  // Must never fail too
            memory += sizeof(T);
            mChunkList.push(chunk);
        }
    }

    template <typename T>
    void MemoryPool<T>::allocBlock() noexcept
    {
        const size_t allocSize = mNextAllocSize;
        mCurrentBlock = alignedMalloc();
        mCurrentBlockEnd = mCurrentBlock + allocSize*sizeof(T);
    }

    template <typename T>
    T* MemoryPool<T>::createChunk() noexcept
    {
        assert(mCurrentBlock + sizeof(T) <= mCurrentBlockEnd);

        T* chunk = reinterpret_cast<T*>(mCurrentBlock);
        mCurrentBlock += sizeof(T);
        
        assert((size_t)chunk % alignof(T) == 0);
        return chunk;
    }

    template <typename T>
    size_t MemoryPool<T>::bytesToAlloc() noexcept
    {
        return mNextAllocSize*sizeof(T) + alignof(T);
    }

    template <typename T>
    char* MemoryPool<T>::callMalloc() noexcept
    {
        char* memory = static_cast<char*>(malloc(bytesToAlloc()));
        if (memory)  // May be null if there's no free memory enough
            mBlockList.push(memory);

        mNextAllocSize = mNextAllocSize * mAllocSizeGrow;

        return memory;  // Nullptr may be returned
    }

    template <typename T>
    char* MemoryPool<T>::alignedMalloc() noexcept
    {
        return internal::align(callMalloc() + sizeof(void*), alignof(T));
    }
}

#endif  /* ART_MEMORY_POOL_IMPL */

#endif  /* ART_MEMORY_POOL_HPP */


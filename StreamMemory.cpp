/*
Copyright (C) 2009-2010 Electronic Arts, Inc.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1.  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
2.  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
3.  Neither the name of Electronic Arts, Inc. ("EA") nor the names of
    its contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY ELECTRONIC ARTS AND ITS CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ELECTRONIC ARTS OR ITS CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/////////////////////////////////////////////////////////////////////////////
// EAStreamMemory.cpp
//
// Copyright (c) 2007, Electronic Arts Inc. All rights reserved.
// Written by Paul Pedriana
//
// Implements a IO stream that reads and writes to a block of memory.
/////////////////////////////////////////////////////////////////////////////


#include "EAIO/internal/Config.h"
#include "EAIO/StreamMemory.h"
#include "EAIO/PathString.h"
#include "eastl/extra/debug.h"
#include <limits.h>


///////////////////////////////////////////////////////////////////////////////
// SharedPointer
///////////////////////////////////////////////////////////////////////////////

eaio::SharedPointer::SharedPointer(void* pData, bool bFreeData, Allocator* pAllocator)
    : mpAllocator(pAllocator ? pAllocator : eaio::getAllocator())
    , mpData((uint8_t*)pData)
    , mnRefCount(0)
    , mbFreeData(bFreeData)
{
    EASTL_ASSERT(mpAllocator);
}


eaio::SharedPointer::SharedPointer(size_type nSize, const char* pName)
    : mpAllocator(eaio::getAllocator())
    , mpData((uint8_t*)mpAllocator->allocate((size_t)nSize, /*pName ? pName : EAIO_ALLOC_PREFIX "EAStreamMemory/data",*/ 0))
    , mnRefCount(0)
    , mbFreeData(true)
{
    EASTL_ASSERT(mpAllocator);
}


eaio::SharedPointer::SharedPointer(size_type nSize, Allocator* pAllocator, const char* pName)
    : mpAllocator(pAllocator ? pAllocator : eaio::getAllocator())
    , mpData((uint8_t*)mpAllocator->allocate((size_t)nSize, /*pName ? pName : EAIO_ALLOC_PREFIX "EAStreamMemory/data",*/ 0))
    , mnRefCount(0)
    , mbFreeData(true)
{
    EASTL_ASSERT(mpAllocator);
}


int eaio::SharedPointer::release()
{ 
    if(mnRefCount > 1)
        return --mnRefCount;
    if(mbFreeData)
        mpAllocator->deallocate(mpData, 0);
    delete this;
    return 0;
}




///////////////////////////////////////////////////////////////////////////////
// MemoryStream
///////////////////////////////////////////////////////////////////////////////

eaio::MemoryStream::MemoryStream(SharedPointer* pSharedPointer, size_type nSize, const char* pName)
  : mpSharedPointer(NULL),
    mpAllocator(pSharedPointer ? pSharedPointer->getAllocator() : NULL),
    mpName(pName),
    mnRefCount(0),
    mnSize(0),
    mnCapacity(0),
    mnPosition(0),
  //mbClearNewMemory(false),
    mbResizeEnabled(false),
    mfResizeFactor(1.5f),
    mnResizeIncrement(0)
{
    if(pSharedPointer && nSize)
        setData(pSharedPointer, nSize);
}


eaio::MemoryStream::MemoryStream(void* pData, size_type nSize, bool bUsePointer, bool bFreePointer, Allocator *pAllocator, const char* pName)
  : mpSharedPointer(NULL),
    mpAllocator(pAllocator),
    mpName(pName),
    mnRefCount(0),
    mnSize(0),
    mnCapacity(0),
    mnPosition(0),
  //mbClearNewMemory(false),
    mbResizeEnabled(false),
    mfResizeFactor(1.5f),
    mnResizeIncrement(0)
{
    if(pData || nSize)
        setData(pData, nSize, bUsePointer, bFreePointer, pAllocator);
}


eaio::MemoryStream::MemoryStream(const MemoryStream& memoryStream)
  : mpSharedPointer(memoryStream.mpSharedPointer),
    mpAllocator(memoryStream.mpAllocator),
    mpName(memoryStream.mpName),
    mnRefCount(0),
    mnSize(memoryStream.mnSize),
    mnCapacity(memoryStream.mnSize),
    mnPosition(memoryStream.mnPosition),
  //mbClearNewMemory(false),
    mbResizeEnabled(memoryStream.mbResizeEnabled),
    mfResizeFactor(memoryStream.mfResizeFactor),
    mnResizeIncrement(memoryStream.mnResizeIncrement)
{
    if(mpSharedPointer)
        mpSharedPointer->addRef();
}



float eaio::MemoryStream::getOption(Options option) const
{
    switch (option)
    {
        case kOptionResizeEnabled:
            return mbResizeEnabled ? 1.f : 0.f;

        case kOptionResizeFactor:
            return mfResizeFactor;

        case kOptionResizeIncrement:
            return (float)mnResizeIncrement;

        case kOptionResizeMaximum:
            return (float)mnResizeMax;

        //case kOptionClearNewMemory:
        //    return mbClearNewMemory ? 1.f : 0.f;

        case kOptionNone:
        default:
            break;
    }

    return 0.f;
}


void eaio::MemoryStream::setOption(Options option, float fValue)
{
    switch (option)
    {
        case kOptionResizeEnabled:
            mbResizeEnabled = (fValue != 0);
            break;

        case kOptionResizeFactor:
            if(fValue < 1.f)
                fValue = 1.f;
            mfResizeFactor = fValue;
            break;

        case kOptionResizeIncrement:
            if(fValue < 0)
                fValue = 0;
            mnResizeIncrement = (int)fValue;
            break;

        case kOptionResizeMaximum:
            mnResizeMax = (int)fValue;
            break;

        // Currently disabled until we deem it is useful. Then we will want to make sure it completely works as intended.
        //case kOptionClearNewMemory:
        //    mbClearNewMemory = (fValue != 0);
        //    if(mbClearNewMemory)
        //        memset((char*)mpSharedPointer->GetPointer() + mnSize, 0, mnCapacity - mnSize);
        //    break;

        case kOptionNone:
        default:
            break;
    }
}


bool eaio::MemoryStream::setData(void* pData, size_type nSize, bool bUsePointer, bool bFreePointer, Allocator *pAllocator)
{
    bool bReturnValue(false);

    if(pData || nSize)
    {
        if(pAllocator == NULL)
            pAllocator = mpAllocator ? mpAllocator : eaio::getAllocator();

        EASTL_ASSERT(pAllocator);
        void* const pDataCopy = (bUsePointer ? pData : pAllocator->allocate((size_t)nSize, /*mpName ? mpName : EAIO_ALLOC_PREFIX "EAStreamMemory/data",*/ 0));

        if(pDataCopy)
        {
            if(mpSharedPointer)
                mpSharedPointer->release();

            mpSharedPointer = new (pAllocator, mpName ? mpName : EAIO_ALLOC_PREFIX "EAStreamMemory/ptr") SharedPointer(pDataCopy, bFreePointer, pAllocator);

            if(mpSharedPointer)
            {
                mpSharedPointer->addRef();

                if(pData && nSize && !bUsePointer)
                    memcpy(pDataCopy, pData, (size_t)nSize);

                bReturnValue = true;
            }
            else if(!bUsePointer) // Avoid leaking memory in the failure case
                pAllocator->deallocate(pDataCopy, 0);
        }
    }
    else
    {
       if(mpSharedPointer)
          mpSharedPointer->release();

       mpSharedPointer = NULL;
       bReturnValue = true;
    }


    if(mpSharedPointer)
        mnCapacity = nSize;
    else
        mnCapacity = 0;

    mnSize = mnCapacity;

    mnPosition = 0;

    return bReturnValue;
}


bool eaio::MemoryStream::setData(SharedPointer* pSharedPointer, size_type nSize)
{
    if(mpSharedPointer != pSharedPointer)
    {
        if(pSharedPointer)
            pSharedPointer->addRef();
        if(mpSharedPointer)
            mpSharedPointer->release();
        mpSharedPointer = pSharedPointer;
    }

    if(mpSharedPointer)
        mnCapacity = mnSize = nSize;
    else
        mnCapacity = mnSize = 0;

    mnPosition = 0;

    return (mpSharedPointer != NULL);
}


bool eaio::MemoryStream::setSize(size_type size)
{
    bool bReturnValue(true);

    if(size != mnSize) // If there is a change...
    {
        if(mbResizeEnabled)
        {
            if(size < mnSize) // If the size is being reduced...
            {
                mnSize = size;

                // In this case we don't reallocate but instead  
                // just set our reported memory size lower.
                if(mnPosition > size)
                    mnPosition = size;
            }
            else // Else the user is asking to increase the size of the memory.
            {
                bReturnValue = reallocBuffer(size);

                if(bReturnValue)
                    mnSize = size;
            }
        }
        else
            bReturnValue = false;
    }

    return bReturnValue;
}


bool eaio::MemoryStream::reallocBuffer(size_type nSize)
{
    SharedPointer* pNewSharedPointer;
    Allocator*     pAllocator;

    if(mpSharedPointer && mpSharedPointer->getAllocator())
        pAllocator = mpSharedPointer->getAllocator();
    else
        pAllocator = mpAllocator ? mpAllocator : eaio::getAllocator();

    if(nSize)
    {
        EASTL_ASSERT(pAllocator);
        pNewSharedPointer = new(pAllocator, mpName ? mpName : EAIO_ALLOC_PREFIX "EAStreamMemory/ptr") SharedPointer(nSize, pAllocator, mpName);

        if(pNewSharedPointer)
        {
            pNewSharedPointer->addRef();
            //if(mbClearNewMemory)
            //    memset(pNewSharedPointer->GetPointer(), 0, nSize);
        }
        else
        {
            eastl::fatalError("MemoryStream::Realloc: Realloc memory allocation failed.");
            return false;
        }
    }
    else
    {
        // In this case we are resizing to zero and don't need to allocate any new memory.
        pNewSharedPointer = NULL;
    }

    if(mpSharedPointer) // If we have previous memory...
    {
        if(pNewSharedPointer)
        {
            const size_type nCopySize(nSize < mnCapacity ? nSize : mnCapacity);
            memcpy(pNewSharedPointer->getPointer(), mpSharedPointer->getPointer(), (size_t)nCopySize);
        }

        mpSharedPointer->release();
    }

    mpSharedPointer = pNewSharedPointer;
    mnCapacity      = nSize;

    return true;
}


eaio::off_type eaio::MemoryStream::getPosition(PositionType positionType) const
{
    // We have a small problem here: off_type is signed while mnPosition is 
    // unsigned but of equal size to off_type. Thus, if mnPosition is of a 
    // high enough value then the return value can be negative. 

    switch(positionType)
    {
        case kPositionTypeBegin:
            return (off_type)mnPosition;

        case kPositionTypeEnd:
            return (off_type)(mnPosition - mnSize);

        case kPositionTypeCurrent:
        default:
            break;
    }

    return 0; // For kPositionTypeCurrent the result is always zero for a 'get' operation.
}


bool eaio::MemoryStream::setPosition(off_type nPosition, PositionType positionType)
{
    const off_type nPositionSaved = (off_type)mnPosition;

    switch(positionType)
    {
        case kPositionTypeBegin:
            EASTL_ASSERT(nPosition >= 0);
            mnPosition = (size_type)nPosition; // We deal with negative positions below.
            break;

        case kPositionTypeCurrent:
            mnPosition = mnPosition + (size_type)nPosition;  // We have a signed/unsigned match, but the math should work anyway.
            break;

        case kPositionTypeEnd:
            mnPosition = mnSize + nPosition; // We deal with invalid resulting positions below.
            break;
    }

    // Deal with out-of-bounds situations that result from the above.
    if(mnPosition > mnSize)
    {
        EASTL_ASSERT(mnPosition < (size_type((off_type)-1) / 2));

        if(mbResizeEnabled)
        {
            // Note that we don't do a 'smart' resize here whereby we possibly resize 
            // beyond the requested position like we do in the Write function. 
            // The reason for this is that we assume the user doesn't call SetPosition 
            // with out-of-bounds positions frequently. We can reevaluate this policy.
            if((mnPosition + 1) > mnCapacity) // If we need additional capacity...
            {
                if(!reallocBuffer(mnPosition + 1)) // If we fail to get the additional capacity...
                {
                    mnPosition = (size_type)nPositionSaved;
                    return false;
                }
            }
        }
        else
        {
            // Disabled because this isn't a function requirements failure: 
            //    EA_FAIL_MESSAGE("MemoryStream::SetPosition: Requested position is out of bounds and resize is disabled.");
            mnPosition = mnSize;
            return false;
        }
    }

    return true;
}


bool eaio::MemoryStream::setCapacity(size_type capacity)
{
    if(capacity > mnCapacity)
        return reallocBuffer(capacity);

    return false;
}


eaio::size_type eaio::MemoryStream::read(void* pData, size_type nSize)
{
    if(nSize > 0)
    {
        EASTL_ASSERT(mnPosition <= mnSize);
        const size_type nBytesAvailable(mnSize - mnPosition);

        if(nBytesAvailable > 0)
        {
            if(nSize > nBytesAvailable)
                nSize = nBytesAvailable;

            memcpy(pData, (const uint8_t*)mpSharedPointer->getPointer() + mnPosition, (size_t)nSize);
            mnPosition += nSize;

            return nSize;
        }
    }

    return 0;
}


bool eaio::MemoryStream::write(const void* pData, size_type nSize)
{
    if(nSize > 0)
    {
        EASTL_ASSERT(mbResizeEnabled || (mnPosition <= mnSize));
        size_type nRequiredSize(mnPosition + nSize);
        size_type nBytesToWrite(nSize);

        if(nRequiredSize > mnCapacity) // If we need to increase our capacity...
        {
            if(mbResizeEnabled)
            {
                size_type nNewCapacity = (size_type)((mnCapacity * mfResizeFactor) + mnResizeIncrement);

                if(nNewCapacity < nRequiredSize)
                    nNewCapacity = nRequiredSize;

                if(reallocBuffer(nNewCapacity))
                    mnSize = nRequiredSize;
                else
                    return false;
            }
            else // Else the write will be only partial.
                nBytesToWrite = (mnSize - mnPosition);
        }
        else if(mnSize < nRequiredSize)
           mnSize = nRequiredSize;

        // We assume that 99% of the timethat 'nBytesToWrite' is > 0, 
        // so we don't bother to check to see if it is zero here.
        uint8_t* const pSharedPtrData = (uint8_t*)mpSharedPointer->getPointer();
        EASTL_ASSERT(pSharedPtrData && pData);
        memcpy(pSharedPtrData + mnPosition, pData, (size_t)nBytesToWrite);
        mnPosition += nBytesToWrite;

        return (nBytesToWrite == nSize); // This will be false if the user tried to write beyond the end and (mbResizeEnabled == false).
    }

    return true;
}









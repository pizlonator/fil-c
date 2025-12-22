// MIT License
//
// Copyright (c) 2016 Microsoft Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// Source taken from https://github.com/microsoft/MixedRealityCompanionKit

#pragma once

#include <wrl.h>
#include <wrl\async.h>
#include <Windows.System.Threading.h>
#include <functional>

template <typename TDelegate, typename TOperation, typename TLambda>
HRESULT StartAsyncThen(_In_ TOperation* pOperation, _In_ TLambda&& tFunc)
{
    if (nullptr == pOperation)
    {
        return E_INVALIDARG;
    }

    auto spCallback = Microsoft::WRL::Callback<TDelegate>(
        [tFunc](_In_ TOperation* pOperation, _In_ AsyncStatus status) -> HRESULT
        {
            HRESULT hr = S_OK;

            // wrap the operation
            if (status != AsyncStatus::Completed)
            {
                Microsoft::WRL::ComPtr<TOperation> spOperation(pOperation);
                Microsoft::WRL::ComPtr<IAsyncInfo> spAsyncInfo;
                hr = spOperation.As(&spAsyncInfo);
                if (SUCCEEDED(hr))
                {
                    spAsyncInfo->get_ErrorCode(&hr);
                }
            }

            return tFunc(hr, pOperation, status);
        });

    // start
    return (nullptr != spCallback) ? pOperation->put_Completed(spCallback.Get()) : E_OUTOFMEMORY;
}
template <typename TLambda>
HRESULT StartAsyncThen(_In_ ABI::Windows::Foundation::IAsyncAction* pOperation, _In_ TLambda&& tFunc)
{
    return StartAsyncThen<ABI::Windows::Foundation::IAsyncActionCompletedHandler, ABI::Windows::Foundation::IAsyncAction>(pOperation, static_cast<TLambda&&>(tFunc));
}
template <typename TProgress, typename TLambda>
HRESULT StartAsyncThen(_In_ ABI::Windows::Foundation::IAsyncActionWithProgress<TProgress>* pOperation, _In_ TLambda&& tFunc)
{
    return StartAsyncThen<ABI::Windows::Foundation::IAsyncActionWithProgressCompletedHandler<TProgress>, Windows::Foundation::IAsyncActionWithProgress<TProgress>>(pOperation, static_cast<TLambda&&>(tFunc));
}
template <typename TResult, typename TLambda>
HRESULT StartAsyncThen(_In_ ABI::Windows::Foundation::IAsyncOperation<TResult>* pOperation, _In_ TLambda&& tFunc)
{
    return StartAsyncThen<ABI::Windows::Foundation::IAsyncOperationCompletedHandler<TResult>, ABI::Windows::Foundation::IAsyncOperation<TResult>>(pOperation, static_cast<TLambda&&>(tFunc));
}
template <typename TResult, typename TProgress, typename TLambda>
HRESULT StartAsyncThen(_In_ ABI::Windows::Foundation::IAsyncOperationWithProgress<TResult, TProgress>* pOperation, _In_ TLambda&& tFunc)
{
    return StartAsyncThen<ABI::Windows::Foundation::IAsyncOperationWithProgressCompletedHandler<TResult, TProgress>, ABI::Windows::Foundation::IAsyncOperationWithProgress<TResult, TProgress>>(pOperation, static_cast<TLambda&&>(tFunc));
}


// eg. TOperation   = IAsyncOperationWithProgress<UINT32, UINT32>
// eg. THandler     = IAsyncOperationWithProgressCompletedHandler<UINT, UINT>
template<typename TOperation, typename THandler>
class AsyncEventDelegate
    : public Microsoft::WRL::RuntimeClass
    < Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::Delegate>
    , THandler
    , Microsoft::WRL::FtmBase >
{
public:
    AsyncEventDelegate()
        : _completedEvent(CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS))
    {
        ComPtr<AsyncEventDelegate> spThis(this);
        auto lambda = ([this, spThis](_In_ HRESULT hr, _In_ TOperation* pOperation)
        {
            SetEvent(_completedEvent.Get());
        });
        _func = std::move(lambda);
    }

    STDMETHOD(Invoke)(
        _In_ TOperation* pOperation,
        _In_ AsyncStatus status)
    {
        HRESULT hr = S_OK;

        // if we completed successfully, then there is no need for getting hresult
        if (status != AsyncStatus::Completed)
        {
            Microsoft::WRL::ComPtr<TOperation> spOperation(pOperation);
            Microsoft::WRL::ComPtr<IAsyncInfo> spAsyncInfo;
            if (SUCCEEDED(spOperation.As(&spAsyncInfo)))
            {
                spAsyncInfo->get_ErrorCode(&hr);
            }
        }

        _func(hr, pOperation);

        return S_OK;
    }

    STDMETHOD(SyncWait)(_In_ TOperation* pOperation, _In_ DWORD dwMilliseconds)
    {
        HRESULT hr = pOperation->put_Completed(this);
        if (FAILED(hr))
        {
            return hr;
        }

        DWORD dwWait = WaitForSingleObjectEx(_completedEvent.Get(), dwMilliseconds, TRUE);
        if (WAIT_IO_COMPLETION == dwWait || WAIT_OBJECT_0 == dwWait)
            return S_OK;

        return HRESULT_FROM_WIN32(GetLastError());
    }

private:
    std::function<void(HRESULT, TOperation*)> _func;
    Microsoft::WRL::Wrappers::Event _completedEvent;
};
template <typename TOperation, typename THandler>
HRESULT SyncWait(_In_ TOperation* pOperation, _In_ DWORD dwMilliseconds)
{
    auto spCallback = Microsoft::WRL::Make<AsyncEventDelegate<TOperation, THandler>>();

    return spCallback->SyncWait(pOperation, dwMilliseconds);
}
template <typename TResult>
HRESULT SyncWait(_In_ ABI::Windows::Foundation::IAsyncAction* pOperation, _In_ DWORD dwMilliseconds = INFINITE)
{
    return SyncWait<ABI::Windows::Foundation::IAsyncAction, ABI::Windows::Foundation::IAsyncActionCompletedHandler>(pOperation, dwMilliseconds);
}
template <typename TResult>
HRESULT SyncWait(_In_ ABI::Windows::Foundation::IAsyncOperation<TResult>* pOperation, _In_ DWORD dwMilliseconds = INFINITE)
{
    return SyncWait<ABI::Windows::Foundation::IAsyncOperation<TResult>, ABI::Windows::Foundation::IAsyncOperationCompletedHandler<TResult>>(pOperation, dwMilliseconds);
}
template <typename TResult, typename TProgress>
HRESULT SyncWait(_In_ ABI::Windows::Foundation::IAsyncOperationWithProgress<TResult, TProgress>* pOperation, _In_ DWORD dwMilliseconds = INFINITE)
{
    return SyncWait<ABI::Windows::Foundation::IAsyncOperationWithProgress<TResult, TProgress>, ABI::Windows::Foundation::IAsyncOperationWithProgressCompletedHandler<TResult, TProgress>>(pOperation, dwMilliseconds);
}

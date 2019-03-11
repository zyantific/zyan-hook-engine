# Hook Barrier API

## Introduction

The Hook Barrier API was developed to prevent unwanted hook recursion. Unwanted hook recursion is a serious issue in many hooking libraries.

#### Example

Lets assume we hooked the Windows `NtResumeThread` API which can be used as a generic way to track process creation. In our callback we want to start a second process (e.g. a debugger) and attach it to the new process:

```c
NTSTATUS NtResumeThread_Callback(HANDLE ThreadHandle, PULONG SuspendCount)
{
    if (BelongsToNewlyCreatedProcess(ThreadHandle))
    {
        if (CreateProcessW("debugger.exe", "-attach {pid}", ...))
        {
            // ...
        }
    }

    return NtResumeThread_Trampoline(ThreadHandle, SuspendCount);
}
```

This small code does already trigger an endless recursion loop.

The `CreateProcessW` function internally invokes some other APIs like `CreateProcessInternalW` and `NtCreateProcess` which then finally calls `NtResumeThread` to resume the main thread of the new process.

This last call to `NtResumeThread` will of course be redirected to our callback function which then starts the recursion.

#### Prevention

To prevent unwanted hook recursion, every thread needs to track the current recursion level for each hooked function. Temporarily removing the hook is no solution in a preemptive multitasking system, as this could cause unwanted side-effects like e.g. the hook not triggering for an other thread.

## Usage

 As the `trampoline` pointer might get modified from another thread (e.g. during hook removal), it is neccessary to obtain a constant barrier handle before invoking a barrier API function inside a hook callback. The returned handle should be saved and then used for all subsequent calls to the barrier API inside the current hook callback.

```c
ZYREX_EXPORT ZyrexBarrierHandle ZyrexBarrierGetHandle(const void* trampoline);
```

After a barrier handle got obtained, it can be used to enter the barrier. The function returns `ZYAN_STATUS_TRUE` if the current recursion level is 0 or `ZYAN_STATUS_FALSE`, if not. The callback should invoke the trampoline function with unchanged parameters and immediately return afterwards in the latter case.

```c
ZYREX_EXPORT ZyanStatus ZyrexBarrierTryEnter(ZyrexBarrierHandle handle);
```

If the barrier was successfully entered before, it needs to be leaved before returning from the callback.

```c
ZYREX_EXPORT ZyanStatus ZyrexBarrierLeave(ZyrexBarrierHandle handle);
```

#### Example

Here is the example from the introduction rewritten to use the Hook Barrier API:

```c
NTSTATUS NtResumeThread_Callback(HANDLE ThreadHandle, PULONG SuspendCount)
{
    // Obtain the barrier handle identified by the given trampoline
    const ZyrexBarrierHandle barrier_handle = ZyrexBarrierGetHandle(&NtResumeThread_Tampoline);

    // Try to enter the Hook Barrier
    if (ZyrexBarrierTryEnter(barrier_handle) != ZYAN_STATUS_TRUE)
    {
        // Failed to pass the barrier. Invoke the trampoline function and immediately leave the 
        // callback
        return NtResumeThread_Trampoline(ThreadHandle, SuspendCount);    
    }

    // We successfully passed the barrier ..
    if (BelongsToNewlyCreatedProcess(ThreadHandle))
    {
        if (CreateProcessW("debugger.exe", "-attach {pid}", ...))
        {
            // ...
        }
    }

    // Leave the barrier
    ZyrexBarrierLeave(barrier_handle);

    return NtResumeThread_Trampoline(ThreadHandle, SuspendCount);
}
```

## Internals

The Hook Barrier API uses Thread-Local-Storage (TLS) to store a vector which contains the current recursion level for every hook that called `ZyrexBarrierTryEnter` at least once.

TLS functionality is abstracted by Zycore and thus available on Windows and all POSIX compliant platforms.
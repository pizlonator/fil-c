#include <setjmp.h>
#include <unwind.h>
#include <stdfil.h>
#include <signal.h>

struct C {
    C()
    {
        zprintf("C::C;");
    }

    ~C()
    {
        zprintf("C::~C;");
    }
};

struct D {
    D()
    {
        zprintf("D::D;");
    }

    ~D()
    {
        zprintf("D::~D;");
    }
};

static jmp_buf jmpBuf;

static _Unwind_Reason_Code stopCallback(
    int version, _Unwind_Action actions, _Unwind_Exception_Class exceptionClass,
    _Unwind_Exception* exceptionObject, _Unwind_Context* Context, void* stopArg)
{
    ZASSERT(stopArg == (void*)666);
    if (actions & _UA_END_OF_STACK)
        longjmp(jmpBuf, 1);
    return _URC_NO_REASON;
}

static void foo(int signum)
{
    C c;
    _Unwind_Exception* exceptionObject = new _Unwind_Exception;
    exceptionObject->exception_class = 0;
    exceptionObject->exception_cleanup = nullptr;
    _Unwind_ForcedUnwind(exceptionObject, stopCallback, (void*)666);
}

static __attribute__((__noinline__)) void bar()
{
    D d;
    signal(SIGUSR1, foo);
    raise(SIGUSR1);
}

int main()
{
    if (!setjmp(jmpBuf))
        bar();
    return 0;
}

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

struct E {
    E()
    {
        zprintf("E::E;");
    }

    ~E()
    {
        zprintf("E::~E;");
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

static __attribute__((__noinline__)) void foo()
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
    foo();
}

static __attribute__((__noinline__)) void baz()
{
    if (!setjmp(jmpBuf))
        bar();
}

int main()
{
    E e;
    baz();
    return 0;
}

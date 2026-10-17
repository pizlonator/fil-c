// The first member's null representation must survive pointer-word storage.
// In the Itanium ABI a null data-member pointer is -1, not all-zero bytes.
#include <cassert>
#include <cstring>

struct Owner { int value; };
using Member = int Owner::*;
union Choice { Member member; void* pointer; };

Choice staticChoice;
thread_local Choice threadChoice;
struct Nested : Owner { char lead; Choice choices[2]; char sentinel; };
Nested staticNested;
thread_local Nested threadNested;

struct AnonymousHolder {
    char lead;
    union { Member member; void* pointer; };
    char sentinel;
};
AnonymousHolder staticAnonymous;

// The anonymous aggregate is still the first member. Its data-member pointer
// at byte 8 catches confusing AST field bit offsets with byte offsets.
union AnonymousChoice {
    struct { unsigned long lead; Member member; };
    void* pointer;
};
AnonymousChoice staticAnonymousChoice;
struct Base { Member inherited; };
struct WithBase : Base { Member own; };
union RecordChoice { WithBase members; void* pointer; };
RecordChoice staticRecord;

// A typed zero aggregate has implicit LLVM padding. It must not replace the
// explicit zero bytes of the containing null initializer. The union also has
// a full extra word beyond the selected member, which must be zero, not undef.
struct PaddedZero { unsigned char byte; unsigned int word; };
struct NullPayload { PaddedZero zero; Member member; };
union PaddedChoice { NullPayload payload; void* pointers[3]; };
static_assert(sizeof(PaddedZero) == 8 && sizeof(NullPayload) == 16);
static_assert(sizeof(PaddedChoice) == 24);
PaddedChoice staticPadded[2];

static void checkPadding(const PaddedChoice& value)
{
    assert(value.payload.member == nullptr);
    unsigned char bytes[sizeof(value)];
    std::memcpy(bytes, &value, sizeof(bytes));
    for (unsigned i = 0; i < sizeof(bytes); ++i)
        assert(bytes[i] == (i >= 8 && i < 16 ? 0xff : 0));
}

// The base's complete size includes seven tail bytes; its base-subobject
// extent ends sooner so the derived field can reuse that padding. Mixed member
// access makes it non-POD for layout without a user-provided constructor.
struct TailBase {
    Member member;
private:
    unsigned char byte;
public:
    unsigned char getByte() const { return byte; }
};
struct TailDerived : TailBase { unsigned char sentinel; };
static_assert(sizeof(TailBase) == 16 && sizeof(TailDerived) == 16);

// A lifetime-extended temporary is allocated before its null initializer is
// emitted. Differently typed initializers must keep its size and access type.
union Dynamic {
    Member member;
    void* pointer;
    explicit Dynamic(int) : member(nullptr) {}
};
int runtimeValue = 17;
const Dynamic& temporary = Dynamic(runtimeValue);

int copyCount;
struct Counter {
    Counter() = default;
    Counter(const Counter&) { ++copyCount; }
    Counter& operator=(const Counter&) { ++copyCount; return *this; }
};
struct Grouped {
    char lead;
    Choice choice;
    char sentinel;
    Counter counter;
    Grouped() : lead(0x39), choice{nullptr}, sentinel(0x67) {}
    Grouped(const Grouped&) = default;
    Grouped& operator=(const Grouped&) = default;
};

int main()
{
    assert(staticChoice.member == nullptr && threadChoice.member == nullptr);
    assert(staticNested.choices[0].member == nullptr &&
           staticNested.choices[1].member == nullptr);
    assert(threadNested.choices[0].member == nullptr &&
           threadNested.choices[1].member == nullptr);
    assert(staticNested.lead == 0 && staticNested.sentinel == 0);
    assert(staticAnonymous.member == nullptr && staticAnonymous.sentinel == 0);
    assert(staticAnonymousChoice.lead == 0 &&
           staticAnonymousChoice.member == nullptr);
    assert(staticRecord.members.inherited == nullptr &&
           staticRecord.members.own == nullptr);
    assert(temporary.member == nullptr);

    checkPadding(staticPadded[0]);
    checkPadding(staticPadded[1]);
    auto* heapPadded = new PaddedChoice();
    checkPadding(*heapPadded);
    delete heapPadded;
    auto* derived = new TailDerived();
    assert(derived->member == nullptr && derived->getByte() == 0 && derived->sentinel == 0);
    derived->sentinel = 0x67;
    assert(derived->member == nullptr && derived->getByte() == 0 && derived->sentinel == 0x67);
    delete derived;

    Choice initialized{nullptr};
    assert(initialized.member == nullptr);
    Grouped source, assigned;
    Grouped constructed(source);
    assigned = source;
    assert(copyCount == 2);
    assert(constructed.choice.member == nullptr && assigned.choice.member == nullptr);
    assert(constructed.lead == 0x39 && assigned.sentinel == 0x67);

    // Bitwise representation changes must retain a nonzero member-pointer null.
    struct Bytes { unsigned char data[sizeof(Choice)]; };
    Bytes bytes = __builtin_bit_cast(Bytes, initialized);
    Choice recovered = __builtin_bit_cast(Choice, bytes);
    assert(recovered.member == nullptr);
}

#include <vector>
#include <random>
#include <stdint.h>
#include <stdarg.h>
#include <time.h>
#include <limits.h>
#include <stdexcept>
#include <memory>
#include <algorithm>

#include "message/MessageConverter.h"
#include "buffer/ProcessorSelector.h"


#define RND std::mt19937

typedef std::vector<uint8_t> Data;

//simple string operations
Data operator+ (const Data &a, const Data &b) {
    Data res = a;
    res.insert(res.end(), b.begin(), b.end());
    return res;
}
Data Substr(const Data &data, int start, int end = -1) {
    if (end < 0) end = data.size();
    start = std::max(std::min(start, (int)data.size()), 0);
    end = std::max(std::min(end, (int)data.size()), start);
    return Data(data.begin() + start, data.begin() + end);
}
Data Reverse(const Data &data) {
    Data res = data;
    std::reverse(res.begin(), res.end());
    return res;
}

class SimpleConverter {
protected:
    DataFormat format;  

public:
    static const int MaxCode = 0x10FFFF;
    static const int MaxBytes = 4;
    static int MaxCodeOfSize(int bytes) {
        if (bytes == 0) return -1;
        if (bytes == 1) return 0x0000007F;
        if (bytes == 2) return 0x000007FF;
        if (bytes == 3) return 0x0000FFFF;
        if (bytes == 4) return MaxCode;
        assert(0); return 0;
    }

    static bool IsCodeValid(int code) {
        return code >= 0 && code <= MaxCode && !(code >= 0xD800U && code < 0xE000U);
    }

    SimpleConverter(DataFormat format) : format(format) {}

    static void WriteWord8(Data &data, uint8_t word) {
        data.push_back(word & 0xFFU);
    }
    static void WriteWord16(Data &data, uint16_t word) {
        data.push_back(word & 0xFFU); word >>= 8;
        data.push_back(word & 0xFFU);
    }
    static void WriteWord32(Data &data, uint32_t word) {
        data.push_back(word & 0xFFU); word >>= 8;
        data.push_back(word & 0xFFU); word >>= 8;
        data.push_back(word & 0xFFU); word >>= 8;
        data.push_back(word & 0xFFU);
    }

    void AddChar(Data &data, int code, int utf8len = -1) const {
        //assert(IsCodeValid(code));
        if (format == dfUtf32) {
            WriteWord32(data, uint32_t(code));
        }
        else if (format == dfUtf16) {
            //from https://ru.wikipedia.org/wiki/UTF-16#.D0.9A.D0.BE.D0.B4.D0.B8.D1.80.D0.BE.D0.B2.D0.B0.D0.BD.D0.B8.D0.B5
            if (code < 0x10000)
                WriteWord16(data, uint16_t(code));
            else {
                code = code - 0x10000;
                int lo10 = (code & 0x03FF);
                int hi10 = (code >> 10);
                WriteWord16(data, uint16_t(0xD800 | hi10));
                WriteWord16(data, uint16_t(0xDC00 | lo10));
            }
        }
        else if (format == dfUtf8) {
            //from http://stackoverflow.com/a/6240184/556899
            if (code <= 0x0000007F && utf8len <= 1)             //0xxxxxxx
                WriteWord8(data, uint8_t(0x00 | ((code >>  0) & 0x7F)));
            else if (code <= 0x000007FF && utf8len <= 2) {      //110xxxxx 10xxxxxx
                WriteWord8(data, uint8_t(0xC0 | ((code >>  6) & 0x1F)));
                WriteWord8(data, uint8_t(0x80 | ((code >>  0) & 0x3F)));
            }
            else if (code <= 0x0000FFFF && utf8len <= 3) {      //1110xxxx 10xxxxxx 10xxxxxx
                WriteWord8(data, uint8_t(0xE0 | ((code >> 12) & 0x0F)));
                WriteWord8(data, uint8_t(0x80 | ((code >>  6) & 0x3F)));
                WriteWord8(data, uint8_t(0x80 | ((code >>  0) & 0x3F)));
            }
            else {                                              //11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
                WriteWord8(data, uint8_t(0xF0 | ((code >> 18) & 0x07)));
                WriteWord8(data, uint8_t(0x80 | ((code >> 12) & 0x3F)));
                WriteWord8(data, uint8_t(0x80 | ((code >>  6) & 0x3F)));
                WriteWord8(data, uint8_t(0x80 | ((code >>  0) & 0x3F)));
            }
        }
        else assert(0);
    }

    static int ParseWord8(const Data &data, const uint8_t *&ptr) {
        if (ptr + 1 > data.data() + data.size())
            throw std::runtime_error("Cannot read a byte");
        uint32_t a = (*ptr++);
        return a;
    }
    static int ParseWord16(const Data &data, const uint8_t *&ptr) {
        if (ptr + 2 > data.data() + data.size())
            throw std::runtime_error("Cannot read a 16-bit word");
        uint32_t a = (*ptr++);
        uint32_t b = (*ptr++);
        return a + (b << 8);
    }
    static int ParseWord32(const Data &data, const uint8_t *&ptr) {
        if (ptr + 4 > data.data() + data.size())
            throw std::runtime_error("Cannot read a 32-bit word");
        uint32_t a = (*ptr++);
        uint32_t b = (*ptr++);
        uint32_t c = (*ptr++);
        uint32_t d = (*ptr++);
        return a + (b << 8) + (c << 16) + (d << 24);
    }

    int ParseChar(const Data &data, const uint8_t *&uptr) const {
        const uint8_t *ptr = uptr;
        uint32_t code = (uint32_t)-1;

        if (format == dfUtf32) {
            code = ParseWord32(data, ptr);
        }
        else if (format == dfUtf16) {
            code = ParseWord16(data, ptr);
            if (code >= 0xD800U && code < 0xDC00U) {
                uint32_t rem = ParseWord16(data, ptr);
                if (!(rem >= 0xDC00U && rem < 0xE000U))
                    throw std::runtime_error("Second part of surrogate pair is out of range");
                code = ((code - 0xD800U) << 10) + (rem - 0xDC00U) + 0x010000;
            }
        }
        else if (format == dfUtf8) {
            code = ParseWord8(data, ptr);
            int cnt;
            if ((code & 0x80U) == 0x00U)            //0xxxxxxx
                cnt = 0;
            else if ((code & 0xE0U) == 0xC0U) {     //110xxxxx 10xxxxxx
                cnt = 1;
                code -= 0xC0U;
            }
            else if ((code & 0xF0U) == 0xE0U) {     //1110xxxx 10xxxxxx 10xxxxxx
                cnt = 2;
                code -= 0xE0U;
            }
            else if ((code & 0xF8U) == 0xF0U) {     //11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
                cnt = 3;
                code -= 0xF0U;
            }
            else
                throw std::runtime_error("Failed to parse leading byte");

            for (int t = 0; t < cnt; t++) {
                uint32_t add = ParseWord8(data, ptr);
                if ((add & 0xC0U) != 0x80U)
                    throw std::runtime_error("Continuation byte out of range");
                code = (code << 6) + (add - 0x80U);
            }
            if (int(code) <= MaxCodeOfSize(cnt))
                throw std::runtime_error("Overlong encoding");
        }
        else assert(0);

        if (!IsCodeValid(code))
            throw std::runtime_error("Parsed invalid code point");

        uptr = ptr;
        return code;
    }

    Data ByteToData(uint8_t byte) const {
        return Data(1, byte);
    }
    Data CodeToData(int code) const {
        Data res;
        AddChar(res, code);
        return res;
    }
    template<typename BytesContainer>
    Data BytesToData(BytesContainer bytes) {
        Data res;
        for (auto it = std::begin(bytes); it != std::end(bytes); it++) {
            uint8_t byte = *it;
            res.push_back(byte);
        }
        return res;
    }
    template<typename CodesContainer>
    Data CodesToData(CodesContainer codes) {
        Data res;
        for (auto it = std::begin(codes); it != std::end(codes); it++) {
            int code = *it;
            AddChar(res, code);
        }
        return res;
    }

    std::vector<int> ParseCodes(const Data &data) {
        const uint8_t *ptr = data.data();
        std::vector<int> res;
        while (ptr < data.data() + data.size()) {
            int code = ParseChar(data, ptr);
            res.push_back(code);
        }
        return res;
    }

    //always finds a split at range [pos .. pos+3]
    //if input is correct, then split is surely between two chars
    //if input is invalid, then any split can be returned
    int FindSplit(const Data &data, int pos) const {
        pos = std::max(std::min(pos, (int)data.size()), 0);
        if (format == dfUtf32) {
            //align with 32-bit words
            while (pos & 3) pos++;
        }
        else if (format == dfUtf16) {
            //align with 16-bit words
            while (pos & 1) pos++;
            //check if in the middle of surrogate pair
            if (pos + 1 < data.size()) {
                uint16_t val = data[pos] + (data[pos+1] << 8);
                if (val >= 0xDC00U && val < 0xE000U)
                    pos += 2;   //move past surrogate's second half
            }
        }
        else if (format == dfUtf8) {
            //move past continuation bytes (at most 3 of them)
            for (int i = 0; i < 3; i++)
                if (pos < data.size() && data[pos] >= 0x80U && data[pos] < 0xC0U)
                    pos++;
        }
        pos = std::min(pos, int(data.size()));
        return pos;
    }
};


class BaseGenerator : public SimpleConverter {
protected:
    RND &rnd;

    typedef std::uniform_int_distribution<int> Distrib;

public:
    BaseGenerator(DataFormat format, RND &rnd) : SimpleConverter(format), rnd(rnd) {}

//=============== Data generation ==================

    uint8_t RandomByte() {
        uint8_t byte = Distrib(0, 255)(rnd);
        return byte;
    }
    int RandomCode(int mask = 0) {
        if (mask == 0) mask = (1 << MaxBytes) - 1;
        assert(mask > 0 && mask < (1 << MaxBytes));

        int bytes;
        do {
            bytes = Distrib(1, MaxBytes)(rnd);
        } while (!(mask & (1 << (bytes-1))));

        int minCode = MaxCodeOfSize(bytes - 1) + 1;
        int maxCode = MaxCodeOfSize(bytes);
        int code;
        do {
            code = Distrib(minCode, maxCode)(rnd);
        } while (code >= 0xD800U && code < 0xE000U);

        return code;
    }
    std::vector<uint8_t> RandomBytes(int size) {
        std::vector<uint8_t> res;
        for (int i = 0; i < size; i++)
            res.push_back(RandomByte());
        return res;
    }
    std::vector<int> RandomCodes(int size, int mask = 0) {
        std::vector<int> res;
        for (int i = 0; i < size; i++)
            res.push_back(RandomCode(mask));
        return res;
    }

    int RandomPos(const Data &data) {
        int pos = Distrib(0, int(data.size()))(rnd);
        return pos;
    }
    int RandomSplit(const Data &data) {
        int pos = RandomPos(data);
        pos = FindSplit(data, pos);
        return pos;
    }
};


class TestsGenerator : public BaseGenerator {
    int mutRad;

public:
    TestsGenerator (DataFormat format, RND &rnd) : BaseGenerator(format, rnd) {
        SetMutationRadius();
    }

    //how close mutations are to each other (if there are several of them)
    void SetMutationRadius(int rad = 10) {
        mutRad = rad;
    }

    int PosByHint(const Data &data, int hint = -1) {
        if (hint < 0)
            hint = RandomPos(data);
        hint += Distrib(-mutRad, mutRad)(rnd);
        return hint;
    }

//============ Mutations of single data ===============

    int MutateDoubleBytes(Data &data, int hint = -1) {
        int pos = PosByHint(data, hint);
        int next = pos + Distrib(1, mutRad)(rnd);
        data = Substr(data, 0, pos) + Substr(data, pos, next) + Substr(data, pos, next) + Substr(data, next);
        return next;
    }
    int MutateDoubleChars(Data &data, int hint = -1) {
        int pos = FindSplit(data, PosByHint(data, hint));
        int next = FindSplit(data, pos + Distrib(1, mutRad)(rnd));
        data = Substr(data, 0, pos) + Substr(data, pos, next) + Substr(data, pos, next) + Substr(data, next);
        return next;
    }

    int MutateAddRandomBytes(Data &data, int hint = -1) {
        int pos = PosByHint(data, hint);
        data = Substr(data, 0, pos) + BytesToData(RandomBytes(Distrib(1, mutRad)(rnd))) + Substr(data, pos);
        return pos;
    }
    int MutateAddRandomChar(Data &data, int hint = -1) {
        int pos = FindSplit(data, PosByHint(data, hint));
        data = Substr(data, 0, pos) + CodesToData(RandomCodes(Distrib(1, mutRad)(rnd))) + Substr(data, pos);
        return pos;
    }

    int MutateRemoveRandomBytes(Data &data, int hint = -1) {
        int pos = PosByHint(data, hint);
        int next = pos + Distrib(1, mutRad)(rnd);
        data = Substr(data, 0, pos) + Substr(data, next);
        return pos;
    }
    int MutateRemoveRandomChars(Data &data, int hint = -1) {
        int pos = FindSplit(data, PosByHint(data, hint));
        int next = FindSplit(data, pos + Distrib(1, mutRad)(rnd));
        data = Substr(data, 0, pos) + Substr(data, next);
        return pos;
    }

    int MutateRevertBytes(Data &data, int hint = -1) {
        int pos = PosByHint(data, hint);
        int next = pos + Distrib(1, mutRad)(rnd);
        data = Substr(data, 0, pos) + Reverse(Substr(data, pos, next)) + Substr(data, next);
        return pos;
    }

    int MutateShortenEnd(Data &data, int hint = -1) {
        int num = Distrib(1, mutRad)(rnd);
        if (Distrib(0, 1)(rnd))
            data = Substr(data, 0, data.size() - num);
        else 
            data = Substr(data, num);
        return hint;
    }

    int MutateMakeOverlong(Data &data, int hint = -1) {
        if (format != dfUtf8)
            return hint;
        int pos = FindSplit(data, PosByHint(data, hint));
        const uint8_t *ptr = data.data() + pos;
        try {
            int code = ParseChar(data, ptr);
            int next = ptr - data.data();
            int len = next - pos;
            len = Distrib(len, 4)(rnd);
            Data prefD = Substr(data, 0, pos);
            AddChar(prefD, code, len);
            data = prefD + Substr(data, next);
        }
        catch (std::runtime_error &e) {}
        return pos;
    }

    int MutateAddWrongCode(Data &data, int hint = -1) {
        int pos = FindSplit(data, PosByHint(data, hint));
        int code;
        if (Distrib(0, 2)(rnd))
            code = Distrib(0xD800, 0xE000)(rnd);
        else
            code = Distrib(0x10FFFF, 0x1FFFFF)(rnd);
        data = Substr(data, 0, pos) + CodeToData(code) + Substr(data, pos);
        return pos;
    }

//============ Mixes of several data ================

    static Data MixConcatenate(const Data &a, const Data &b) {
        return a + b;
    }
    Data MixOneInsideOther(const Data &a, const Data &b) {
        int pos = RandomPos(a);
        return Substr(a, 0, pos) + b + Substr(a, pos);
    }
    Data MixInterleaveBytes(const Data &a, const Data &b) {
        Data res;
        int pa = 0, pb = 0;
        while (pa < a.size() || pb < b.size()) {
            int q = Distrib(0, 1)(rnd);
            if (q == 0 && pa < a.size())
                res.push_back(a[pa++]);
            if (q == 1 && pb < b.size())
                res.push_back(b[pb++]);
        }
        return res;
    }

//============ apply many mutations ================

    int MutateAny(Data &data, int hint = -1) {
        int t = Distrib(0, 9)(rnd);
        if (t == 0) return MutateDoubleBytes(data, hint);
        if (t == 1) return MutateDoubleChars(data, hint);
        if (t == 2) return MutateAddRandomBytes(data, hint);
        if (t == 3) return MutateAddRandomChar(data, hint);
        if (t == 4) return MutateRemoveRandomBytes(data, hint);
        if (t == 5) return MutateRemoveRandomChars(data, hint);
        if (t == 6) return MutateRevertBytes(data, hint);
        if (t == 7) return MutateShortenEnd(data, hint);
        if (t == 8) return MutateMakeOverlong(data, hint);
        if (t == 9) return MutateAddWrongCode(data, hint);
        return hint;
    }

    int MutateSeveral(Data &data, int hint = -1) {
        int t = Distrib(0, 99)(rnd);
        int k = 0;
        if (t < 40) k = 1;
        else if (t < 60) k = 2;
        else if (t < 80) k = 5;
        else k = 10;
    
        for (int i = 0; i < k; i++)
            hint = MutateAny(data, hint);

        return hint;
    }
};

struct Result {
    bool success;
    int inDone;
    Data data;

    Result() : success(false), inDone(-1) {}
    Result(bool success, int inDone, Data &&data) : success(success), inDone(inDone), data(std::move(data)) {}
};

Result SimpleConvert(const Data &data, DataFormat from, DataFormat to, int *maxBytes = 0) {
    SimpleConverter Usrc(from), Udst(to);

    const uint8_t *ptr = data.data();
    std::vector<int> codes;
    bool error = false;
    try {
        while (ptr < data.data() + data.size()) {
            int code = Usrc.ParseChar(data, ptr);
            codes.push_back(code);
        }
    }
    catch (std::exception &e) {
        error = true;
    }

    if (maxBytes) {
        if (error)
            *maxBytes = INT_MAX;
        else {
            int maxCode = (codes.empty() ? 0 : *std::max_element(codes.begin(), codes.end()));
            int maxLen = 1;
            while (maxCode > Usrc.MaxCodeOfSize(maxLen))
                maxLen++;
            *maxBytes = maxLen;
        }
    }

    return Result(!error, ptr - data.data(), Udst.CodesToData(codes));
}

//linked from AllProcessors.cpp
BaseBufferProcessor *GenerateProcessor(int srcFormat, int dstFormat, int maxBytes, int checkMode, int multiplier, int *errorCounter = 0);

Result TestedConvert(const Data &data, DataFormat from, DataFormat to, BaseBufferProcessor *processor) {
    long long outMaxSize = ConvertInMemorySize(*processor, data.size());
    Data answer(outMaxSize);
    auto res = ConvertInMemory(*processor, (const char*)data.data(), data.size(), (char*)answer.data(), answer.size());
    assert(res.status != csOverflowPossible);
    assert(res.status != csInputOutputNoAccess);
    answer.resize(res.outputSize);
    return Result(res.status == csSuccess, res.inputSize, std::move(answer));
}

bool CheckResults(const Result &ans, const Result &out) {
    if (ans.success != out.success)
        return false;   //validity check failed
    if (ans.inDone != out.inDone)
        return false;   //processed different number of bytes
    if (ans.data.size() != out.data.size())
        return false;   //produced different number of bytes
    if (ans.data != out.data)
        return false;   //output data is wrong
    return true;
}

void DumpDataToFile(const Data *data, const char *filename) {
    if (data) {
        FILE *f = fopen(filename, "wb");
        fwrite(data->data(), data->size(), 1, f);
        fclose(f);
    }
}

//enabled by console parameter (allows saving crashing tests before they happened)
bool DumpCurrentInput = false;

void RunTest(const Data &data, const std::string &name, const std::string *options = 0) {
    std::string str(data.begin(), data.end());
    uint32_t hash = std::hash<std::string>()(str);
    printf("%30s [%7u] (%08X):   ", name.c_str(), unsigned(data.size()), hash);

    if (DumpCurrentInput)
        DumpDataToFile(&data, "test_1in.bin");

    Result answer;

    auto RunConversion = [&](DataFormat from, DataFormat to, int mode, int bytes, int streams) -> void {
        if (DumpCurrentInput) {
            FILE *f = fopen("test_0info.txt", "wt");
            fprintf(f, "%s (%08X)\n", name.c_str(), hash);
            fprintf(f, "from = %d  to = %d  mode = %d  bytes = %d  streams = %d\n", from, to, mode, bytes, streams);
            fclose(f);
        }
        
        std::unique_ptr<BaseBufferProcessor> processor(GenerateProcessor(from, to, bytes, mode, streams));
        auto result = TestedConvert(data, from, to, processor.get());
        printf("%c", (result.success ? '#' : 'o'));

        if (!CheckResults(answer, result)) {
            printf("Error!\n");
            if (!options) {
                FILE *f = fopen("test_0info.txt", "wt");
                fprintf(f, "%s (%08X)\n", name.c_str(), hash);
                fprintf(f, "from = %d  to = %d  mode = %d  bytes = %d  streams = %d\n", from, to, mode, bytes, streams);
                fprintf(f, "answer: %d -> %d  (%s)\n", answer.inDone, (int)answer.data.size(), answer.success ? "OK" : "WR");
                fprintf(f, "result: %d -> %d  (%s)\n", result.inDone, (int)result.data.size(), result.success ? "OK" : "WR");
                fclose(f);
                DumpDataToFile(&data, "test_1in.bin");
                DumpDataToFile(&answer.data, "test_2ans.bin");
                DumpDataToFile(&result.data, "test_3res.bin");
            }
            std::terminate();
        }
    };

    auto RunDir = [&](DataFormat from, DataFormat to) -> void {
        int maxBytes = -1;
        answer = SimpleConvert(data, from, to, &maxBytes);
        printf(" %c[", (answer.success ? '#' : 'o'));

        RunConversion(from, to, 2, 0, 1);
        for (int mode = 0; mode <= 2; mode++)
            for (int bytes = 1; bytes <= 3; bytes++) {
                static const int streams[] = {1, 4};
                for (int s = 0; s < 2; s++) {
                    if (mode != 2 && maxBytes == INT_MAX) {
                        printf(".");
                        continue;       //invalid input is supported only on "Validate" mode
                    }
                    if (mode == 0 && bytes < maxBytes) {
                        printf(".");
                        continue;       //no support for bigger codes in "Fast" mode 
                    }
                    RunConversion(from, to, mode, bytes, streams[s]);
                }
            }

        printf("]");
    };


    if (options) {
        int fromX, toX, mode, bytes, streams;
        sscanf(options->c_str(), "from = %d  to = %d  mode = %d  bytes = %d  streams = %d\n", &fromX, &toX, &mode, &bytes, &streams);
        DataFormat from = DataFormat(fromX), to = DataFormat(toX);
        answer = SimpleConvert(data, from, to);
        RunConversion(from, to, mode, bytes, streams);
    }
    else {
        for (int from = 0; from < dfUtfCount; from++)
            for (int to = 0; to < dfUtfCount; to++) {
                if ((from == dfUtf8) == (to == dfUtf8))
                    continue;
                RunDir(DataFormat(from), DataFormat(to));
            }
    }

    printf("\n");
}

void RunTestF(const Data &data, const char *format, ...) {
    va_list args;
    va_start(args, format);
    char name[256];
    vsprintf(name, format, args);
    va_end(args);
    RunTest(data, std::string(name));
}


int main(int argc, char **argv) {
    RND rnd;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0) {
            //replay old test, for debugging
            FILE *ft = fopen("test_0info.txt", "rt");
            char buff[256];
            fgets(buff, 256, ft);
            fgets(buff, 256, ft);
            std::string options = buff;
            fclose(ft);
            FILE *fi = fopen("test_1in.bin", "rb");
            fseek(fi, 0, SEEK_END);
            int inputSize = ftell(fi);
            fseek(fi, 0, SEEK_SET);
            Data data(inputSize);
            fread(data.data(), 1, inputSize, fi);
            fclose(fi);

            RunTest(data, "replay", &options);
            return 0;
        }
        if (strcmp(argv[i], "-nd") == 0) {
            rnd.seed(time(0) + clock());
        }
        if (strcmp(argv[i], "-s") == 0) {
            DumpCurrentInput = true;
        }
    }


    RunTestF(Data(), "empty");

    TestsGenerator gl(dfUtf32, rnd);
    for (int i = 1; i <= 32; i++)
        RunTestF(gl.RandomBytes(i), "random_bytes_%d", i);

    for (int fmt = 0; fmt < dfUtfCount; fmt++) {
        TestsGenerator gen(DataFormat(fmt), rnd);

        for (int i = 1; i <= 32; i++)
            for (int b = 1; b < 16; b++)
                RunTestF(gen.CodesToData(gen.RandomCodes(i, b)), "%d_random_codes(%d)_%d", fmt, b, i);
    }

    printf("\n");
    printf("================================================================================\n");
    printf("                                                                                \n");
    printf("================================================================================\n");
    printf("\n");

    for (int fmt = 0; fmt < dfUtfCount; fmt++) {
        TestsGenerator gen(DataFormat(fmt), rnd);
        static const int K = 0x10091;
        std::vector<int> codes;
        for (int i = 0; i < K; i++) if (gen.IsCodeValid(i))
            codes.push_back(i);
        codes.push_back(int(gen.MaxCode));
        codes.push_back(int(gen.MaxCode));
        for (int i = K-1; i >= 0; i--) if (gen.IsCodeValid(i))
            codes.push_back(i);
        Data data = gen.CodesToData(codes);
        RunTestF(data, "%d_all_sequental_codes", fmt);

        std::random_shuffle(codes.begin(), codes.end());
        Data data2 = gen.CodesToData(codes);
        RunTestF(data2, "%d_all_shuffled_codes", fmt);

        data = gen.CodesToData(std::vector<int>(1000, 0));
        RunTestF(data, "%d_samecode_%d", fmt, 0);
        data = gen.CodesToData(std::vector<int>(1001, 255));
        RunTestF(data, "%d_samecode_%d", fmt, 255);
        data = gen.CodesToData(std::vector<int>(1024, 65535));
        RunTestF(data, "%d_samecode_%d", fmt, 65535);
        data = gen.CodesToData(std::vector<int>(1017, 1000000));
        RunTestF(data, "%d_samecode_%d", fmt, 1000000);
    }

    printf("\n");
    printf("================================================================================\n");
    printf("                                                                                \n");
    printf("================================================================================\n");
    printf("\n");

    while (1) {
        for (int fmt = 0; fmt < dfUtfCount; fmt++) {
            TestsGenerator gen(DataFormat(fmt), rnd);

            std::vector<int> codes;
            for (int i = 0; i < 65600; i++) if (gen.IsCodeValid(i))
                codes.push_back(i);
            std::random_shuffle(codes.begin(), codes.end());
            Data data = gen.CodesToData(codes);
            gen.MutateSeveral(data);
            RunTestF(data, "%d_all_shuffled_codes_M", fmt);

            int len = -1, r = std::uniform_int_distribution<int>(0, 100)(rnd);
            if (false);
            else if (r <  1) len = std::uniform_int_distribution<int>(100000, 1000000)(rnd);
            else if (r < 10) len = std::uniform_int_distribution<int>(10000, 100000)(rnd);
            else if (r < 40) len = std::uniform_int_distribution<int>(1000, 10000)(rnd);
            else if (r < 70) len = std::uniform_int_distribution<int>(100, 1000)(rnd);
            else if ( true ) len = std::uniform_int_distribution<int>(0, 100)(rnd);
            for (int b = 1; b < 16; b++) {
                Data data = gen.CodesToData(gen.RandomCodes(len, b));
                gen.MutateSeveral(data);
                RunTestF(data, "%d_random_codes(%d)_M", fmt, b);
            }
        }
    }

/*    for (int t = 32; t <= 1<<20; t *= 2)
        for (int i = t-20; i <= t+20; i++)
            RunTestF(gl.RandomBytes(i), "random_bytes_%d", i);

    for (int fmt = 0; fmt < dfUtfCount; fmt++) {
        TestsGenerator gen(DataFormat(fmt), rnd);

        for (int t = 32; t <= 1<<18; t *= 2)
            for (int i = t-10; i <= t+10; i++)
                for (int b = 1; b < 16; b++)
                    RunTestF(gen.CodesToData(gen.RandomCodes(i, b)), "%d_random_codes(%d)_%d", fmt, b, i);
    }*/

    return 0;
}



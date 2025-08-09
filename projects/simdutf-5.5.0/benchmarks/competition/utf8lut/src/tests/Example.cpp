#include "buffer/ProcessorSelector.h"
#include "message/MessageConverter.h"
#include <stdio.h>
#include <memory>

int main() {
    std::unique_ptr<BaseBufferProcessor> processor(ProcessorSelector<dfUtf8, dfUtf16>::WithOptions<>::Create());
    ConversionResult res = ConvertFile(*processor, "input.utf8", "output.utf16");
    if (res.status == csSuccess) printf("File converted successfully\n");
    else printf("Conversion failed (error: %d)\n", res.status);
	printf("Converted %d bytes into %d bytes", res.inputSize, res.outputSize);
}

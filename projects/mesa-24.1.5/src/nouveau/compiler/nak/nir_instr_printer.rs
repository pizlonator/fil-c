// Copyright © 2024 Collabora, Ltd.
// SPDX-License-Identifier: MIT

use std::pin::Pin;

use nak_bindings::nak_clear_memstream;
use nak_bindings::nak_close_memstream;
use nak_bindings::nak_memstream;
use nak_bindings::nak_nir_asprint_instr;
use nak_bindings::nak_open_memstream;
use nak_bindings::nir_instr;

/// A memstream that holds the printed NIR instructions.
pub struct NirInstrPrinter {
    //  XXX: we need this to be pinned because we've passed references to its
    //  fields when calling open_memstream.
    stream: Pin<Box<nak_memstream>>,
}

impl NirInstrPrinter {
    pub fn new() -> Self {
        let mut stream =
            Box::pin(unsafe { std::mem::zeroed::<nak_memstream>() });
        unsafe {
            nak_open_memstream(stream.as_mut().get_unchecked_mut());
        }
        Self { stream }
    }

    /// Prints the given NIR instruction.
    pub fn instr_to_string(&mut self, instr: &nir_instr) -> String {
        unsafe {
            let stream = self.stream.as_mut().get_unchecked_mut();
            nak_nir_asprint_instr(stream, instr);
            let bytes = std::slice::from_raw_parts(
                stream.buffer as *const u8,
                stream.written,
            );
            let string = String::from_utf8_lossy(bytes).into_owned();
            nak_clear_memstream(stream);
            string
        }
    }
}

impl Drop for NirInstrPrinter {
    fn drop(&mut self) {
        unsafe {
            let stream = self.stream.as_mut().get_unchecked_mut();
            nak_close_memstream(stream)
        }
    }
}

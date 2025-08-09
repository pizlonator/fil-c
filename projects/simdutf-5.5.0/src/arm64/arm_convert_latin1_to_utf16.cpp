template <endianness big_endian>
std::pair<const char*, char16_t*> arm_convert_latin1_to_utf16(const char* buf, size_t len, char16_t* utf16_output) {
    const char* end = buf + len;

    while (end - buf >= 16) {
        uint8x16_t in8 = vld1q_u8(reinterpret_cast<const uint8_t *>(buf));
        uint16x8_t inlow = vmovl_u8(vget_low_u8(in8));
        if (!match_system(big_endian)) { inlow = vreinterpretq_u16_u8(vrev16q_u8(vreinterpretq_u8_u16(inlow))); }
        vst1q_u16(reinterpret_cast<uint16_t *>(utf16_output), inlow);
        uint16x8_t inhigh = vmovl_u8(vget_high_u8(in8));
        if (!match_system(big_endian)) { inhigh = vreinterpretq_u16_u8(vrev16q_u8(vreinterpretq_u8_u16(inhigh))); }
        vst1q_u16(reinterpret_cast<uint16_t *>(utf16_output+8), inhigh);
        utf16_output += 16;
        buf += 16;
    }

    return std::make_pair(buf, utf16_output);
}

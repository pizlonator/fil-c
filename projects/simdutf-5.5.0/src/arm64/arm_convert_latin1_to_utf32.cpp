std::pair<const char*, char32_t*> arm_convert_latin1_to_utf32(const char* buf, size_t len, char32_t* utf32_output) {
    const char* end = buf + len;

    while (end - buf >= 16) {
        uint8x16_t in8 = vld1q_u8(reinterpret_cast<const uint8_t *>(buf));
        uint16x8_t in8low = vmovl_u8(vget_low_u8(in8));
        uint32x4_t in16lowlow = vmovl_u16(vget_low_u16(in8low));
        uint32x4_t in16lowhigh = vmovl_u16(vget_high_u16(in8low));
        uint16x8_t in8high = vmovl_u8(vget_high_u8(in8));
        uint32x4_t in8highlow = vmovl_u16(vget_low_u16(in8high));
        uint32x4_t in8highhigh = vmovl_u16(vget_high_u16(in8high));
        vst1q_u32(reinterpret_cast<uint32_t *>(utf32_output), in16lowlow);
        vst1q_u32(reinterpret_cast<uint32_t *>(utf32_output+4), in16lowhigh);
        vst1q_u32(reinterpret_cast<uint32_t *>(utf32_output+8), in8highlow);
        vst1q_u32(reinterpret_cast<uint32_t *>(utf32_output+12), in8highhigh);

        utf32_output += 16;
        buf += 16;
    }

    return std::make_pair(buf, utf32_output);
}

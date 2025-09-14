;;; charscript.el --- character script table  -*- lexical-binding:t -*-
;;; Automatically generated from admin/unidata/{Blocks,emoji-data}.txt
(let (script-list)
  (dolist (elt '(
    (#x0000 #x007F latin) ; Basic Latin
    (#x00A0 #x024F latin) ; Latin-1 Supplement, Latin Extended-A, Latin Extended-B
    (#x0250 #x02AF phonetic) ; IPA Extensions
    (#x02B0 #x036F latin) ; Spacing Modifier Letters, Combining Diacritical Marks
    (#x0370 #x03E1 greek) ; Greek and Coptic
    (#x03E2 #x03EF coptic)
    (#x03F0 #x03FF greek)
    (#x0400 #x052F cyrillic) ; Cyrillic, Cyrillic Supplement
    (#x0530 #x058F armenian)
    (#x0590 #x05FF hebrew)
    (#x0600 #x06FF arabic)
    (#x0700 #x074F syriac)
    (#x0750 #x077F arabic) ; Arabic Supplement
    (#x0780 #x07BF thaana)
    (#x07C0 #x07FF nko)
    (#x0800 #x083F samaritan)
    (#x0840 #x085F mandaic)
    (#x0860 #x086F syriac) ; Syriac Supplement
    (#x0870 #x08FF arabic) ; Arabic Extended-B, Arabic Extended-A
    (#x0900 #x097F devanagari)
    (#x0980 #x09FF bengali)
    (#x0A00 #x0A7F gurmukhi)
    (#x0A80 #x0AFF gujarati)
    (#x0B00 #x0B7F oriya)
    (#x0B80 #x0BFF tamil)
    (#x0C00 #x0C7F telugu)
    (#x0C80 #x0CFF kannada)
    (#x0D00 #x0D7F malayalam)
    (#x0D80 #x0DFF sinhala)
    (#x0E00 #x0E7F thai)
    (#x0E80 #x0EFF lao)
    (#x0F00 #x0FFF tibetan)
    (#x1000 #x109F burmese) ; Myanmar
    (#x10A0 #x10FF georgian)
    (#x1100 #x11FF hangul) ; Hangul Jamo
    (#x1200 #x139F ethiopic) ; Ethiopic, Ethiopic Supplement
    (#x13A0 #x13FF cherokee)
    (#x1400 #x167F canadian-aboriginal)
    (#x1680 #x169F ogham)
    (#x16A0 #x16FF runic)
    (#x1700 #x171F tagalog)
    (#x1720 #x173F hanunoo)
    (#x1740 #x175F buhid)
    (#x1760 #x177F tagbanwa)
    (#x1780 #x17FF khmer)
    (#x1800 #x18AF mongolian)
    (#x18B0 #x18FF canadian-aboriginal)
    (#x1900 #x194F limbu)
    (#x1950 #x197F tai-le)
    (#x1980 #x19DF tai-lue)
    (#x19E0 #x19FF khmer) ; Khmer Symbols
    (#x1A00 #x1A1F buginese)
    (#x1A20 #x1AAF tai-tham)
    (#x1AB0 #x1AFF latin) ; Combining Diacritical Marks Extended
    (#x1B00 #x1B7F balinese)
    (#x1B80 #x1BBF sundanese)
    (#x1BC0 #x1BFF batak)
    (#x1C00 #x1C4F lepcha)
    (#x1C50 #x1C7F ol-chiki)
    (#x1C80 #x1C8F cyrillic) ; Cyrillic Extended-C
    (#x1C90 #x1CBF georgian) ; Georgian Extended
    (#x1CC0 #x1CCF sundanese) ; Sundanese Supplement
    (#x1CD0 #x1CFF vedic) ; Vedic Extensions
    (#x1D00 #x1DBF phonetic) ; Phonetic Extensions, Phonetic Extensions Supplement
    (#x1DC0 #x1EFF latin) ; Combining Diacritical Marks Supplement, Latin Extended Additional
    (#x1F00 #x1FFF greek) ; Greek Extended
    (#x2000 #x27FF symbol) ; General Punctuation, Superscripts and Subscripts, Currency Symbols, Combining Diacritical Marks for Symbols, Letterlike Symbols, Number Forms, Arrows, Mathematical Operators, Miscellaneous Technical, Control Pictures, Optical Character Recognition, Enclosed Alphanumerics, Box Drawing, Block Elements, Geometric Shapes, Miscellaneous Symbols, Dingbats, Miscellaneous Mathematical Symbols-A, Supplemental Arrows-A
    (#x2800 #x28FF braille) ; Braille Patterns
    (#x2900 #x2BFF symbol) ; Supplemental Arrows-B, Miscellaneous Mathematical Symbols-B, Supplemental Mathematical Operators, Miscellaneous Symbols and Arrows
    (#x2C00 #x2C5F glagolitic)
    (#x2C60 #x2C7F latin) ; Latin Extended-C
    (#x2C80 #x2CFF coptic)
    (#x2D00 #x2D2F georgian) ; Georgian Supplement
    (#x2D30 #x2D7F tifinagh)
    (#x2D80 #x2DDF ethiopic) ; Ethiopic Extended
    (#x2DE0 #x2DFF cyrillic) ; Cyrillic Extended-A
    (#x2E00 #x2E7F symbol) ; Supplemental Punctuation
    (#x2E80 #x2FDF han) ; CJK Radicals Supplement, Kangxi Radicals
    (#x2FF0 #x2FFF ideographic-description)
    (#x3000 #x303F cjk-misc)
    (#x3040 #x30FF kana) ; Hiragana, Katakana
    (#x3100 #x312F bopomofo)
    (#x3130 #x318F hangul) ; Hangul Compatibility Jamo
    (#x3190 #x319F kanbun)
    (#x31A0 #x31BF bopomofo) ; Bopomofo Extended
    (#x31C0 #x31EF cjk-misc)
    (#x31F0 #x31FF kana) ; Katakana Phonetic Extensions
    (#x3200 #x4DBF han) ; Enclosed CJK Letters and Months, CJK Compatibility, CJK Unified Ideographs Extension A
    (#x4DC0 #x4DFF cjk-misc)
    (#x4E00 #x9FFF han) ; CJK Unified Ideographs
    (#xA000 #xA4CF yi) ; Yi Syllables, Yi Radicals
    (#xA4D0 #xA4FF lisu)
    (#xA500 #xA63F vai)
    (#xA640 #xA69F cyrillic) ; Cyrillic Extended-B
    (#xA6A0 #xA6FF bamum)
    (#xA700 #xA7FF latin) ; Modifier Tone Letters, Latin Extended-D
    (#xA800 #xA82F syloti-nagri)
    (#xA830 #xA83F north-indic-number)
    (#xA840 #xA87F phags-pa)
    (#xA880 #xA8DF saurashtra)
    (#xA8E0 #xA8FF devanagari) ; Devanagari Extended
    (#xA900 #xA92F kayah-li)
    (#xA930 #xA95F rejang)
    (#xA960 #xA97F hangul) ; Hangul Jamo Extended-A
    (#xA980 #xA9DF javanese)
    (#xA9E0 #xA9FF burmese) ; Myanmar Extended-B
    (#xAA00 #xAA5F cham)
    (#xAA60 #xAA7F burmese) ; Myanmar Extended-A
    (#xAA80 #xAADF tai-viet)
    (#xAAE0 #xAAFF meetei-mayek)
    (#xAB00 #xAB2F ethiopic) ; Ethiopic Extended-A
    (#xAB30 #xAB6F latin) ; Latin Extended-E
    (#xAB70 #xABBF cherokee) ; Cherokee Supplement
    (#xABC0 #xABFF meetei-mayek)
    (#xAC00 #xD7FF hangul) ; Hangul Syllables, Hangul Jamo Extended-B
    (#xF900 #xFAFF han) ; CJK Compatibility Ideographs
    (#xFB00 #xFB06 latin) ; Alphabetic Presentation Forms
    (#xFB13 #xFB17 armenian)
    (#xFB1D #xFB4F hebrew)
    (#xFB50 #xFDFF arabic) ; Arabic Presentation Forms-A
    (#xFE10 #xFE1F vertical-form)
    (#xFE20 #xFE2F latin) ; Combining Half Marks
    (#xFE30 #xFE4F han) ; CJK Compatibility Forms
    (#xFE50 #xFE6F symbol) ; Small Form Variants
    (#xFE70 #xFEFF arabic) ; Arabic Presentation Forms-B
    (#xFF00 #xFF60 cjk-misc)
    (#xFF61 #xFF9F kana)
    (#xFFA0 #xFFDF hangul)
    (#xFFE0 #xFFEF cjk-misc)
    (#x10000 #x100FF linear-b)
    (#x10100 #x1013F aegean-number)
    (#x10140 #x1018F ancient-greek-number)
    (#x10190 #x101CF ancient-symbol)
    (#x101D0 #x101FF phaistos-disc)
    (#x10280 #x1029F lycian)
    (#x102A0 #x102DF carian)
    (#x102E0 #x102FF coptic) ; Coptic Epact Numbers
    (#x10300 #x1032F old-italic)
    (#x10330 #x1034F gothic)
    (#x10350 #x1037F old-permic)
    (#x10380 #x1039F ugaritic)
    (#x103A0 #x103DF old-persian)
    (#x10400 #x1044F deseret)
    (#x10450 #x1047F shavian)
    (#x10480 #x104AF osmanya)
    (#x104B0 #x104FF osage)
    (#x10500 #x1052F elbasan)
    (#x10530 #x1056F caucasian-albanian)
    (#x10570 #x105BF vithkuqi)
    (#x10600 #x1077F linear-a)
    (#x10780 #x107BF latin) ; Latin Extended-F
    (#x10800 #x1083F cypriot-syllabary)
    (#x10840 #x1085F aramaic) ; Imperial Aramaic
    (#x10860 #x1087F palmyrene)
    (#x10880 #x108AF nabataean)
    (#x108E0 #x108FF hatran)
    (#x10900 #x1091F phoenician)
    (#x10920 #x1093F lydian)
    (#x10980 #x109FF meroitic) ; Meroitic Hieroglyphs, Meroitic Cursive
    (#x10A00 #x10A5F kharoshthi)
    (#x10A60 #x10A7F old-south-arabian)
    (#x10A80 #x10A9F old-north-arabian)
    (#x10AC0 #x10AFF manichaean)
    (#x10B00 #x10B3F avestan)
    (#x10B40 #x10B5F inscriptional-parthian)
    (#x10B60 #x10B7F inscriptional-pahlavi)
    (#x10B80 #x10BAF psalter-pahlavi)
    (#x10C00 #x10C4F old-turkic)
    (#x10C80 #x10CFF old-hungarian)
    (#x10D00 #x10D3F hanifi-rohingya)
    (#x10E60 #x10E7F arabic) ; Rumi Numeral Symbols
    (#x10E80 #x10EBF yezidi)
    (#x10EC0 #x10EFF arabic) ; Arabic Extended-C
    (#x10F00 #x10F2F old-sogdian)
    (#x10F30 #x10F6F sogdian)
    (#x10F70 #x10FAF old-uyghur)
    (#x10FB0 #x10FDF chorasmian)
    (#x10FE0 #x10FFF elymaic)
    (#x11000 #x1107F brahmi)
    (#x11080 #x110CF kaithi)
    (#x110D0 #x110FF sora-sompeng)
    (#x11100 #x1114F chakma)
    (#x11150 #x1117F mahajani)
    (#x11180 #x111DF sharada)
    (#x111E0 #x111FF sinhala) ; Sinhala Archaic Numbers
    (#x11200 #x1124F khojki)
    (#x11280 #x112AF multani)
    (#x112B0 #x112FF khudawadi)
    (#x11300 #x1137F grantha)
    (#x11400 #x1147F newa)
    (#x11480 #x114DF tirhuta)
    (#x11580 #x115FF siddham)
    (#x11600 #x1165F modi)
    (#x11660 #x1167F mongolian) ; Mongolian Supplement
    (#x11680 #x116CF takri)
    (#x11700 #x1174F ahom)
    (#x11800 #x1184F dogra)
    (#x118A0 #x118FF warang-citi)
    (#x11900 #x1195F dives-akuru)
    (#x119A0 #x119FF nandinagari)
    (#x11A00 #x11A4F zanabazar-square)
    (#x11A50 #x11AAF soyombo)
    (#x11AB0 #x11ABF canadian-aboriginal)
    (#x11AC0 #x11AFF pau-cin-hau)
    (#x11B00 #x11B5F devanagari) ; Devanagari Extended-A
    (#x11C00 #x11C6F bhaiksuki)
    (#x11C70 #x11CBF marchen)
    (#x11D00 #x11D5F masaram-gondi)
    (#x11D60 #x11DAF gunjala-gondi)
    (#x11EE0 #x11EFF makasar)
    (#x11F00 #x11F5F kawi)
    (#x11FB0 #x11FBF lisu) ; Lisu Supplement
    (#x11FC0 #x11FFF tamil) ; Tamil Supplement
    (#x12000 #x1254F cuneiform) ; Cuneiform, Cuneiform Numbers and Punctuation, Early Dynastic Cuneiform
    (#x12F90 #x12FFF cypro-minoan)
    (#x13000 #x1345F egyptian) ; Egyptian Hieroglyphs, Egyptian Hieroglyph Format Controls
    (#x14400 #x1467F anatolian) ; Anatolian Hieroglyphs
    (#x16800 #x16A3F bamum) ; Bamum Supplement
    (#x16A40 #x16A6F mro)
    (#x16A70 #x16ACF tangsa)
    (#x16AD0 #x16AFF bassa-vah)
    (#x16B00 #x16B8F pahawh-hmong)
    (#x16E40 #x16E9F medefaidrin)
    (#x16F00 #x16F9F miao)
    (#x16FE0 #x16FFF symbol) ; Ideographic Symbols and Punctuation
    (#x17000 #x18AFF tangut) ; Tangut, Tangut Components
    (#x18B00 #x18CFF khitan-small-script)
    (#x18D00 #x18D7F tangut) ; Tangut Supplement
    (#x1AFF0 #x1B16F kana) ; Kana Extended-B, Kana Supplement, Kana Extended-A, Small Kana Extension
    (#x1B170 #x1B2FF nushu)
    (#x1BC00 #x1BCAF duployan-shorthand)
    (#x1CF00 #x1CFCF znamenny-musical-notation)
    (#x1D000 #x1D0FF byzantine-musical-symbol)
    (#x1D100 #x1D1FF musical-symbol)
    (#x1D200 #x1D24F ancient-greek-musical-notation)
    (#x1D2C0 #x1D2DF kaktovik-numeral)
    (#x1D2E0 #x1D2FF mayan-numeral)
    (#x1D300 #x1D35F tai-xuan-jing-symbol)
    (#x1D360 #x1D37F counting-rod-numeral)
    (#x1D400 #x1D7FF mathematical) ; Mathematical Alphanumeric Symbols
    (#x1D800 #x1DAAF sutton-sign-writing)
    (#x1DF00 #x1DFFF latin) ; Latin Extended-G
    (#x1E000 #x1E02F glagolitic) ; Glagolitic Supplement
    (#x1E030 #x1E08F cyrillic) ; Cyrillic Extended-D
    (#x1E100 #x1E14F nyiakeng-puachue-hmong)
    (#x1E290 #x1E2BF toto)
    (#x1E2C0 #x1E2FF wancho)
    (#x1E4D0 #x1E4FF nag-mundari)
    (#x1E7E0 #x1E7FF ethiopic) ; Ethiopic Extended-B
    (#x1E800 #x1E8DF mende-kikakui)
    (#x1E900 #x1E95F adlam)
    (#x1EC70 #x1ECBF indic-siyaq-number)
    (#x1ED00 #x1ED4F ottoman-siyaq-number)
    (#x1EE00 #x1EEFF arabic) ; Arabic Mathematical Alphabetic Symbols
    (#x1F000 #x1F02F mahjong-tile)
    (#x1F030 #x1F09F domino-tile)
    (#x1F0A0 #x1F0FF playing-cards)
    (#x1F100 #x1F1FF symbol) ; Enclosed Alphanumeric Supplement
    (#x1F200 #x1F2FF han) ; Enclosed Ideographic Supplement
    (#x1F300 #x1F9FF symbol) ; Miscellaneous Symbols and Pictographs, Emoticons, Ornamental Dingbats, Transport and Map Symbols, Alchemical Symbols, Geometric Shapes Extended, Supplemental Arrows-C, Supplemental Symbols and Pictographs
    (#x1FA00 #x1FA6F chess-symbol)
    (#x1FA70 #x1FBFF symbol) ; Symbols and Pictographs Extended-A, Symbols for Legacy Computing
    (#x20000 #x2A6DF han) ; CJK Unified Ideographs Extension B
    (#x2A700 #x2EE5F han) ; CJK Unified Ideographs Extension C, CJK Unified Ideographs Extension D, CJK Unified Ideographs Extension E, CJK Unified Ideographs Extension F, CJK Unified Ideographs Extension I
    (#x2F800 #x2FA1F han) ; CJK Compatibility Ideographs Supplement
    (#x30000 #x323AF han) ; CJK Unified Ideographs Extension G, CJK Unified Ideographs Extension H
    (#x231A #x231B emoji) ; Autogenerated emoji
    (#x23E9 #x23EC emoji) ; Autogenerated emoji
    (#x23F0 #x23F0 emoji) ; Autogenerated emoji
    (#x23F3 #x23F3 emoji) ; Autogenerated emoji
    (#x25FD #x25FE emoji) ; Autogenerated emoji
    (#x2614 #x2615 emoji) ; Autogenerated emoji
    (#x2648 #x2653 emoji) ; Autogenerated emoji
    (#x267F #x267F emoji) ; Autogenerated emoji
    (#x2693 #x2693 emoji) ; Autogenerated emoji
    (#x26A1 #x26A1 emoji) ; Autogenerated emoji
    (#x26AA #x26AB emoji) ; Autogenerated emoji
    (#x26BD #x26BE emoji) ; Autogenerated emoji
    (#x26C4 #x26C5 emoji) ; Autogenerated emoji
    (#x26CE #x26CE emoji) ; Autogenerated emoji
    (#x26D4 #x26D4 emoji) ; Autogenerated emoji
    (#x26EA #x26EA emoji) ; Autogenerated emoji
    (#x26F2 #x26F3 emoji) ; Autogenerated emoji
    (#x26F5 #x26F5 emoji) ; Autogenerated emoji
    (#x26FA #x26FA emoji) ; Autogenerated emoji
    (#x26FD #x26FD emoji) ; Autogenerated emoji
    (#x2705 #x2705 emoji) ; Autogenerated emoji
    (#x270A #x270B emoji) ; Autogenerated emoji
    (#x2728 #x2728 emoji) ; Autogenerated emoji
    (#x274C #x274C emoji) ; Autogenerated emoji
    (#x274E #x274E emoji) ; Autogenerated emoji
    (#x2753 #x2755 emoji) ; Autogenerated emoji
    (#x2757 #x2757 emoji) ; Autogenerated emoji
    (#x2795 #x2797 emoji) ; Autogenerated emoji
    (#x27B0 #x27B0 emoji) ; Autogenerated emoji
    (#x27BF #x27BF emoji) ; Autogenerated emoji
    (#x2B1B #x2B1C emoji) ; Autogenerated emoji
    (#x2B50 #x2B50 emoji) ; Autogenerated emoji
    (#x2B55 #x2B55 emoji) ; Autogenerated emoji
    (#x1F004 #x1F004 emoji) ; Autogenerated emoji
    (#x1F0CF #x1F0CF emoji) ; Autogenerated emoji
    (#x1F18E #x1F18E emoji) ; Autogenerated emoji
    (#x1F191 #x1F19A emoji) ; Autogenerated emoji
    (#x1F1E6 #x1F1FF emoji) ; Autogenerated emoji
    (#x1F201 #x1F201 emoji) ; Autogenerated emoji
    (#x1F21A #x1F21A emoji) ; Autogenerated emoji
    (#x1F22F #x1F22F emoji) ; Autogenerated emoji
    (#x1F232 #x1F236 emoji) ; Autogenerated emoji
    (#x1F238 #x1F23A emoji) ; Autogenerated emoji
    (#x1F250 #x1F251 emoji) ; Autogenerated emoji
    (#x1F300 #x1F30C emoji) ; Autogenerated emoji
    (#x1F30D #x1F30E emoji) ; Autogenerated emoji
    (#x1F30F #x1F30F emoji) ; Autogenerated emoji
    (#x1F310 #x1F310 emoji) ; Autogenerated emoji
    (#x1F311 #x1F311 emoji) ; Autogenerated emoji
    (#x1F312 #x1F312 emoji) ; Autogenerated emoji
    (#x1F313 #x1F315 emoji) ; Autogenerated emoji
    (#x1F316 #x1F318 emoji) ; Autogenerated emoji
    (#x1F319 #x1F319 emoji) ; Autogenerated emoji
    (#x1F31A #x1F31A emoji) ; Autogenerated emoji
    (#x1F31B #x1F31B emoji) ; Autogenerated emoji
    (#x1F31C #x1F31C emoji) ; Autogenerated emoji
    (#x1F31D #x1F31E emoji) ; Autogenerated emoji
    (#x1F31F #x1F320 emoji) ; Autogenerated emoji
    (#x1F32D #x1F32F emoji) ; Autogenerated emoji
    (#x1F330 #x1F331 emoji) ; Autogenerated emoji
    (#x1F332 #x1F333 emoji) ; Autogenerated emoji
    (#x1F334 #x1F335 emoji) ; Autogenerated emoji
    (#x1F337 #x1F34A emoji) ; Autogenerated emoji
    (#x1F34B #x1F34B emoji) ; Autogenerated emoji
    (#x1F34C #x1F34F emoji) ; Autogenerated emoji
    (#x1F350 #x1F350 emoji) ; Autogenerated emoji
    (#x1F351 #x1F37B emoji) ; Autogenerated emoji
    (#x1F37C #x1F37C emoji) ; Autogenerated emoji
    (#x1F37E #x1F37F emoji) ; Autogenerated emoji
    (#x1F380 #x1F393 emoji) ; Autogenerated emoji
    (#x1F3A0 #x1F3C4 emoji) ; Autogenerated emoji
    (#x1F3C5 #x1F3C5 emoji) ; Autogenerated emoji
    (#x1F3C6 #x1F3C6 emoji) ; Autogenerated emoji
    (#x1F3C7 #x1F3C7 emoji) ; Autogenerated emoji
    (#x1F3C8 #x1F3C8 emoji) ; Autogenerated emoji
    (#x1F3C9 #x1F3C9 emoji) ; Autogenerated emoji
    (#x1F3CA #x1F3CA emoji) ; Autogenerated emoji
    (#x1F3CF #x1F3D3 emoji) ; Autogenerated emoji
    (#x1F3E0 #x1F3E3 emoji) ; Autogenerated emoji
    (#x1F3E4 #x1F3E4 emoji) ; Autogenerated emoji
    (#x1F3E5 #x1F3F0 emoji) ; Autogenerated emoji
    (#x1F3F4 #x1F3F4 emoji) ; Autogenerated emoji
    (#x1F3F8 #x1F407 emoji) ; Autogenerated emoji
    (#x1F408 #x1F408 emoji) ; Autogenerated emoji
    (#x1F409 #x1F40B emoji) ; Autogenerated emoji
    (#x1F40C #x1F40E emoji) ; Autogenerated emoji
    (#x1F40F #x1F410 emoji) ; Autogenerated emoji
    (#x1F411 #x1F412 emoji) ; Autogenerated emoji
    (#x1F413 #x1F413 emoji) ; Autogenerated emoji
    (#x1F414 #x1F414 emoji) ; Autogenerated emoji
    (#x1F415 #x1F415 emoji) ; Autogenerated emoji
    (#x1F416 #x1F416 emoji) ; Autogenerated emoji
    (#x1F417 #x1F429 emoji) ; Autogenerated emoji
    (#x1F42A #x1F42A emoji) ; Autogenerated emoji
    (#x1F42B #x1F43E emoji) ; Autogenerated emoji
    (#x1F440 #x1F440 emoji) ; Autogenerated emoji
    (#x1F442 #x1F464 emoji) ; Autogenerated emoji
    (#x1F465 #x1F465 emoji) ; Autogenerated emoji
    (#x1F466 #x1F46B emoji) ; Autogenerated emoji
    (#x1F46C #x1F46D emoji) ; Autogenerated emoji
    (#x1F46E #x1F4AC emoji) ; Autogenerated emoji
    (#x1F4AD #x1F4AD emoji) ; Autogenerated emoji
    (#x1F4AE #x1F4B5 emoji) ; Autogenerated emoji
    (#x1F4B6 #x1F4B7 emoji) ; Autogenerated emoji
    (#x1F4B8 #x1F4EB emoji) ; Autogenerated emoji
    (#x1F4EC #x1F4ED emoji) ; Autogenerated emoji
    (#x1F4EE #x1F4EE emoji) ; Autogenerated emoji
    (#x1F4EF #x1F4EF emoji) ; Autogenerated emoji
    (#x1F4F0 #x1F4F4 emoji) ; Autogenerated emoji
    (#x1F4F5 #x1F4F5 emoji) ; Autogenerated emoji
    (#x1F4F6 #x1F4F7 emoji) ; Autogenerated emoji
    (#x1F4F8 #x1F4F8 emoji) ; Autogenerated emoji
    (#x1F4F9 #x1F4FC emoji) ; Autogenerated emoji
    (#x1F4FF #x1F502 emoji) ; Autogenerated emoji
    (#x1F503 #x1F503 emoji) ; Autogenerated emoji
    (#x1F504 #x1F507 emoji) ; Autogenerated emoji
    (#x1F508 #x1F508 emoji) ; Autogenerated emoji
    (#x1F509 #x1F509 emoji) ; Autogenerated emoji
    (#x1F50A #x1F514 emoji) ; Autogenerated emoji
    (#x1F515 #x1F515 emoji) ; Autogenerated emoji
    (#x1F516 #x1F52B emoji) ; Autogenerated emoji
    (#x1F52C #x1F52D emoji) ; Autogenerated emoji
    (#x1F52E #x1F53D emoji) ; Autogenerated emoji
    (#x1F54B #x1F54E emoji) ; Autogenerated emoji
    (#x1F550 #x1F55B emoji) ; Autogenerated emoji
    (#x1F55C #x1F567 emoji) ; Autogenerated emoji
    (#x1F57A #x1F57A emoji) ; Autogenerated emoji
    (#x1F595 #x1F596 emoji) ; Autogenerated emoji
    (#x1F5A4 #x1F5A4 emoji) ; Autogenerated emoji
    (#x1F5FB #x1F5FF emoji) ; Autogenerated emoji
    (#x1F600 #x1F600 emoji) ; Autogenerated emoji
    (#x1F601 #x1F606 emoji) ; Autogenerated emoji
    (#x1F607 #x1F608 emoji) ; Autogenerated emoji
    (#x1F609 #x1F60D emoji) ; Autogenerated emoji
    (#x1F60E #x1F60E emoji) ; Autogenerated emoji
    (#x1F60F #x1F60F emoji) ; Autogenerated emoji
    (#x1F610 #x1F610 emoji) ; Autogenerated emoji
    (#x1F611 #x1F611 emoji) ; Autogenerated emoji
    (#x1F612 #x1F614 emoji) ; Autogenerated emoji
    (#x1F615 #x1F615 emoji) ; Autogenerated emoji
    (#x1F616 #x1F616 emoji) ; Autogenerated emoji
    (#x1F617 #x1F617 emoji) ; Autogenerated emoji
    (#x1F618 #x1F618 emoji) ; Autogenerated emoji
    (#x1F619 #x1F619 emoji) ; Autogenerated emoji
    (#x1F61A #x1F61A emoji) ; Autogenerated emoji
    (#x1F61B #x1F61B emoji) ; Autogenerated emoji
    (#x1F61C #x1F61E emoji) ; Autogenerated emoji
    (#x1F61F #x1F61F emoji) ; Autogenerated emoji
    (#x1F620 #x1F625 emoji) ; Autogenerated emoji
    (#x1F626 #x1F627 emoji) ; Autogenerated emoji
    (#x1F628 #x1F62B emoji) ; Autogenerated emoji
    (#x1F62C #x1F62C emoji) ; Autogenerated emoji
    (#x1F62D #x1F62D emoji) ; Autogenerated emoji
    (#x1F62E #x1F62F emoji) ; Autogenerated emoji
    (#x1F630 #x1F633 emoji) ; Autogenerated emoji
    (#x1F634 #x1F634 emoji) ; Autogenerated emoji
    (#x1F635 #x1F635 emoji) ; Autogenerated emoji
    (#x1F636 #x1F636 emoji) ; Autogenerated emoji
    (#x1F637 #x1F640 emoji) ; Autogenerated emoji
    (#x1F641 #x1F644 emoji) ; Autogenerated emoji
    (#x1F645 #x1F64F emoji) ; Autogenerated emoji
    (#x1F680 #x1F680 emoji) ; Autogenerated emoji
    (#x1F681 #x1F682 emoji) ; Autogenerated emoji
    (#x1F683 #x1F685 emoji) ; Autogenerated emoji
    (#x1F686 #x1F686 emoji) ; Autogenerated emoji
    (#x1F687 #x1F687 emoji) ; Autogenerated emoji
    (#x1F688 #x1F688 emoji) ; Autogenerated emoji
    (#x1F689 #x1F689 emoji) ; Autogenerated emoji
    (#x1F68A #x1F68B emoji) ; Autogenerated emoji
    (#x1F68C #x1F68C emoji) ; Autogenerated emoji
    (#x1F68D #x1F68D emoji) ; Autogenerated emoji
    (#x1F68E #x1F68E emoji) ; Autogenerated emoji
    (#x1F68F #x1F68F emoji) ; Autogenerated emoji
    (#x1F690 #x1F690 emoji) ; Autogenerated emoji
    (#x1F691 #x1F693 emoji) ; Autogenerated emoji
    (#x1F694 #x1F694 emoji) ; Autogenerated emoji
    (#x1F695 #x1F695 emoji) ; Autogenerated emoji
    (#x1F696 #x1F696 emoji) ; Autogenerated emoji
    (#x1F697 #x1F697 emoji) ; Autogenerated emoji
    (#x1F698 #x1F698 emoji) ; Autogenerated emoji
    (#x1F699 #x1F69A emoji) ; Autogenerated emoji
    (#x1F69B #x1F6A1 emoji) ; Autogenerated emoji
    (#x1F6A2 #x1F6A2 emoji) ; Autogenerated emoji
    (#x1F6A3 #x1F6A3 emoji) ; Autogenerated emoji
    (#x1F6A4 #x1F6A5 emoji) ; Autogenerated emoji
    (#x1F6A6 #x1F6A6 emoji) ; Autogenerated emoji
    (#x1F6A7 #x1F6AD emoji) ; Autogenerated emoji
    (#x1F6AE #x1F6B1 emoji) ; Autogenerated emoji
    (#x1F6B2 #x1F6B2 emoji) ; Autogenerated emoji
    (#x1F6B3 #x1F6B5 emoji) ; Autogenerated emoji
    (#x1F6B6 #x1F6B6 emoji) ; Autogenerated emoji
    (#x1F6B7 #x1F6B8 emoji) ; Autogenerated emoji
    (#x1F6B9 #x1F6BE emoji) ; Autogenerated emoji
    (#x1F6BF #x1F6BF emoji) ; Autogenerated emoji
    (#x1F6C0 #x1F6C0 emoji) ; Autogenerated emoji
    (#x1F6C1 #x1F6C5 emoji) ; Autogenerated emoji
    (#x1F6CC #x1F6CC emoji) ; Autogenerated emoji
    (#x1F6D0 #x1F6D0 emoji) ; Autogenerated emoji
    (#x1F6D1 #x1F6D2 emoji) ; Autogenerated emoji
    (#x1F6D5 #x1F6D5 emoji) ; Autogenerated emoji
    (#x1F6D6 #x1F6D7 emoji) ; Autogenerated emoji
    (#x1F6DC #x1F6DC emoji) ; Autogenerated emoji
    (#x1F6DD #x1F6DF emoji) ; Autogenerated emoji
    (#x1F6EB #x1F6EC emoji) ; Autogenerated emoji
    (#x1F6F4 #x1F6F6 emoji) ; Autogenerated emoji
    (#x1F6F7 #x1F6F8 emoji) ; Autogenerated emoji
    (#x1F6F9 #x1F6F9 emoji) ; Autogenerated emoji
    (#x1F6FA #x1F6FA emoji) ; Autogenerated emoji
    (#x1F6FB #x1F6FC emoji) ; Autogenerated emoji
    (#x1F7E0 #x1F7EB emoji) ; Autogenerated emoji
    (#x1F7F0 #x1F7F0 emoji) ; Autogenerated emoji
    (#x1F90C #x1F90C emoji) ; Autogenerated emoji
    (#x1F90D #x1F90F emoji) ; Autogenerated emoji
    (#x1F910 #x1F918 emoji) ; Autogenerated emoji
    (#x1F919 #x1F91E emoji) ; Autogenerated emoji
    (#x1F91F #x1F91F emoji) ; Autogenerated emoji
    (#x1F920 #x1F927 emoji) ; Autogenerated emoji
    (#x1F928 #x1F92F emoji) ; Autogenerated emoji
    (#x1F930 #x1F930 emoji) ; Autogenerated emoji
    (#x1F931 #x1F932 emoji) ; Autogenerated emoji
    (#x1F933 #x1F93A emoji) ; Autogenerated emoji
    (#x1F93C #x1F93E emoji) ; Autogenerated emoji
    (#x1F93F #x1F93F emoji) ; Autogenerated emoji
    (#x1F940 #x1F945 emoji) ; Autogenerated emoji
    (#x1F947 #x1F94B emoji) ; Autogenerated emoji
    (#x1F94C #x1F94C emoji) ; Autogenerated emoji
    (#x1F94D #x1F94F emoji) ; Autogenerated emoji
    (#x1F950 #x1F95E emoji) ; Autogenerated emoji
    (#x1F95F #x1F96B emoji) ; Autogenerated emoji
    (#x1F96C #x1F970 emoji) ; Autogenerated emoji
    (#x1F971 #x1F971 emoji) ; Autogenerated emoji
    (#x1F972 #x1F972 emoji) ; Autogenerated emoji
    (#x1F973 #x1F976 emoji) ; Autogenerated emoji
    (#x1F977 #x1F978 emoji) ; Autogenerated emoji
    (#x1F979 #x1F979 emoji) ; Autogenerated emoji
    (#x1F97A #x1F97A emoji) ; Autogenerated emoji
    (#x1F97B #x1F97B emoji) ; Autogenerated emoji
    (#x1F97C #x1F97F emoji) ; Autogenerated emoji
    (#x1F980 #x1F984 emoji) ; Autogenerated emoji
    (#x1F985 #x1F991 emoji) ; Autogenerated emoji
    (#x1F992 #x1F997 emoji) ; Autogenerated emoji
    (#x1F998 #x1F9A2 emoji) ; Autogenerated emoji
    (#x1F9A3 #x1F9A4 emoji) ; Autogenerated emoji
    (#x1F9A5 #x1F9AA emoji) ; Autogenerated emoji
    (#x1F9AB #x1F9AD emoji) ; Autogenerated emoji
    (#x1F9AE #x1F9AF emoji) ; Autogenerated emoji
    (#x1F9B0 #x1F9B9 emoji) ; Autogenerated emoji
    (#x1F9BA #x1F9BF emoji) ; Autogenerated emoji
    (#x1F9C0 #x1F9C0 emoji) ; Autogenerated emoji
    (#x1F9C1 #x1F9C2 emoji) ; Autogenerated emoji
    (#x1F9C3 #x1F9CA emoji) ; Autogenerated emoji
    (#x1F9CB #x1F9CB emoji) ; Autogenerated emoji
    (#x1F9CC #x1F9CC emoji) ; Autogenerated emoji
    (#x1F9CD #x1F9CF emoji) ; Autogenerated emoji
    (#x1F9D0 #x1F9E6 emoji) ; Autogenerated emoji
    (#x1F9E7 #x1F9FF emoji) ; Autogenerated emoji
    (#x1FA70 #x1FA73 emoji) ; Autogenerated emoji
    (#x1FA74 #x1FA74 emoji) ; Autogenerated emoji
    (#x1FA75 #x1FA77 emoji) ; Autogenerated emoji
    (#x1FA78 #x1FA7A emoji) ; Autogenerated emoji
    (#x1FA7B #x1FA7C emoji) ; Autogenerated emoji
    (#x1FA80 #x1FA82 emoji) ; Autogenerated emoji
    (#x1FA83 #x1FA86 emoji) ; Autogenerated emoji
    (#x1FA87 #x1FA88 emoji) ; Autogenerated emoji
    (#x1FA90 #x1FA95 emoji) ; Autogenerated emoji
    (#x1FA96 #x1FAA8 emoji) ; Autogenerated emoji
    (#x1FAA9 #x1FAAC emoji) ; Autogenerated emoji
    (#x1FAAD #x1FAAF emoji) ; Autogenerated emoji
    (#x1FAB0 #x1FAB6 emoji) ; Autogenerated emoji
    (#x1FAB7 #x1FABA emoji) ; Autogenerated emoji
    (#x1FABB #x1FABD emoji) ; Autogenerated emoji
    (#x1FABF #x1FABF emoji) ; Autogenerated emoji
    (#x1FAC0 #x1FAC2 emoji) ; Autogenerated emoji
    (#x1FAC3 #x1FAC5 emoji) ; Autogenerated emoji
    (#x1FACE #x1FACF emoji) ; Autogenerated emoji
    (#x1FAD0 #x1FAD6 emoji) ; Autogenerated emoji
    (#x1FAD7 #x1FAD9 emoji) ; Autogenerated emoji
    (#x1FADA #x1FADB emoji) ; Autogenerated emoji
    (#x1FAE0 #x1FAE7 emoji) ; Autogenerated emoji
    (#x1FAE8 #x1FAE8 emoji) ; Autogenerated emoji
    (#x1FAF0 #x1FAF6 emoji) ; Autogenerated emoji
    (#x1FAF7 #x1FAF8 emoji) ; Autogenerated emoji
    (#xFE0F #xFE0F emoji) ; Autogenerated emoji (override)
    ))
    (set-char-table-range char-script-table
			  (cons (car elt) (nth 1 elt)) (nth 2 elt))
    (or (memq (nth 2 elt) script-list)
	(setq script-list (cons (nth 2 elt) script-list))))
  (set-char-table-extra-slot char-script-table 0 (nreverse script-list)))


(provide 'charscript)

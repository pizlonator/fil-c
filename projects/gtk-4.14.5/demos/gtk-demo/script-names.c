#include "config.h"
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <hb-ot.h>

#include "script-names.h"

static struct {
  GUnicodeScript script;
  hb_script_t hb_script;
  const char *name;
} scripts[] =
{
  { G_UNICODE_SCRIPT_COMMON, HB_SCRIPT_COMMON,  NULL },
  { G_UNICODE_SCRIPT_INHERITED, HB_SCRIPT_INHERITED,  NULL },
  { G_UNICODE_SCRIPT_ARABIC, HB_SCRIPT_ARABIC,  NC_("Script", "Arabic") },
  { G_UNICODE_SCRIPT_ARMENIAN, HB_SCRIPT_ARMENIAN,  NC_("Script", "Armenian") },
  { G_UNICODE_SCRIPT_BENGALI, HB_SCRIPT_BENGALI,  NC_("Script", "Bengali") },
  { G_UNICODE_SCRIPT_BOPOMOFO, HB_SCRIPT_BOPOMOFO,  NC_("Script", "Bopomofo") },
  { G_UNICODE_SCRIPT_CHEROKEE, HB_SCRIPT_CHEROKEE,  NC_("Script", "Cherokee") },
  { G_UNICODE_SCRIPT_COPTIC, HB_SCRIPT_COPTIC,  NC_("Script", "Coptic") },
  { G_UNICODE_SCRIPT_CYRILLIC, HB_SCRIPT_CYRILLIC,  NC_("Script", "Cyrillic") },
  { G_UNICODE_SCRIPT_DESERET, HB_SCRIPT_DESERET,  NC_("Script", "Deseret") },
  { G_UNICODE_SCRIPT_DEVANAGARI, HB_SCRIPT_DEVANAGARI,  NC_("Script", "Devanagari") },
  { G_UNICODE_SCRIPT_ETHIOPIC, HB_SCRIPT_ETHIOPIC,  NC_("Script", "Ethiopic") },
  { G_UNICODE_SCRIPT_GEORGIAN, HB_SCRIPT_GEORGIAN,  NC_("Script", "Georgian") },
  { G_UNICODE_SCRIPT_GOTHIC, HB_SCRIPT_GOTHIC,  NC_("Script", "Gothic") },
  { G_UNICODE_SCRIPT_GREEK, HB_SCRIPT_GREEK,  NC_("Script", "Greek") },
  { G_UNICODE_SCRIPT_GUJARATI, HB_SCRIPT_GUJARATI,  NC_("Script", "Gujarati") },
  { G_UNICODE_SCRIPT_GURMUKHI, HB_SCRIPT_GURMUKHI,  NC_("Script", "Gurmukhi") },
  { G_UNICODE_SCRIPT_HAN, HB_SCRIPT_HAN,  NC_("Script", "Han") },
  { G_UNICODE_SCRIPT_HANGUL, HB_SCRIPT_HANGUL,  NC_("Script", "Hangul") },
  { G_UNICODE_SCRIPT_HEBREW, HB_SCRIPT_HEBREW,  NC_("Script", "Hebrew") },
  { G_UNICODE_SCRIPT_HIRAGANA, HB_SCRIPT_HIRAGANA,  NC_("Script", "Hiragana") },
  { G_UNICODE_SCRIPT_KANNADA, HB_SCRIPT_KANNADA,  NC_("Script", "Kannada") },
  { G_UNICODE_SCRIPT_KATAKANA, HB_SCRIPT_KATAKANA,  NC_("Script", "Katakana") },
  { G_UNICODE_SCRIPT_KHMER, HB_SCRIPT_KHMER,  NC_("Script", "Khmer") },
  { G_UNICODE_SCRIPT_LAO, HB_SCRIPT_LAO,  NC_("Script", "Lao") },
  { G_UNICODE_SCRIPT_LATIN, HB_SCRIPT_LATIN,  NC_("Script", "Latin") },
  { G_UNICODE_SCRIPT_MALAYALAM, HB_SCRIPT_MALAYALAM,  NC_("Script", "Malayalam") },
  { G_UNICODE_SCRIPT_MONGOLIAN, HB_SCRIPT_MONGOLIAN,  NC_("Script", "Mongolian") },
  { G_UNICODE_SCRIPT_MYANMAR, HB_SCRIPT_MYANMAR,  NC_("Script", "Myanmar") },
  { G_UNICODE_SCRIPT_OGHAM, HB_SCRIPT_OGHAM,  NC_("Script", "Ogham") },
  { G_UNICODE_SCRIPT_OLD_ITALIC, HB_SCRIPT_OLD_ITALIC,  NC_("Script", "Old Italic") },
  { G_UNICODE_SCRIPT_ORIYA, HB_SCRIPT_ORIYA,  NC_("Script", "Oriya") },
  { G_UNICODE_SCRIPT_RUNIC, HB_SCRIPT_RUNIC,  NC_("Script", "Runic") },
  { G_UNICODE_SCRIPT_SINHALA, HB_SCRIPT_SINHALA,  NC_("Script", "Sinhala") },
  { G_UNICODE_SCRIPT_SYRIAC, HB_SCRIPT_SYRIAC,  NC_("Script", "Syriac") },
  { G_UNICODE_SCRIPT_TAMIL, HB_SCRIPT_TAMIL,  NC_("Script", "Tamil") },
  { G_UNICODE_SCRIPT_TELUGU, HB_SCRIPT_TELUGU,  NC_("Script", "Telugu") },
  { G_UNICODE_SCRIPT_THAANA, HB_SCRIPT_THAANA,  NC_("Script", "Thaana") },
  { G_UNICODE_SCRIPT_THAI, HB_SCRIPT_THAI,  NC_("Script", "Thai") },
  { G_UNICODE_SCRIPT_TIBETAN, HB_SCRIPT_TIBETAN,  NC_("Script", "Tibetan") },
  { G_UNICODE_SCRIPT_CANADIAN_ABORIGINAL, HB_SCRIPT_CANADIAN_ABORIGINAL,  NC_("Script", "Canadian Aboriginal") },
  { G_UNICODE_SCRIPT_YI, HB_SCRIPT_YI,  NC_("Script", "Yi") },
  { G_UNICODE_SCRIPT_TAGALOG, HB_SCRIPT_TAGALOG,  NC_("Script", "Tagalog") },
  { G_UNICODE_SCRIPT_HANUNOO, HB_SCRIPT_HANUNOO,  NC_("Script", "Hanunoo") },
  { G_UNICODE_SCRIPT_BUHID, HB_SCRIPT_BUHID,  NC_("Script", "Buhid") },
  { G_UNICODE_SCRIPT_TAGBANWA, HB_SCRIPT_TAGBANWA,  NC_("Script", "Tagbanwa") },
  { G_UNICODE_SCRIPT_BRAILLE, HB_SCRIPT_BRAILLE,  NC_("Script", "Braille") },
  { G_UNICODE_SCRIPT_CYPRIOT, HB_SCRIPT_CYPRIOT,  NC_("Script", "Cypriot") },
  { G_UNICODE_SCRIPT_LIMBU, HB_SCRIPT_LIMBU,  NC_("Script", "Limbu") },
  { G_UNICODE_SCRIPT_OSMANYA, HB_SCRIPT_OSMANYA,  NC_("Script", "Osmanya") },
  { G_UNICODE_SCRIPT_SHAVIAN, HB_SCRIPT_SHAVIAN,  NC_("Script", "Shavian") },
  { G_UNICODE_SCRIPT_LINEAR_B, HB_SCRIPT_LINEAR_B,  NC_("Script", "Linear B") },
  { G_UNICODE_SCRIPT_TAI_LE, HB_SCRIPT_TAI_LE,  NC_("Script", "Tai Le") },
  { G_UNICODE_SCRIPT_UGARITIC, HB_SCRIPT_UGARITIC,  NC_("Script", "Ugaritic") },
  { G_UNICODE_SCRIPT_NEW_TAI_LUE, HB_SCRIPT_NEW_TAI_LUE,  NC_("Script", "New Tai Lue") },
  { G_UNICODE_SCRIPT_BUGINESE, HB_SCRIPT_BUGINESE,  NC_("Script", "Buginese") },
  { G_UNICODE_SCRIPT_GLAGOLITIC, HB_SCRIPT_GLAGOLITIC,  NC_("Script", "Glagolitic") },
  { G_UNICODE_SCRIPT_TIFINAGH, HB_SCRIPT_TIFINAGH,  NC_("Script", "Tifinagh") },
  { G_UNICODE_SCRIPT_SYLOTI_NAGRI, HB_SCRIPT_SYLOTI_NAGRI,  NC_("Script", "Syloti Nagri") },
  { G_UNICODE_SCRIPT_OLD_PERSIAN, HB_SCRIPT_OLD_PERSIAN,  NC_("Script", "Old Persian") },
  { G_UNICODE_SCRIPT_KHAROSHTHI, HB_SCRIPT_KHAROSHTHI,  NC_("Script", "Kharoshthi") },
  { G_UNICODE_SCRIPT_UNKNOWN, HB_SCRIPT_UNKNOWN,  NC_("Script", "Unknown") },
  { G_UNICODE_SCRIPT_BALINESE, HB_SCRIPT_BALINESE,  NC_("Script", "Balinese") },
  { G_UNICODE_SCRIPT_CUNEIFORM, HB_SCRIPT_CUNEIFORM,  NC_("Script", "Cuneiform") },
  { G_UNICODE_SCRIPT_PHOENICIAN, HB_SCRIPT_PHOENICIAN,  NC_("Script", "Phoenician") },
  { G_UNICODE_SCRIPT_PHAGS_PA, HB_SCRIPT_PHAGS_PA,  NC_("Script", "Phags-pa") },
  { G_UNICODE_SCRIPT_NKO, HB_SCRIPT_NKO,  NC_("Script", "N'Ko") },
  { G_UNICODE_SCRIPT_KAYAH_LI, HB_SCRIPT_KAYAH_LI,  NC_("Script", "Kayah Li") },
  { G_UNICODE_SCRIPT_LEPCHA, HB_SCRIPT_LEPCHA,  NC_("Script", "Lepcha") },
  { G_UNICODE_SCRIPT_REJANG, HB_SCRIPT_REJANG,  NC_("Script", "Rejang") },
  { G_UNICODE_SCRIPT_SUNDANESE, HB_SCRIPT_SUNDANESE,  NC_("Script", "Sundanese") },
  { G_UNICODE_SCRIPT_SAURASHTRA, HB_SCRIPT_SAURASHTRA,  NC_("Script", "Saurashtra") },
  { G_UNICODE_SCRIPT_CHAM, HB_SCRIPT_CHAM,  NC_("Script", "Cham") },
  { G_UNICODE_SCRIPT_OL_CHIKI, HB_SCRIPT_OL_CHIKI,  NC_("Script", "Ol Chiki") },
  { G_UNICODE_SCRIPT_VAI, HB_SCRIPT_VAI,  NC_("Script", "Vai") },
  { G_UNICODE_SCRIPT_CARIAN, HB_SCRIPT_CARIAN,  NC_("Script", "Carian") },
  { G_UNICODE_SCRIPT_LYCIAN, HB_SCRIPT_LYCIAN,  NC_("Script", "Lycian") },
  { G_UNICODE_SCRIPT_LYDIAN, HB_SCRIPT_LYDIAN,  NC_("Script", "Lydian") },
  { G_UNICODE_SCRIPT_AVESTAN, HB_SCRIPT_AVESTAN,  NC_("Script", "Avestan") },
  { G_UNICODE_SCRIPT_BAMUM, HB_SCRIPT_BAMUM,  NC_("Script", "Bamum") },
  { G_UNICODE_SCRIPT_EGYPTIAN_HIEROGLYPHS, HB_SCRIPT_EGYPTIAN_HIEROGLYPHS,  NC_("Script", "Egyptian Hieroglyphs") },
  { G_UNICODE_SCRIPT_IMPERIAL_ARAMAIC, HB_SCRIPT_IMPERIAL_ARAMAIC,  NC_("Script", "Imperial Aramaic") },
  { G_UNICODE_SCRIPT_INSCRIPTIONAL_PAHLAVI, HB_SCRIPT_INSCRIPTIONAL_PAHLAVI,  NC_("Script", "Inscriptional Pahlavi") },
  { G_UNICODE_SCRIPT_INSCRIPTIONAL_PARTHIAN, HB_SCRIPT_INSCRIPTIONAL_PARTHIAN,  NC_("Script", "Inscriptional Parthian") },
  { G_UNICODE_SCRIPT_JAVANESE, HB_SCRIPT_JAVANESE,  NC_("Script", "Javanese") },
  { G_UNICODE_SCRIPT_KAITHI, HB_SCRIPT_KAITHI,  NC_("Script", "Kaithi") },
  { G_UNICODE_SCRIPT_LISU, HB_SCRIPT_LISU,  NC_("Script", "Lisu") },
  { G_UNICODE_SCRIPT_MEETEI_MAYEK, HB_SCRIPT_MEETEI_MAYEK,  NC_("Script", "Meetei Mayek") },
  { G_UNICODE_SCRIPT_OLD_SOUTH_ARABIAN, HB_SCRIPT_OLD_SOUTH_ARABIAN,  NC_("Script", "Old South Arabian") },
  { G_UNICODE_SCRIPT_OLD_TURKIC, HB_SCRIPT_OLD_TURKIC,  NC_("Script", "Old Turkic") },
  { G_UNICODE_SCRIPT_SAMARITAN, HB_SCRIPT_SAMARITAN,  NC_("Script", "Samaritan") },
  { G_UNICODE_SCRIPT_TAI_THAM, HB_SCRIPT_TAI_THAM,  NC_("Script", "Tai Tham") },
  { G_UNICODE_SCRIPT_TAI_VIET, HB_SCRIPT_TAI_VIET,  NC_("Script", "Tai Viet") },
  { G_UNICODE_SCRIPT_BATAK, HB_SCRIPT_BATAK,  NC_("Script", "Batak") },
  { G_UNICODE_SCRIPT_BRAHMI, HB_SCRIPT_BRAHMI,  NC_("Script", "Brahmi") },
  { G_UNICODE_SCRIPT_MANDAIC, HB_SCRIPT_MANDAIC,  NC_("Script", "Mandaic") },
  { G_UNICODE_SCRIPT_CHAKMA, HB_SCRIPT_CHAKMA,  NC_("Script", "Chakma") },
  { G_UNICODE_SCRIPT_MEROITIC_CURSIVE, HB_SCRIPT_MEROITIC_CURSIVE,  NC_("Script", "Meroitic Cursive") },
  { G_UNICODE_SCRIPT_MEROITIC_HIEROGLYPHS, HB_SCRIPT_MEROITIC_HIEROGLYPHS,  NC_("Script", "Meroitic Hieroglyphs") },
  { G_UNICODE_SCRIPT_MIAO, HB_SCRIPT_MIAO,  NC_("Script", "Miao") },
  { G_UNICODE_SCRIPT_SHARADA, HB_SCRIPT_SHARADA,  NC_("Script", "Sharada") },
  { G_UNICODE_SCRIPT_SORA_SOMPENG, HB_SCRIPT_SORA_SOMPENG,  NC_("Script", "Sora Sompeng") },
  { G_UNICODE_SCRIPT_TAKRI, HB_SCRIPT_TAKRI,  NC_("Script", "Takri") },
  { G_UNICODE_SCRIPT_BASSA_VAH, HB_SCRIPT_BASSA_VAH,  NC_("Script", "Bassa") },
  { G_UNICODE_SCRIPT_CAUCASIAN_ALBANIAN, HB_SCRIPT_CAUCASIAN_ALBANIAN,  NC_("Script", "Caucasian Albanian") },
  { G_UNICODE_SCRIPT_DUPLOYAN, HB_SCRIPT_DUPLOYAN,  NC_("Script", "Duployan") },
  { G_UNICODE_SCRIPT_ELBASAN, HB_SCRIPT_ELBASAN,  NC_("Script", "Elbasan") },
  { G_UNICODE_SCRIPT_GRANTHA, HB_SCRIPT_GRANTHA,  NC_("Script", "Grantha") },
  { G_UNICODE_SCRIPT_KHOJKI, HB_SCRIPT_KHOJKI,  NC_("Script", "Khojki") },
  { G_UNICODE_SCRIPT_KHUDAWADI, HB_SCRIPT_KHUDAWADI,  NC_("Script", "Khudawadi, Sindhi") },
  { G_UNICODE_SCRIPT_LINEAR_A, HB_SCRIPT_LINEAR_A,  NC_("Script", "Linear A") },
  { G_UNICODE_SCRIPT_MAHAJANI, HB_SCRIPT_MAHAJANI,  NC_("Script", "Mahajani") },
  { G_UNICODE_SCRIPT_MANICHAEAN, HB_SCRIPT_MANICHAEAN,  NC_("Script", "Manichaean") },
  { G_UNICODE_SCRIPT_MENDE_KIKAKUI, HB_SCRIPT_MENDE_KIKAKUI,  NC_("Script", "Mende Kikakui") },
  { G_UNICODE_SCRIPT_MODI, HB_SCRIPT_MODI,  NC_("Script", "Modi") },
  { G_UNICODE_SCRIPT_MRO, HB_SCRIPT_MRO,  NC_("Script", "Mro") },
  { G_UNICODE_SCRIPT_NABATAEAN, HB_SCRIPT_NABATAEAN,  NC_("Script", "Nabataean") },
  { G_UNICODE_SCRIPT_OLD_NORTH_ARABIAN, HB_SCRIPT_OLD_NORTH_ARABIAN,  NC_("Script", "Old North Arabian") },
  { G_UNICODE_SCRIPT_OLD_PERMIC, HB_SCRIPT_OLD_PERMIC,  NC_("Script", "Old Permic") },
  { G_UNICODE_SCRIPT_PAHAWH_HMONG, HB_SCRIPT_PAHAWH_HMONG,  NC_("Script", "Pahawh Hmong") },
  { G_UNICODE_SCRIPT_PALMYRENE, HB_SCRIPT_PALMYRENE,  NC_("Script", "Palmyrene") },
  { G_UNICODE_SCRIPT_PAU_CIN_HAU, HB_SCRIPT_PAU_CIN_HAU,  NC_("Script", "Pau Cin Hau") },
  { G_UNICODE_SCRIPT_PSALTER_PAHLAVI, HB_SCRIPT_PSALTER_PAHLAVI,  NC_("Script", "Psalter Pahlavi") },
  { G_UNICODE_SCRIPT_SIDDHAM, HB_SCRIPT_SIDDHAM,  NC_("Script", "Siddham") },
  { G_UNICODE_SCRIPT_TIRHUTA, HB_SCRIPT_TIRHUTA,  NC_("Script", "Tirhuta") },
  { G_UNICODE_SCRIPT_WARANG_CITI, HB_SCRIPT_WARANG_CITI,  NC_("Script", "Warang Citi") },
  { G_UNICODE_SCRIPT_AHOM, HB_SCRIPT_AHOM,  NC_("Script", "Ahom") },
  { G_UNICODE_SCRIPT_ANATOLIAN_HIEROGLYPHS, HB_SCRIPT_ANATOLIAN_HIEROGLYPHS,  NC_("Script", "Anatolian Hieroglyphs") },
  { G_UNICODE_SCRIPT_HATRAN, HB_SCRIPT_HATRAN,  NC_("Script", "Hatran") },
  { G_UNICODE_SCRIPT_MULTANI, HB_SCRIPT_MULTANI,  NC_("Script", "Multani") },
  { G_UNICODE_SCRIPT_OLD_HUNGARIAN, HB_SCRIPT_OLD_HUNGARIAN,  NC_("Script", "Old Hungarian") },
  { G_UNICODE_SCRIPT_SIGNWRITING, HB_SCRIPT_SIGNWRITING,  NC_("Script", "Signwriting") },
  { G_UNICODE_SCRIPT_ADLAM, HB_SCRIPT_ADLAM,  NC_("Script", "Adlam") },
  { G_UNICODE_SCRIPT_BHAIKSUKI, HB_SCRIPT_BHAIKSUKI,  NC_("Script", "Bhaiksuki") },
  { G_UNICODE_SCRIPT_MARCHEN, HB_SCRIPT_MARCHEN,  NC_("Script", "Marchen") },
  { G_UNICODE_SCRIPT_NEWA, HB_SCRIPT_NEWA,  NC_("Script", "Newa") },
  { G_UNICODE_SCRIPT_OSAGE, HB_SCRIPT_OSAGE,  NC_("Script", "Osage") },
  { G_UNICODE_SCRIPT_TANGUT, HB_SCRIPT_TANGUT,  NC_("Script", "Tangut") },
  { G_UNICODE_SCRIPT_MASARAM_GONDI, HB_SCRIPT_MASARAM_GONDI,  NC_("Script", "Masaram Gondi") },
  { G_UNICODE_SCRIPT_NUSHU, HB_SCRIPT_NUSHU,  NC_("Script", "Nushu") },
  { G_UNICODE_SCRIPT_SOYOMBO, HB_SCRIPT_SOYOMBO,  NC_("Script", "Soyombo") },
  { G_UNICODE_SCRIPT_ZANABAZAR_SQUARE, HB_SCRIPT_ZANABAZAR_SQUARE,  NC_("Script", "Zanabazar Square") },
  { G_UNICODE_SCRIPT_DOGRA, HB_SCRIPT_DOGRA, NC_("Script", "Dogra") },
  { G_UNICODE_SCRIPT_GUNJALA_GONDI, HB_SCRIPT_GUNJALA_GONDI, NC_("Script", "Gunjala Gondi") },
  { G_UNICODE_SCRIPT_HANIFI_ROHINGYA, HB_SCRIPT_HANIFI_ROHINGYA, NC_("Script", "Hanifi Rohingya") },
  { G_UNICODE_SCRIPT_MAKASAR, HB_SCRIPT_MAKASAR, NC_("Script", "Makasar") },
  { G_UNICODE_SCRIPT_MEDEFAIDRIN, HB_SCRIPT_MEDEFAIDRIN, NC_("Script", "Medefaidrin") },
  { G_UNICODE_SCRIPT_OLD_SOGDIAN, HB_SCRIPT_OLD_SOGDIAN, NC_("Script", "Old Sogdian") },
  { G_UNICODE_SCRIPT_SOGDIAN, HB_SCRIPT_SOGDIAN, NC_("Script", "Sogdian") },

#if HB_VERSION_ATLEAST(2,4,0)
  { G_UNICODE_SCRIPT_ELYMAIC, HB_SCRIPT_ELYMAIC, NC_("Script", "Elym") },
  { G_UNICODE_SCRIPT_NANDINAGARI, HB_SCRIPT_NANDINAGARI, NC_("Script", "Nand") },
  { G_UNICODE_SCRIPT_NYIAKENG_PUACHUE_HMONG, HB_SCRIPT_NYIAKENG_PUACHUE_HMONG, NC_("Script", "Rohg") },
  { G_UNICODE_SCRIPT_WANCHO, HB_SCRIPT_WANCHO, NC_("Script", "Wcho") },
#else
  { G_UNICODE_SCRIPT_ELYMAIC, HB_SCRIPT_INVALID, NC_("Script", "Elym") },
  { G_UNICODE_SCRIPT_NANDINAGARI, HB_SCRIPT_INVALID, NC_("Script", "Nand") },
  { G_UNICODE_SCRIPT_NYIAKENG_PUACHUE_HMONG, HB_SCRIPT_INVALID, NC_("Script", "Rohg") },
  { G_UNICODE_SCRIPT_WANCHO, HB_SCRIPT_INVALID, NC_("Script", "Wcho") },
#endif
#if HB_VERSION_ATLEAST(2,6,7)
  { G_UNICODE_SCRIPT_CHORASMIAN, HB_SCRIPT_CHORASMIAN, NC_("Script", "Chorasmian") },
  { G_UNICODE_SCRIPT_DIVES_AKURU, HB_SCRIPT_DIVES_AKURU, NC_("Script", "Dives Akuru") },
  { G_UNICODE_SCRIPT_KHITAN_SMALL_SCRIPT, HB_SCRIPT_KHITAN_SMALL_SCRIPT, NC_("Script", "Khitan small script") },
  { G_UNICODE_SCRIPT_YEZIDI, HB_SCRIPT_YEZIDI, NC_("Script", "Yezidi") },
#else
  { G_UNICODE_SCRIPT_CHORASMIAN, HB_SCRIPT_INVALID, NC_("Script", "Chorasmian") },
  { G_UNICODE_SCRIPT_DIVES_AKURU, HB_SCRIPT_INVALID, NC_("Script", "Dives Akuru") },
  { G_UNICODE_SCRIPT_KHITAN_SMALL_SCRIPT, HB_SCRIPT_INVALID, NC_("Script", "Khitan small script") },
  { G_UNICODE_SCRIPT_YEZIDI, HB_SCRIPT_INVALID, NC_("Script", "Yezidi") },
#endif
};

const char *
get_script_name (GUnicodeScript script)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (scripts); i++)
    {
      if (scripts[i].script == script)
        {
          if (scripts[i].name)
            return g_dpgettext2 (GETTEXT_PACKAGE, "Script", scripts[i].name);
          else
            return "";
        }
    }

  return NULL;
}

const char *
get_script_name_for_tag (guint32 tag)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (scripts); i++)
    {
      if (scripts[i].hb_script == hb_script_from_iso15924_tag (tag))
        return g_dpgettext2 (GETTEXT_PACKAGE, "Script", scripts[i].name);
    }

  return NULL;
}

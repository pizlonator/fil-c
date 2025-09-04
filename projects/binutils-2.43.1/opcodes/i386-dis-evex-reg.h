  /* REG_EVEX_0F71 */
  {
    { Bad_Opcode },
    { Bad_Opcode },
    { "%XEvpsrlw",	{ Vex, EXx, Ib }, PREFIX_DATA },
    { Bad_Opcode },
    { "%XEvpsraw",	{ Vex, EXx, Ib }, PREFIX_DATA },
    { Bad_Opcode },
    { "%XEvpsllw",	{ Vex, EXx, Ib }, PREFIX_DATA },
  },
  /* REG_EVEX_0F72 */
  {
    { "vpror%DQ",	{ Vex, EXx, Ib }, PREFIX_DATA },
    { "vprol%DQ",	{ Vex, EXx, Ib }, PREFIX_DATA },
    { VEX_W_TABLE (EVEX_W_0F72_R_2) },
    { Bad_Opcode },
    { "%XEvpsra%DQ",	{ Vex, EXx, Ib }, PREFIX_DATA },
    { Bad_Opcode },
    { VEX_W_TABLE (EVEX_W_0F72_R_6) },
  },
  /* REG_EVEX_0F73 */
  {
    { Bad_Opcode },
    { Bad_Opcode },
    { VEX_W_TABLE (EVEX_W_0F73_R_2) },
    { "%XEvpsrldqY",	{ Vex, EXx, Ib }, PREFIX_DATA },
    { Bad_Opcode },
    { Bad_Opcode },
    { VEX_W_TABLE (EVEX_W_0F73_R_6) },
    { "%XEvpslldqY",	{ Vex, EXx, Ib }, PREFIX_DATA },
  },
  /* REG_EVEX_0F38C6_L_2 */
  {
    { Bad_Opcode },
    { "vgatherpf0dp%XW",  { MVexVSIBDWpX }, PREFIX_DATA },
    { "vgatherpf1dp%XW",  { MVexVSIBDWpX }, PREFIX_DATA },
    { Bad_Opcode },
    { Bad_Opcode },
    { "vscatterpf0dp%XW",  { MVexVSIBDWpX }, PREFIX_DATA },
    { "vscatterpf1dp%XW",  { MVexVSIBDWpX }, PREFIX_DATA },
  },
  /* REG_EVEX_0F38C7_L_2 */
  {
    { Bad_Opcode },
    { "vgatherpf0qp%XW",  { MVexVSIBQWpX }, PREFIX_DATA },
    { "vgatherpf1qp%XW",  { MVexVSIBQWpX }, PREFIX_DATA },
    { Bad_Opcode },
    { Bad_Opcode },
    { "vscatterpf0qp%XW",  { MVexVSIBQWpX }, PREFIX_DATA },
    { "vscatterpf1qp%XW",  { MVexVSIBQWpX }, PREFIX_DATA },
  },
  /* REG_EVEX_MAP4_80 */
  {
    { "%NFaddA",	{ VexGb, Eb, Ib }, NO_PREFIX },
    { "%NForA",	{ VexGb, Eb, Ib }, NO_PREFIX },
    { "adcA",	{ VexGb, Eb, Ib }, NO_PREFIX },
    { "sbbA",	{ VexGb, Eb, Ib }, NO_PREFIX },
    { "%NFandA",	{ VexGb, Eb, Ib }, NO_PREFIX },
    { "%NFsubA",	{ VexGb, Eb, Ib }, NO_PREFIX },
    { "%NFxorA",	{ VexGb, Eb, Ib }, NO_PREFIX },
    { "%NEccmp%SCA%DF",	{ Eb, Ib }, NO_PREFIX },
  },
  /* REG_EVEX_MAP4_81 */
  {
    { "%NFaddQ",	{ VexGv, Ev, Iv }, PREFIX_NP_OR_DATA },
    { "%NForQ",	{ VexGv, Ev, Iv }, PREFIX_NP_OR_DATA },
    { "adcQ",	{ VexGv, Ev, Iv }, PREFIX_NP_OR_DATA },
    { "sbbQ",	{ VexGv, Ev, Iv }, PREFIX_NP_OR_DATA },
    { "%NFandQ",	{ VexGv, Ev, Iv }, PREFIX_NP_OR_DATA },
    { "%NFsubQ",	{ VexGv, Ev, Iv }, PREFIX_NP_OR_DATA },
    { "%NFxorQ",	{ VexGv, Ev, Iv }, PREFIX_NP_OR_DATA },
    { "%NEccmp%SCQ%DF",	{ Ev, Iv }, PREFIX_NP_OR_DATA },
  },
  /* REG_EVEX_MAP4_83 */
  {
    { "%NFaddQ",	{ VexGv, Ev, sIb }, PREFIX_NP_OR_DATA },
    { "%NForQ",	{ VexGv, Ev, sIb }, PREFIX_NP_OR_DATA },
    { "adcQ",	{ VexGv, Ev, sIb }, PREFIX_NP_OR_DATA },
    { "sbbQ",	{ VexGv, Ev, sIb }, PREFIX_NP_OR_DATA },
    { "%NFandQ",	{ VexGv, Ev, sIb }, PREFIX_NP_OR_DATA },
    { "%NFsubQ",	{ VexGv, Ev, sIb }, PREFIX_NP_OR_DATA },
    { "%NFxorQ",	{ VexGv, Ev, sIb }, PREFIX_NP_OR_DATA },
    { "%NEccmp%SCQ%DF",	{ Ev, sIb }, PREFIX_NP_OR_DATA },
  },
  /* REG_EVEX_MAP4_8F */
  {
    { VEX_W_TABLE (EVEX_W_MAP4_8F_R_0) },
  },
  /* REG_EVEX_MAP4_F6 */
  {
    { "%NEctest%SCA%DF",  { Eb, Ib }, NO_PREFIX },
    { "%NEctest%SCA%DF",  { Eb, Ib }, NO_PREFIX },
    { "notA",	{ VexGb, Eb }, NO_PREFIX },
    { "%NFnegA",	{ VexGb, Eb }, NO_PREFIX },
    { "%NFmulA",	{ Eb }, NO_PREFIX },
    { "%NFimulA",	{ Eb }, NO_PREFIX },
    { "%NFdivA",	{ Eb }, NO_PREFIX },
    { "%NFidivA",	{ Eb }, NO_PREFIX },
  },
  /* REG_EVEX_MAP4_F7 */
  {
    { "%NEctest%SCQ%DF",  { Ev, Iv }, PREFIX_NP_OR_DATA },
    { "%NEctest%SCQ%DF",  { Ev, Iv }, PREFIX_NP_OR_DATA },
    { "notQ",	{ VexGv, Ev }, PREFIX_NP_OR_DATA },
    { "%NFnegQ",	{ VexGv, Ev }, PREFIX_NP_OR_DATA },
    { "%NFmulQ",	{ Ev }, PREFIX_NP_OR_DATA },
    { "%NFimulQ",	{ Ev }, PREFIX_NP_OR_DATA },
    { "%NFdivQ",	{ Ev }, PREFIX_NP_OR_DATA },
    { "%NFidivQ",	{ Ev }, PREFIX_NP_OR_DATA },
  },
  /* REG_EVEX_MAP4_FE */
  {
    { "%NFincA",	{ VexGb, Eb }, NO_PREFIX },
    { "%NFdecA",	{ VexGb, Eb }, NO_PREFIX },
  },
  /* REG_EVEX_MAP4_FF */
  {
    { "%NFincQ",	{ VexGv, Ev }, PREFIX_NP_OR_DATA },
    { "%NFdecQ",	{ VexGv, Ev }, PREFIX_NP_OR_DATA },
    { Bad_Opcode },
    { Bad_Opcode },
    { Bad_Opcode },
    { Bad_Opcode },
    { VEX_W_TABLE (EVEX_W_MAP4_FF_R_6) },
  },

  /* MOD_EVEX_MAP4_60 */
  {
    { "movbeS",		{ Gv, Mv }, PREFIX_NP_OR_DATA },
    { "%NEmovbeS",	{ Gv, Ev }, PREFIX_NP_OR_DATA },
  },
  /* MOD_EVEX_MAP4_61 */
  {
    { "movbeS",		{ Mv, Gv }, PREFIX_NP_OR_DATA },
    { "%NEmovbeS",	{ Ev, Gv }, PREFIX_NP_OR_DATA },
  },
  /* MOD_EVEX_MAP4_F8_P_1 */
  {
    { "enqcmds",	{ Gva, M }, 0 },
    { VEX_W_TABLE (EVEX_W_MAP4_F8_P1_M_1) },
  },
  /* MOD_EVEX_MAP4_F8_P_3 */
  {
    { "enqcmd",		{ Gva, M }, 0 },
    { VEX_W_TABLE (EVEX_W_MAP4_F8_P3_M_1) },
  },

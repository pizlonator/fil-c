#define mpn_toomN_sqr mpn_toom2_sqr
#define mpn_toomN_sqr_itch mpn_toom2_sqr_itch
#define MIN_AN MPN_TOOM2_SQR_MINSIZE
#define MAX_AN SQR_TOOM3_THRESHOLD

#define MORE_SQR_TESTS explore_unlikely_branch
#include "toom-sqr-shared.h"

void
explore_unlikely_branch (gmp_randstate_ptr rands)
{
  mp_ptr ap, refp, pp, scratch;
  mp_size_t an;
  mp_bitcnt_t bit;
  TMP_DECL;
  TMP_MARK;

  ap = TMP_ALLOC_LIMBS (MAX_AN);
  refp = TMP_ALLOC_LIMBS (MAX_AN * 2);
  pp = 1 + TMP_ALLOC_LIMBS (MAX_AN * 2 + 2);
  scratch
    = 1+TMP_ALLOC_LIMBS (mpn_toomN_sqr_itch (MAX_AN) + 2);

  for (an = MIN_AN + (MIN_AN & 1); an < MAX_AN; an+=2)
    {
      mp_size_t itch;
      mp_limb_t p_before, p_after, s_before, s_after;

      bit = an / 2 * GMP_NUMB_BITS
	+ gmp_urandomm_ui (rands, an / 2 * GMP_NUMB_BITS - 1);

      mpn_zero (ap, an);
      mpn_zero (pp, an * 2);
      pp [an - 1] |= GMP_NUMB_HIGHBIT;
      pp [bit / GMP_NUMB_BITS] |= CNST_LIMB (1) << (bit % GMP_NUMB_BITS);
      mpn_sqrtrem (ap, NULL, pp, an);
      /* We need {ap, an} such that {ap + an/2, an/2} is zero and
	 the result {pp, 2*an} is such that the sum
	 {pp, an/2} + {pp + an/2, an/2} gives a carry. */
      mpn_random2 (pp-1, an * 2 + 2);
      p_before = pp[-1];
      p_after = pp[an * 2];

      itch = mpn_toomN_sqr_itch (an);
      ASSERT_ALWAYS (itch <= mpn_toomN_sqr_itch (MAX_AN));
      mpn_random2 (scratch-1, itch+2);
      s_before = scratch[-1];
      s_after = scratch[itch];

      mpn_toomN_sqr (pp, ap, an, scratch);
      refmpn_mul (refp, ap, an, ap, an);
      if (pp[-1] != p_before || pp[an * 2] != p_after
	  || scratch[-1] != s_before || scratch[itch] != s_after
	  || mpn_cmp (refp, pp, an * 2) != 0)
	{
	  printf ("ERROR with bit %lu, an = %d\n",
		  (unsigned long) bit, (int) an);
	  if (pp[-1] != p_before)
	    {
	      printf ("before pp:"); mpn_dump (pp -1, 1);
	      printf ("keep:   "); mpn_dump (&p_before, 1);
	    }
	  if (pp[an * 2] != p_after)
	    {
	      printf ("after pp:"); mpn_dump (pp + an * 2, 1);
	      printf ("keep:   "); mpn_dump (&p_after, 1);
	    }
	  if (scratch[-1] != s_before)
	    {
	      printf ("before scratch:"); mpn_dump (scratch-1, 1);
	      printf ("keep:   "); mpn_dump (&s_before, 1);
	    }
	  if (scratch[itch] != s_after)
	    {
	      printf ("after scratch:"); mpn_dump (scratch + itch, 1);
	      printf ("keep:   "); mpn_dump (&s_after, 1);
	    }
	  mpn_dump (ap, an);
	  mpn_dump (pp, an * 2);
	  mpn_dump (refp, an * 2);

	  abort();
	}
    }
  TMP_FREE;
}

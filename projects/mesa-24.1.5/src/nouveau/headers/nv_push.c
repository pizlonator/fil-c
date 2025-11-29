#include "nv_push.h"

#include "nv_device_info.h"

#include <inttypes.h>

#include "nvk_cl902d.h"
#include "nvk_cl906f.h"
#include "nvk_cl9097.h"
#include "nvk_cl90b5.h"
#include "nvk_cla097.h"
#include "nvk_cla0b5.h"
#include "nvk_cla0c0.h"
#include "nvk_clb197.h"
#include "nvk_clc0c0.h"
#include "nvk_clc1b5.h"
#include "nvk_clc397.h"
#include "nvk_clc3c0.h"
#include "nvk_clc597.h"
#include "nvk_clc5c0.h"
#include "nvk_clc697.h"
#include "nvk_clc6c0.h"
#include "nvk_clc7c0.h"

#ifndef NDEBUG
void
nv_push_validate(struct nv_push *push)
{
   uint32_t *cur = push->start;

   /* submitting empty push buffers is probably a bug */
   assert(push->end != push->start);

   /* make sure we don't overrun the bo */
   assert(push->end <= push->limit);

   /* parse all the headers to see if we get to buf->map */
   while (cur < push->end) {
      uint32_t hdr = *cur;
      uint32_t mthd = hdr >> 29;

      switch (mthd) {
      /* immd */
      case 4:
         break;
      case 1:
      case 3:
      case 5: {
         uint32_t count = (hdr >> 16) & 0x1fff;
         assert(count);
         cur += count;
         break;
      }
      default:
         assert(!"unknown method found");
      }

      cur++;
      assert(cur <= push->end);
   }
}
#endif

void
vk_push_print(FILE *fp, const struct nv_push *push,
              const struct nv_device_info *devinfo)
{
   uint32_t *cur = push->start;

   while (cur < push->end) {
      uint32_t hdr = *cur;
      uint32_t type = hdr >> 29;
      bool is_tert = type == 0 || type == 2;
      uint32_t inc = 0;
      uint32_t count = is_tert ? (hdr >> 18) & 0x3ff : (hdr >> 16) & 0x1fff;
      uint32_t tert_op = (hdr >> 16) & 0x3;
      uint32_t subchan = (hdr >> 13) & 0x7;
      uint32_t mthd = (hdr & 0xfff) << 2;
      uint32_t value = 0;
      bool is_immd = false;

      if (is_tert && tert_op != 0) {
         fprintf(fp, "[0x%08" PRIxPTR "] HDR %x subch N/A",
                 cur - push->start, hdr);
      } else {
         fprintf(fp, "[0x%08" PRIxPTR "] HDR %x subch %i",
                 cur - push->start, hdr, subchan);
      }

      cur++;

      const char *mthd_name = "";

      switch (type) {
      case 4:
         fprintf(fp, " IMMD\n");
         inc = 0;
         is_immd = true;
         value = count;
         count = 1;
         break;
      case 1:
         fprintf(fp, " NINC\n");
         inc = count;
         break;
      case 2:
      case 3:
         fprintf(fp, " 0INC\n");
         inc = 0;
         break;
      case 5:
         fprintf(fp, " 1INC\n");
         inc = 1;
         break;
      case 0:
         switch (tert_op) {
         case 0:
            fprintf(fp, " NINC\n");
            inc = count;
            break;
         case 1:
            fprintf(fp, " SUB_DEVICE_OP\n");
            mthd_name = "SET_SUBDEVICE_MASK";
            mthd = tert_op;
            value = (hdr >> 4) & 0xfff;
            count = 1;
            is_immd = true;
            break;
         case 2:
            fprintf(fp, " SUB_DEVICE_OP\n");
            mthd_name = "STORE_SUBDEVICE_MASK";
            mthd = tert_op;
            value = (hdr >> 4) & 0xfff;
            count = 1;
            is_immd = true;
            break;
         case 3:
            fprintf(fp, " SUB_DEVICE_OP\n");
            mthd_name = "USE_SUBDEVICE_MASK";
            mthd = tert_op;
            count = 1;
            break;
         }
         break;
      }

      while (count--) {
         if (!is_tert) {
            if (mthd < 0x100) {
               mthd_name = P_PARSE_NV906F_MTHD(mthd);
            } else {
               switch (subchan) {
               case 0:
                  if (devinfo->cls_eng3d >= 0xc697)
                     mthd_name = P_PARSE_NVC697_MTHD(mthd);
                  else if (devinfo->cls_eng3d >= 0xc597)
                     mthd_name = P_PARSE_NVC597_MTHD(mthd);
                  else if (devinfo->cls_eng3d >= 0xc397)
                     mthd_name = P_PARSE_NVC397_MTHD(mthd);
                  else if (devinfo->cls_eng3d >= 0xb197)
                     mthd_name = P_PARSE_NVB197_MTHD(mthd);
                  else if (devinfo->cls_eng3d >= 0xa097)
                     mthd_name = P_PARSE_NVA097_MTHD(mthd);
                  else
                     mthd_name = P_PARSE_NV9097_MTHD(mthd);
                  break;
               case 1:
                  if (devinfo->cls_compute >= 0xc7c0)
                     mthd_name = P_PARSE_NVC7C0_MTHD(mthd);
                  else if (devinfo->cls_compute >= 0xc6c0)
                     mthd_name = P_PARSE_NVC6C0_MTHD(mthd);
                  else if (devinfo->cls_compute >= 0xc5c0)
                     mthd_name = P_PARSE_NVC5C0_MTHD(mthd);
                  else if (devinfo->cls_compute >= 0xc3c0)
                     mthd_name = P_PARSE_NVC3C0_MTHD(mthd);
                  else if (devinfo->cls_compute >= 0xc0c0)
                     mthd_name = P_PARSE_NVC0C0_MTHD(mthd);
                  else
                     mthd_name = P_PARSE_NVA0C0_MTHD(mthd);
                  break;
               case 3:
                  mthd_name = P_PARSE_NV902D_MTHD(mthd);
                  break;
               case 4:
                  if (devinfo->cls_copy >= 0xc1b5)
                     mthd_name = P_PARSE_NVC1B5_MTHD(mthd);
                  else if (devinfo->cls_copy >= 0xa0b5)
                     mthd_name = P_PARSE_NVA0B5_MTHD(mthd);
                  else
                     mthd_name = P_PARSE_NV90B5_MTHD(mthd);
                  break;
               default:
                  mthd_name = "unknown method";
                  break;
               }
            }
         }

         if (!is_immd)
            value = *cur;

         fprintf(fp, "\tmthd %04x %s\n", mthd, mthd_name);
         if (mthd < 0x100) {
            P_DUMP_NV906F_MTHD_DATA(fp, mthd, value, "\t\t");
         } else {
            switch (subchan) {
            case 0:
               if (devinfo->cls_eng3d >= 0xc597)
                  P_DUMP_NVC597_MTHD_DATA(fp, mthd, value, "\t\t");
               else if (devinfo->cls_eng3d >= 0xc397)
                  P_DUMP_NVC397_MTHD_DATA(fp, mthd, value, "\t\t");
               else if (devinfo->cls_eng3d >= 0xb197)
                  P_DUMP_NVB197_MTHD_DATA(fp, mthd, value, "\t\t");
               else if (devinfo->cls_eng3d >= 0xa097)
                  P_DUMP_NVA097_MTHD_DATA(fp, mthd, value, "\t\t");
               else
                  P_DUMP_NV9097_MTHD_DATA(fp, mthd, value, "\t\t");
               break;
            case 1:
               if (devinfo->cls_compute >= 0xc3c0)
                  P_DUMP_NVC3C0_MTHD_DATA(fp, mthd, value, "\t\t");
               else if (devinfo->cls_compute >= 0xc0c0)
                  P_DUMP_NVC0C0_MTHD_DATA(fp, mthd, value, "\t\t");
               else
                  P_DUMP_NVA0C0_MTHD_DATA(fp, mthd, value, "\t\t");
               break;
            case 3:
               P_DUMP_NV902D_MTHD_DATA(fp, mthd, value, "\t\t");
               break;
            case 4:
               if (devinfo->cls_copy >= 0xc1b5)
                  P_DUMP_NVC1B5_MTHD_DATA(fp, mthd, value, "\t\t");
               else if (devinfo->cls_copy >= 0xa0b5)
                  P_DUMP_NVA0B5_MTHD_DATA(fp, mthd, value, "\t\t");
               else
                  P_DUMP_NV90B5_MTHD_DATA(fp, mthd, value, "\t\t");
               break;
            default:
               fprintf(fp, "%s.VALUE = 0x%x\n", "\t\t", value);
               break;
            }
         }

         if (!is_immd)
            cur++;

         if (inc) {
            inc--;
            mthd += 4;
         }
      }

      fprintf(fp, "\n");
   }
}

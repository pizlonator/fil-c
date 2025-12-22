/* GStreamer
 * Copyright (C) 2024 Seungha Yang <seungha@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstd3d12h264enc.h"
#include "gstd3d12encoder.h"
#include "gstd3d12dpbstorage.h"
#include "gstd3d12pluginutils.h"
#include <gst/base/gstqueuearray.h>
#include <gst/codecparsers/gsth264parser.h>
#include <gst/codecparsers/gsth264bitwriter.h>
#include <directx/d3dx12.h>
#include <wrl.h>
#include <string.h>
#include <cmath>
#include <vector>
#include <array>
#include <string>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <memory>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY_STATIC (gst_d3d12_h264_enc_debug);
#define GST_CAT_DEFAULT gst_d3d12_h264_enc_debug

enum
{
  PROP_0,
  PROP_RATE_CONTROL_SUPPORT,
  PROP_SLICE_MODE_SUPPORT,
  PROP_AUD,
  PROP_GOP_SIZE,
  PROP_REF_FRAMES,
  PROP_FRAME_ANALYSIS,
  PROP_RATE_CONTROL,
  PROP_BITRATE,
  PROP_MAX_BITRATE,
  PROP_QVBR_QUALITY,
  PROP_QP_INIT,
  PROP_QP_MIN,
  PROP_QP_MAX,
  PROP_QP_I,
  PROP_QP_P,
  PROP_QP_B,
  PROP_SLICE_MODE,
  PROP_SLICE_PARTITION,
  PROP_CC_INSERT,
};

#define DEFAULT_AUD TRUE
#define DEFAULT_FRAME_ANALYSIS FALSE
#define DEFAULT_GOP_SIZE 60
#define DEFAULT_RATE_CONTROL D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_VBR
#define DEFAULT_BITRATE 2000
#define DEFAULT_MAX_BITRATE 4000
#define DEFAULT_QVBR_QUALITY 23
#define DEFAULT_QP 0
#define DEFAULT_CQP 23
#define DEFAULT_REF_FRAMES 0
#define DEFAULT_SLICE_MODE D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_FULL_FRAME
#define DEFAULT_SLICE_PARTITION 0
#define DEFAULT_CC_INSERT GST_D3D12_ENCODER_SEI_INSERT

struct GstD3D12H264EncClassData
{
  gint64 luid;
  guint device_id;
  guint vendor_id;
  gchar *description;
  GstCaps *sink_caps;
  GstCaps *src_caps;
  guint rc_support;
  guint slice_mode_support;
};

/* *INDENT-OFF* */
class GstD3D12H264EncGop
{
public:
  void Init (guint gop_length)
  {
    /* TODO: pic_order_cnt_type == 0 for b-frame  */
    gop_struct_.pic_order_cnt_type = 2;
    if (gop_length == 1)
      gop_struct_.PPicturePeriod = 0;
    else
      gop_struct_.PPicturePeriod = 1;

    /* 0 means infinite */
    if (gop_length == 0) {
      gop_struct_.GOPLength = 0;
      gop_struct_.log2_max_frame_num_minus4 = 12;
    } else {
      /* count bits */
      guint val = gop_length;
      guint num_bits = 0;
      while (val) {
        num_bits++;
        val >>= 1;
      }

      if (num_bits < 4)
        gop_struct_.log2_max_frame_num_minus4 = 0;
      else if (num_bits > 16)
        gop_struct_.log2_max_frame_num_minus4 = 12;
      else
        gop_struct_.log2_max_frame_num_minus4 = num_bits - 4;

      gop_struct_.GOPLength = gop_length;
    }

    MaxFrameNum_ = 1 << (gop_struct_.log2_max_frame_num_minus4 + 4);

    if (gop_struct_.pic_order_cnt_type == 2) {
      /* unused */
      gop_struct_.log2_max_pic_order_cnt_lsb_minus4 = 0;
      MaxPicOrderCnt_ = MaxFrameNum_ * 2;
    } else {
      guint log2_max_pic_order_cnt = gop_struct_.log2_max_frame_num_minus4 + 5;
      log2_max_pic_order_cnt = MIN (16, log2_max_pic_order_cnt);
      gop_struct_.log2_max_pic_order_cnt_lsb_minus4 = log2_max_pic_order_cnt - 4;

      MaxPicOrderCnt_ = 1 << log2_max_pic_order_cnt;
    }

    gop_start_ = true;
    frame_num_ = 0;
    encode_order_ = 0;
  }

  D3D12_VIDEO_ENCODER_SEQUENCE_GOP_STRUCTURE_H264
  GetGopStruct ()
  {
    return gop_struct_;
  }

  void
  FillPicCtrl (D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_H264 & pic_ctrl)
  {
    if (gop_start_) {
      pic_ctrl.FrameType = D3D12_VIDEO_ENCODER_FRAME_TYPE_H264_IDR_FRAME;
      pic_ctrl.idr_pic_id = idr_pic_id_;
      pic_ctrl.FrameDecodingOrderNumber = 0;
      pic_ctrl.PictureOrderCountNumber = 0;
      pic_ctrl.TemporalLayerIndex = 0;
      idr_pic_id_++;
      gop_start_ = false;
    } else {
      pic_ctrl.FrameType = D3D12_VIDEO_ENCODER_FRAME_TYPE_H264_P_FRAME;
      pic_ctrl.idr_pic_id = idr_pic_id_;
      pic_ctrl.FrameDecodingOrderNumber = frame_num_;
      pic_ctrl.PictureOrderCountNumber = ((guint) frame_num_) * 2;
      pic_ctrl.TemporalLayerIndex = 0;
    }

    /* And increase frame num */
    /* FIXME: frame_num = (frame_num_of_pref_ref_pic + 1) % ...  */
    frame_num_ = (frame_num_ + 1) % MaxFrameNum_;
    encode_order_++;
    if (gop_struct_.GOPLength != 0 && encode_order_ >= gop_struct_.GOPLength) {
      frame_num_ = 0;
      encode_order_ = 0;
      gop_start_ = true;
    }
  }

  void ForceKeyUnit ()
  {
    frame_num_ = 0;
    encode_order_ = 0;
    gop_start_ = true;
  }

private:
  D3D12_VIDEO_ENCODER_SEQUENCE_GOP_STRUCTURE_H264 gop_struct_ = { };
  guint16 frame_num_ = 0;
  guint16 idr_pic_id_ = 0;
  guint32 MaxFrameNum_ = 16;
  guint32 MaxPicOrderCnt_ = 16;
  guint64 encode_order_ = 0;
  bool gop_start_ = false;
};

class GstD3D12H264EncDpb
{
public:
  GstD3D12H264EncDpb (GstD3D12Device * device, UINT width, UINT height,
       UINT max_dpb_size, bool array_of_textures)
  {
    max_dpb_size_ = max_dpb_size;
    if (max_dpb_size_ > 0) {
      storage_ = gst_d3d12_dpb_storage_new (device, max_dpb_size + 1,
          array_of_textures, DXGI_FORMAT_NV12, width, height,
          D3D12_RESOURCE_FLAG_VIDEO_ENCODE_REFERENCE_ONLY |
          D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
    }
  }

  ~GstD3D12H264EncDpb ()
  {
    gst_clear_object (&storage_);
  }

  bool IsValid ()
  {
    if (max_dpb_size_ > 0 && !storage_)
      return false;

    return true;
  }

  bool StartFrame (bool is_reference,
      D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_H264 * ctrl_data,
      D3D12_VIDEO_ENCODER_RECONSTRUCTED_PICTURE * recon_pic,
      D3D12_VIDEO_ENCODE_REFERENCE_FRAMES * ref_frames,
      UINT64 display_order)
  {
    ctrl_data_ = *ctrl_data;
    cur_display_order_ = display_order;
    cur_frame_is_ref_ = is_reference;

    recon_pic_.pReconstructedPicture = nullptr;
    recon_pic_.ReconstructedPictureSubresource = 0;

    if (max_dpb_size_ > 0 &&
      ctrl_data_.FrameType == D3D12_VIDEO_ENCODER_FRAME_TYPE_H264_IDR_FRAME) {
      ref_pic_desc_.clear();
      ref_pic_display_order_.clear ();
      gst_d3d12_dpb_storage_clear_dpb (storage_);
    }

    if (is_reference) {
      g_assert (max_dpb_size_ > 0);
      if (!gst_d3d12_dpb_storage_acquire_frame (storage_, &recon_pic_))
        return false;
    }

    *recon_pic = recon_pic_;

    switch (ctrl_data_.FrameType) {
      case D3D12_VIDEO_ENCODER_FRAME_TYPE_H264_P_FRAME:
      case D3D12_VIDEO_ENCODER_FRAME_TYPE_H264_B_FRAME:
        g_assert (max_dpb_size_ > 0);
        gst_d3d12_dpb_storage_get_reference_frames (storage_,
            ref_frames);
        break;
      default:
        ref_frames->NumTexture2Ds = 0;
        ref_frames->ppTexture2Ds = nullptr;
        ref_frames->pSubresources = nullptr;
        break;
    }

    list0_.clear ();
    list1_.clear ();

    bool build_l0 = (ctrl_data_.FrameType ==
      D3D12_VIDEO_ENCODER_FRAME_TYPE_H264_P_FRAME) ||
      (ctrl_data_.FrameType ==
      D3D12_VIDEO_ENCODER_FRAME_TYPE_H264_B_FRAME);
    bool build_l1 = ctrl_data_.FrameType ==
        D3D12_VIDEO_ENCODER_FRAME_TYPE_H264_B_FRAME;

    if (build_l0) {
      for (UINT i = 0; i < (UINT) ref_pic_display_order_.size (); i++) {
        if (ref_pic_display_order_[i] < display_order)
          list0_.push_back (i);
      }
    }

    if (build_l1) {
      for (UINT i = 0; i < (UINT) ref_pic_display_order_.size (); i++) {
        if (ref_pic_display_order_[i] > display_order)
          list1_.push_back (i);
      }
    }

    ctrl_data->List0ReferenceFramesCount = list0_.size ();
    ctrl_data->pList0ReferenceFrames =
        list0_.empty () ? nullptr : list0_.data ();

    ctrl_data->List1ReferenceFramesCount = list1_.size ();
    ctrl_data->pList1ReferenceFrames =
        list1_.empty () ? nullptr : list1_.data ();

    ctrl_data->ReferenceFramesReconPictureDescriptorsCount =
        ref_pic_desc_.size ();
    ctrl_data->pReferenceFramesReconPictureDescriptors =
        ref_pic_desc_.empty () ? nullptr : ref_pic_desc_.data ();

    return true;
  }

  void EndFrame ()
  {
    if (!cur_frame_is_ref_ || max_dpb_size_ == 0)
      return;

    if (gst_d3d12_dpb_storage_get_dpb_size (storage_) == max_dpb_size_) {
      gst_d3d12_dpb_storage_remove_oldest_frame (storage_);
      ref_pic_display_order_.pop_back ();
      ref_pic_desc_.pop_back ();
    }

    gst_d3d12_dpb_storage_add_frame (storage_, &recon_pic_);

    D3D12_VIDEO_ENCODER_REFERENCE_PICTURE_DESCRIPTOR_H264 desc = { };
    desc.ReconstructedPictureResourceIndex = 0;
    desc.IsLongTermReference = FALSE;
    desc.PictureOrderCountNumber = ctrl_data_.PictureOrderCountNumber;
    desc.FrameDecodingOrderNumber = ctrl_data_.FrameDecodingOrderNumber;
    desc.TemporalLayerIndex = 0;

    ref_pic_display_order_.insert (ref_pic_display_order_.begin (),
        cur_display_order_);
    ref_pic_desc_.insert (ref_pic_desc_.begin(), desc);
    for (UINT i = 1; i < ref_pic_desc_.size (); i++)
      ref_pic_desc_[i].ReconstructedPictureResourceIndex = i;
  }

private:
  std::vector<D3D12_VIDEO_ENCODER_REFERENCE_PICTURE_DESCRIPTOR_H264> ref_pic_desc_;
  std::vector<UINT64> ref_pic_display_order_;
  D3D12_VIDEO_ENCODER_RECONSTRUCTED_PICTURE recon_pic_ = { };
  D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_H264 ctrl_data_ = { };
  std::vector<UINT> list0_;
  std::vector<UINT> list1_;
  UINT max_dpb_size_ = 0;
  UINT64 cur_display_order_ = 0;
  bool cur_frame_is_ref_ = false;
  GstD3D12DpbStorage *storage_ = nullptr;
};

struct GstD3D12H264SPS
{
  void Clear ()
  {
    memset (&sps, 0, sizeof (GstH264SPS));
    bytes.clear ();
  }

  GstH264SPS sps;
  std::vector<guint8> bytes;
};

struct GstD3D12H264PPS
{
  void Clear ()
  {
    memset (&pps, 0, sizeof (GstH264PPS));
    bytes.clear ();
  }

  GstH264PPS pps;
  std::vector<guint8> bytes;
};

struct GstD3D12H264EncPrivate
{
  GstD3D12H264EncPrivate ()
  {
    cc_sei = g_array_new (FALSE, FALSE, sizeof (GstH264SEIMessage));
    g_array_set_clear_func (cc_sei, (GDestroyNotify) gst_h264_sei_clear);
  }

  ~GstD3D12H264EncPrivate ()
  {
    g_array_unref (cc_sei);
  }

  GstVideoInfo info;
  GstD3D12H264SPS sps;
  std::vector<GstD3D12H264PPS> pps;
  GstH264Profile selected_profile;
  GstD3D12H264EncGop gop;
  std::unique_ptr<GstD3D12H264EncDpb> dpb;
  guint last_pps_id = 0;
  guint64 display_order = 0;
  GArray *cc_sei;

  std::mutex prop_lock;

  GstD3D12EncoderConfig encoder_config = { };

  D3D12_VIDEO_ENCODER_PROFILE_H264 profile_h264 =
      D3D12_VIDEO_ENCODER_PROFILE_H264_MAIN;
  D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_H264 config_h264 = { };
  D3D12_VIDEO_ENCODER_LEVELS_H264 level_h264;
  D3D12_VIDEO_ENCODER_PICTURE_CONTROL_SUBREGIONS_LAYOUT_DATA_SLICES
      layout_slices = { };
  D3D12_VIDEO_ENCODER_SEQUENCE_GOP_STRUCTURE_H264 gop_struct_h264 = { };
  D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_H264 pic_control_h264 = { };

  D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE selected_rc_mode =
      D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_ABSOLUTE_QP_MAP;
  D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE selected_slice_mode =
      DEFAULT_SLICE_MODE;
  guint selected_ref_frames = 0;
  D3D12_VIDEO_ENCODER_CODEC_PICTURE_CONTROL_SUPPORT_H264 pic_ctrl_support = { };

  /* properties */
  gboolean aud = DEFAULT_AUD;

  /* gop struct related */
  guint gop_size = DEFAULT_GOP_SIZE;
  guint ref_frames = DEFAULT_REF_FRAMES;
  gboolean gop_updated = FALSE;

  /* rate control config */
  D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE rc_mode = DEFAULT_RATE_CONTROL;
  gboolean frame_analysis = DEFAULT_FRAME_ANALYSIS;
  gboolean rc_flag_updated = FALSE;
  guint bitrate = DEFAULT_BITRATE;
  guint max_bitrate = DEFAULT_MAX_BITRATE;
  guint qvbr_quality = DEFAULT_QVBR_QUALITY;
  guint qp_init = DEFAULT_QP;
  guint qp_min = DEFAULT_QP;
  guint qp_max = DEFAULT_QP;
  guint qp_i = DEFAULT_CQP;
  guint qp_p = DEFAULT_CQP;
  guint qp_b = DEFAULT_CQP;
  gboolean rc_updated = FALSE;

  /* slice mode */
  D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE slice_mode =
      DEFAULT_SLICE_MODE;
  guint slice_partition = DEFAULT_SLICE_PARTITION;
  gboolean slice_updated = FALSE;

  GstD3D12EncoderSeiInsertMode cc_insert = DEFAULT_CC_INSERT;
};
/* *INDENT-ON* */

struct GstD3D12H264Enc
{
  GstD3D12Encoder parent;

  GstD3D12H264EncPrivate *priv;
};

struct GstD3D12H264EncClass
{
  GstD3D12EncoderClass parent_class;

  GstD3D12H264EncClassData *cdata;
};

static inline GstD3D12H264Enc *
GST_D3D12_H264_ENC (gpointer ptr)
{
  return (GstD3D12H264Enc *) ptr;
}

static inline GstD3D12H264EncClass *
GST_D3D12_H264_ENC_GET_CLASS (gpointer ptr)
{
  return G_TYPE_INSTANCE_GET_CLASS (ptr, G_TYPE_FROM_INSTANCE (ptr),
      GstD3D12H264EncClass);
}

static void gst_d3d12_h264_enc_finalize (GObject * object);
static void gst_d3d12_h264_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_d3d12_h264_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_d3d12_h264_enc_start (GstVideoEncoder * encoder);
static gboolean gst_d3d12_h264_enc_stop (GstVideoEncoder * encoder);
static gboolean gst_d3d12_h264_enc_transform_meta (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame, GstMeta * meta);
static gboolean gst_d3d12_h264_enc_new_sequence (GstD3D12Encoder * encoder,
    ID3D12VideoDevice * video_device, GstVideoCodecState * state,
    GstD3D12EncoderConfig * config);
static gboolean gst_d3d12_h264_enc_start_frame (GstD3D12Encoder * encoder,
    ID3D12VideoDevice * video_device, GstVideoCodecFrame * frame,
    D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_DESC * seq_ctrl,
    D3D12_VIDEO_ENCODER_PICTURE_CONTROL_DESC * picture_ctrl,
    D3D12_VIDEO_ENCODER_RECONSTRUCTED_PICTURE * recon_pic,
    GstD3D12EncoderConfig * config, gboolean * need_new_session);
static gboolean gst_d3d12_h264_enc_end_frame (GstD3D12Encoder * encoder);

static GstElementClass *parent_class = nullptr;

static void
gst_d3d12_h264_enc_class_init (GstD3D12H264EncClass * klass, gpointer data)
{
  auto object_class = G_OBJECT_CLASS (klass);
  auto element_class = GST_ELEMENT_CLASS (klass);
  auto encoder_class = GST_VIDEO_ENCODER_CLASS (klass);
  auto d3d12enc_class = GST_D3D12_ENCODER_CLASS (klass);
  auto cdata = (GstD3D12H264EncClassData *) data;
  GstPadTemplate *pad_templ;
  auto read_only_params = (GParamFlags) (GST_PARAM_DOC_SHOW_DEFAULT |
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  auto rw_params = (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  parent_class = (GstElementClass *) g_type_class_peek_parent (klass);
  klass->cdata = cdata;

  object_class->finalize = gst_d3d12_h264_enc_finalize;
  object_class->set_property = gst_d3d12_h264_enc_set_property;
  object_class->get_property = gst_d3d12_h264_enc_get_property;

  g_object_class_install_property (object_class, PROP_RATE_CONTROL_SUPPORT,
      g_param_spec_flags ("rate-control-support", "Rate Control Support",
          "Supported rate control modes",
          GST_TYPE_D3D12_ENCODER_RATE_CONTROL_SUPPORT, 0, read_only_params));

  g_object_class_install_property (object_class, PROP_SLICE_MODE_SUPPORT,
      g_param_spec_flags ("slice-mode-support", "Slice Mode Support",
          "Supported slice partition modes",
          GST_TYPE_D3D12_ENCODER_SUBREGION_LAYOUT_SUPPORT, 1,
          read_only_params));

  g_object_class_install_property (object_class, PROP_AUD,
      g_param_spec_boolean ("aud", "AUD", "Use AU delimiter", DEFAULT_AUD,
          rw_params));

  g_object_class_install_property (object_class, PROP_GOP_SIZE,
      g_param_spec_uint ("gop-size", "GOP Size", "Size of GOP (0 = infinite)",
          0, G_MAXUINT32, DEFAULT_GOP_SIZE, rw_params));

  g_object_class_install_property (object_class, PROP_REF_FRAMES,
      g_param_spec_uint ("ref-frames", "Ref frames",
          "Preferred number of reference frames. Actual number of reference "
          "frames can be limited depending on hardware (0 = unspecified)",
          0, 16, DEFAULT_REF_FRAMES, rw_params));

  g_object_class_install_property (object_class, PROP_FRAME_ANALYSIS,
      g_param_spec_boolean ("frame-analysis", "Frame Analysis",
          "Enable 2 pass encoding if supported by hardware",
          DEFAULT_FRAME_ANALYSIS, rw_params));

  g_object_class_install_property (object_class, PROP_RATE_CONTROL,
      g_param_spec_enum ("rate-control", "Rate Control",
          "Rate Control Method", GST_TYPE_D3D12_ENCODER_RATE_CONTROL,
          DEFAULT_RATE_CONTROL, rw_params));

  g_object_class_install_property (object_class, PROP_BITRATE,
      g_param_spec_uint ("bitrate", "Bitrate",
          "Target bitrate in kbits/second. "
          "Used for \"cbr\", \"vbr\", and \"qvbr\" rate control",
          0, G_MAXUINT, DEFAULT_BITRATE, rw_params));

  g_object_class_install_property (object_class, PROP_MAX_BITRATE,
      g_param_spec_uint ("max-bitrate", "Max Bitrate",
          "Peak bitrate in kbits/second. "
          "Used for \"vbr\", and \"qvbr\" rate control",
          0, G_MAXUINT, DEFAULT_MAX_BITRATE, rw_params));

  g_object_class_install_property (object_class, PROP_QVBR_QUALITY,
      g_param_spec_uint ("qvbr-quality", "QVBR Quality",
          "Constant quality target value for \"qvbr\" rate control",
          0, 51, DEFAULT_QVBR_QUALITY, rw_params));

  g_object_class_install_property (object_class, PROP_QP_INIT,
      g_param_spec_uint ("qp-init", "QP Init",
          "Initial QP value. "
          "Used for \"cbr\", \"vbr\", and \"qvbr\" rate control",
          0, 51, DEFAULT_QP, rw_params));

  g_object_class_install_property (object_class, PROP_QP_MIN,
      g_param_spec_uint ("qp-min", "QP Min",
          "Minimum QP value for \"cbr\", \"vbr\", and \"qvbr\" rate control. "
          "To enable min/max QP setting, \"qp-max >= qp-min > 0\" "
          "condition should be satisfied", 0, 51, DEFAULT_QP, rw_params));

  g_object_class_install_property (object_class, PROP_QP_MAX,
      g_param_spec_uint ("qp-max", "QP Max",
          "Maximum QP value for \"cbr\", \"vbr\", and \"qvbr\" rate control. "
          "To enable min/max QP setting, \"qp-max >= qp-min > 0\" "
          "condition should be satisfied", 0, 51, DEFAULT_QP, rw_params));

  g_object_class_install_property (object_class, PROP_QP_I,
      g_param_spec_uint ("qp-i", "QP I",
          "Constant QP value for I frames. Used for \"cqp\" rate control",
          1, 51, DEFAULT_CQP, rw_params));

  g_object_class_install_property (object_class, PROP_QP_P,
      g_param_spec_uint ("qp-p", "QP P",
          "Constant QP value for P frames. Used for \"cqp\" rate control",
          1, 51, DEFAULT_CQP, rw_params));

  g_object_class_install_property (object_class, PROP_QP_I,
      g_param_spec_uint ("qp-b", "QP B",
          "Constant QP value for B frames. Used for \"cqp\" rate control",
          1, 51, DEFAULT_CQP, rw_params));

  g_object_class_install_property (object_class, PROP_SLICE_MODE,
      g_param_spec_enum ("slice-mode", "Slice Mode",
          "Slice partiton mode", GST_TYPE_D3D12_ENCODER_SUBREGION_LAYOUT,
          DEFAULT_SLICE_MODE, rw_params));

  g_object_class_install_property (object_class, PROP_SLICE_PARTITION,
      g_param_spec_uint ("slice-partition", "Slice partition",
          "Slice partition threshold interpreted depending on \"slice-mode\". "
          "If set zero, full frame encoding will be selected without "
          "partitioning regardless of requested \"slice-mode\"",
          0, G_MAXUINT, DEFAULT_SLICE_PARTITION, rw_params));

  g_object_class_install_property (object_class, PROP_CC_INSERT,
      g_param_spec_enum ("cc-insert", "Closed Caption Insert",
          "Closed Caption insert mode", GST_TYPE_D3D12_ENCODER_SEI_INSERT_MODE,
          DEFAULT_CC_INSERT, rw_params));

  std::string long_name = "Direct3D12 H.264 " + std::string (cdata->description)
      + " Encoder";
  gst_element_class_set_metadata (element_class, long_name.c_str (),
      "Codec/Encoder/Video/Hardware", "Direct3D12 H.264 Video Encoder",
      "Seungha Yang <seungha@centricular.com>");

  pad_templ = gst_pad_template_new ("sink",
      GST_PAD_SINK, GST_PAD_ALWAYS, cdata->sink_caps);
  gst_element_class_add_pad_template (element_class, pad_templ);

  pad_templ = gst_pad_template_new ("src",
      GST_PAD_SRC, GST_PAD_ALWAYS, cdata->src_caps);
  gst_element_class_add_pad_template (element_class, pad_templ);

  encoder_class->start = GST_DEBUG_FUNCPTR (gst_d3d12_h264_enc_start);
  encoder_class->stop = GST_DEBUG_FUNCPTR (gst_d3d12_h264_enc_stop);
  encoder_class->transform_meta =
      GST_DEBUG_FUNCPTR (gst_d3d12_h264_enc_transform_meta);

  d3d12enc_class->adapter_luid = cdata->luid;
  d3d12enc_class->device_id = cdata->device_id;
  d3d12enc_class->vendor_id = cdata->vendor_id;
  d3d12enc_class->new_sequence =
      GST_DEBUG_FUNCPTR (gst_d3d12_h264_enc_new_sequence);
  d3d12enc_class->start_frame =
      GST_DEBUG_FUNCPTR (gst_d3d12_h264_enc_start_frame);
  d3d12enc_class->end_frame = GST_DEBUG_FUNCPTR (gst_d3d12_h264_enc_end_frame);
}

static void
gst_d3d12_h264_enc_init (GstD3D12H264Enc * self)
{
  self->priv = new GstD3D12H264EncPrivate ();
}

static void
gst_d3d12_h264_enc_finalize (GObject * object)
{
  auto self = GST_D3D12_H264_ENC (object);

  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_d3d12_h264_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  auto self = GST_D3D12_H264_ENC (object);
  auto priv = self->priv;

  std::lock_guard < std::mutex > lk (priv->prop_lock);
  switch (prop_id) {
    case PROP_AUD:
      priv->aud = g_value_get_boolean (value);
      break;
    case PROP_GOP_SIZE:
    {
      auto gop_size = g_value_get_uint (value);
      if (gop_size != priv->gop_size) {
        priv->gop_size = gop_size;
        priv->gop_updated = TRUE;
      }
      break;
    }
    case PROP_REF_FRAMES:
    {
      auto ref_frames = g_value_get_uint (value);
      if (ref_frames != priv->ref_frames) {
        priv->ref_frames = ref_frames;
        priv->gop_updated = TRUE;
      }
      break;
    }
    case PROP_FRAME_ANALYSIS:
    {
      auto frame_analysis = g_value_get_boolean (value);
      if (frame_analysis != priv->frame_analysis) {
        priv->frame_analysis = frame_analysis;
        priv->rc_updated = TRUE;
      }
      break;
    }
    case PROP_RATE_CONTROL:
    {
      auto mode = (D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE)
          g_value_get_enum (value);
      if (mode != priv->rc_mode) {
        priv->rc_mode = mode;
        priv->rc_updated = TRUE;
      }
      break;
    }
    case PROP_BITRATE:
    {
      auto bitrate = g_value_get_uint (value);
      if (bitrate == 0)
        bitrate = DEFAULT_BITRATE;

      if (bitrate != priv->bitrate) {
        priv->bitrate = bitrate;
        if (priv->selected_rc_mode != D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP)
          priv->rc_updated = TRUE;
      }
      break;
    }
    case PROP_MAX_BITRATE:
    {
      auto max_bitrate = g_value_get_uint (value);
      if (max_bitrate == 0)
        max_bitrate = DEFAULT_MAX_BITRATE;

      if (max_bitrate != priv->max_bitrate) {
        priv->max_bitrate = max_bitrate;
        switch (priv->selected_rc_mode) {
          case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_VBR:
          case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_QVBR:
            priv->rc_updated = TRUE;
            break;
          default:
            break;
        }
      }
      break;
    }
    case PROP_QVBR_QUALITY:
    {
      auto qvbr_quality = g_value_get_uint (value);
      if (qvbr_quality != priv->qvbr_quality) {
        priv->qvbr_quality = qvbr_quality;
        if (priv->selected_rc_mode ==
            D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_QVBR) {
          priv->rc_updated = TRUE;
        }
      }
      break;
    }
    case PROP_QP_INIT:
    {
      auto qp_init = g_value_get_uint (value);
      if (qp_init != priv->qp_init) {
        priv->qp_init = qp_init;
        switch (priv->selected_rc_mode) {
          case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CBR:
          case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_VBR:
          case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_QVBR:
            priv->rc_updated = TRUE;
            break;
          default:
            break;
        }
      }
      break;
    }
    case PROP_QP_MIN:
    {
      auto qp_min = g_value_get_uint (value);
      if (qp_min != priv->qp_min) {
        priv->qp_min = qp_min;
        switch (priv->selected_rc_mode) {
          case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CBR:
          case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_VBR:
          case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_QVBR:
            priv->rc_updated = TRUE;
            break;
          default:
            break;
        }
      }
      break;
    }
    case PROP_QP_MAX:
    {
      auto qp_max = g_value_get_uint (value);
      if (qp_max != priv->qp_max) {
        priv->qp_max = qp_max;
        switch (priv->selected_rc_mode) {
          case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CBR:
          case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_VBR:
          case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_QVBR:
            priv->rc_updated = TRUE;
            break;
          default:
            break;
        }
      }
      break;
    }
    case PROP_QP_I:
    {
      auto qp_i = g_value_get_uint (value);
      if (qp_i != priv->qp_i) {
        priv->qp_i = qp_i;
        if (priv->selected_rc_mode == D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP)
          priv->rc_updated = TRUE;
      }
      break;
    }
    case PROP_QP_P:
    {
      auto qp_p = g_value_get_uint (value);
      if (qp_p != priv->qp_p) {
        priv->qp_p = qp_p;
        if (priv->selected_rc_mode == D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP)
          priv->rc_updated = TRUE;
      }
      break;
    }
    case PROP_QP_B:
    {
      auto qp_b = g_value_get_uint (value);
      if (qp_b != priv->qp_b) {
        priv->qp_b = qp_b;
        if (priv->selected_rc_mode == D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP)
          priv->rc_updated = TRUE;
      }
      break;
    }
    case PROP_SLICE_MODE:
    {
      auto slice_mode = (D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE)
          g_value_get_enum (value);
      if (slice_mode != priv->slice_mode) {
        priv->slice_mode = slice_mode;
        if (priv->selected_slice_mode != slice_mode)
          priv->slice_updated = TRUE;
      }
      break;
    }
    case PROP_SLICE_PARTITION:
    {
      auto slice_partition = g_value_get_uint (value);
      if (slice_partition != priv->slice_partition) {
        priv->slice_partition = slice_partition;
        if (priv->selected_slice_mode !=
            D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_FULL_FRAME) {
          priv->slice_updated = TRUE;
        }
      }
      break;
    }
    case PROP_CC_INSERT:
      priv->cc_insert = (GstD3D12EncoderSeiInsertMode) g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_d3d12_h264_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  auto self = GST_D3D12_H264_ENC (object);
  auto priv = self->priv;
  auto klass = GST_D3D12_H264_ENC_GET_CLASS (self);
  const auto cdata = klass->cdata;

  std::lock_guard < std::mutex > lk (priv->prop_lock);
  switch (prop_id) {
    case PROP_RATE_CONTROL_SUPPORT:
      g_value_set_flags (value, cdata->rc_support);
      break;
    case PROP_SLICE_MODE_SUPPORT:
      g_value_set_flags (value, cdata->slice_mode_support);
      break;
    case PROP_AUD:
      g_value_set_boolean (value, priv->aud);
      break;
    case PROP_GOP_SIZE:
      g_value_set_uint (value, priv->gop_size);
      break;
    case PROP_REF_FRAMES:
      g_value_set_uint (value, priv->ref_frames);
      break;
    case PROP_FRAME_ANALYSIS:
      g_value_set_boolean (value, priv->frame_analysis);
      break;
    case PROP_RATE_CONTROL:
      g_value_set_enum (value, priv->rc_mode);
      break;
    case PROP_BITRATE:
      g_value_set_uint (value, priv->bitrate);
      break;
    case PROP_MAX_BITRATE:
      g_value_set_uint (value, priv->max_bitrate);
      break;
    case PROP_QVBR_QUALITY:
      g_value_set_uint (value, priv->qvbr_quality);
      break;
    case PROP_QP_INIT:
      g_value_set_uint (value, priv->qp_init);
      break;
    case PROP_QP_MIN:
      g_value_set_uint (value, priv->qp_min);
      break;
    case PROP_QP_MAX:
      g_value_set_uint (value, priv->qp_max);
      break;
    case PROP_QP_I:
      g_value_set_uint (value, priv->qp_i);
      break;
    case PROP_QP_P:
      g_value_set_uint (value, priv->qp_p);
      break;
    case PROP_QP_B:
      g_value_set_uint (value, priv->qp_p);
      break;
    case PROP_SLICE_MODE:
      g_value_set_enum (value, priv->slice_mode);
      break;
    case PROP_SLICE_PARTITION:
      g_value_set_uint (value, priv->slice_partition);
      break;
    case PROP_CC_INSERT:
      g_value_set_enum (value, priv->cc_insert);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_d3d12_h264_enc_start (GstVideoEncoder * encoder)
{
  auto self = GST_D3D12_H264_ENC (encoder);
  auto priv = self->priv;

  GST_DEBUG_OBJECT (self, "Start");

  priv->display_order = 0;

  return GST_VIDEO_ENCODER_CLASS (parent_class)->start (encoder);
}

static gboolean
gst_d3d12_h264_enc_stop (GstVideoEncoder * encoder)
{
  auto self = GST_D3D12_H264_ENC (encoder);
  auto priv = self->priv;

  GST_DEBUG_OBJECT (self, "Stop");

  priv->dpb = nullptr;

  return GST_VIDEO_ENCODER_CLASS (parent_class)->stop (encoder);
}

static gboolean
gst_d3d12_h264_enc_transform_meta (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame, GstMeta * meta)
{
  auto self = GST_D3D12_H264_ENC (encoder);
  auto priv = self->priv;

  if (meta->info->api == GST_VIDEO_CAPTION_META_API_TYPE) {
    std::lock_guard < std::mutex > lk (priv->prop_lock);
    if (priv->cc_insert == GST_D3D12_ENCODER_SEI_INSERT_AND_DROP) {
      auto cc_meta = (GstVideoCaptionMeta *) meta;
      if (cc_meta->caption_type == GST_VIDEO_CAPTION_TYPE_CEA708_RAW)
        return FALSE;
    }
  }

  return GST_VIDEO_ENCODER_CLASS (parent_class)->transform_meta (encoder,
      frame, meta);
}

static gboolean
gst_d3d12_h264_enc_build_sps (GstD3D12H264Enc * self, const GstVideoInfo * info,
    const D3D12_VIDEO_ENCODER_PICTURE_RESOLUTION_DESC * resolution,
    guint num_ref)
{
  auto priv = self->priv;
  guint8 sps_buf[4096] = { 0, };
  /* *INDENT-OFF* */
  static const std::unordered_map<D3D12_VIDEO_ENCODER_LEVELS_H264, guint8>
      level_map = {
    {D3D12_VIDEO_ENCODER_LEVELS_H264_1, GST_H264_LEVEL_L1},
    {D3D12_VIDEO_ENCODER_LEVELS_H264_1b, GST_H264_LEVEL_L1B},
    {D3D12_VIDEO_ENCODER_LEVELS_H264_11, GST_H264_LEVEL_L1_1},
    {D3D12_VIDEO_ENCODER_LEVELS_H264_12, GST_H264_LEVEL_L1_2},
    {D3D12_VIDEO_ENCODER_LEVELS_H264_13, GST_H264_LEVEL_L1_3},
    {D3D12_VIDEO_ENCODER_LEVELS_H264_2, GST_H264_LEVEL_L2},
    {D3D12_VIDEO_ENCODER_LEVELS_H264_21, GST_H264_LEVEL_L2_1},
    {D3D12_VIDEO_ENCODER_LEVELS_H264_22, GST_H264_LEVEL_L2_2},
    {D3D12_VIDEO_ENCODER_LEVELS_H264_3, GST_H264_LEVEL_L3},
    {D3D12_VIDEO_ENCODER_LEVELS_H264_31, GST_H264_LEVEL_L3_1},
    {D3D12_VIDEO_ENCODER_LEVELS_H264_32, GST_H264_LEVEL_L3_2},
    {D3D12_VIDEO_ENCODER_LEVELS_H264_4, GST_H264_LEVEL_L4},
    {D3D12_VIDEO_ENCODER_LEVELS_H264_41, GST_H264_LEVEL_L4_1},
    {D3D12_VIDEO_ENCODER_LEVELS_H264_42, GST_H264_LEVEL_L4_2},
    {D3D12_VIDEO_ENCODER_LEVELS_H264_5, GST_H264_LEVEL_L5},
    {D3D12_VIDEO_ENCODER_LEVELS_H264_51, GST_H264_LEVEL_L5_1},
    {D3D12_VIDEO_ENCODER_LEVELS_H264_52, GST_H264_LEVEL_L5_2},
    {D3D12_VIDEO_ENCODER_LEVELS_H264_6, GST_H264_LEVEL_L6},
    {D3D12_VIDEO_ENCODER_LEVELS_H264_61, GST_H264_LEVEL_L6_1},
    {D3D12_VIDEO_ENCODER_LEVELS_H264_62, GST_H264_LEVEL_L6_2},
  };
  static const std::array<std::pair<gint, gint>, 17> par_map {{
    {0, 0},
    {1, 1},
    {12, 11},
    {10, 11},
    {16, 11},
    {40, 33},
    {24, 11},
    {20, 11},
    {32, 11},
    {80, 33},
    {18, 11},
    {15, 11},
    {64, 33},
    {160, 99},
    {4, 3},
    {3, 2},
    {2, 1}
  }};
  /* *INDENT-ON* */

  priv->sps.Clear ();
  auto & sps = priv->sps.sps;
  sps.id = 0;
  sps.profile_idc = priv->selected_profile;
  switch (sps.profile_idc) {
    case GST_H264_PROFILE_BASELINE:
      sps.constraint_set0_flag = 1;
      sps.constraint_set1_flag = 1;
      break;
    case GST_H264_PROFILE_MAIN:
      sps.constraint_set1_flag = 1;
      break;
    default:
      break;
  }

  if (priv->level_h264 == D3D12_VIDEO_ENCODER_LEVELS_H264_1b)
    sps.constraint_set3_flag = 1;

  sps.level_idc = level_map.at (priv->level_h264);
  /* d3d12 supports only 4:2:0 encoding */
  sps.chroma_format_idc = 1;
  sps.separate_colour_plane_flag = 0;
  /* add high-10 profile support */
  sps.bit_depth_luma_minus8 = 0;
  sps.bit_depth_chroma_minus8 = 0;
  sps.qpprime_y_zero_transform_bypass_flag = 0;
  sps.scaling_matrix_present_flag = 0;
  sps.log2_max_frame_num_minus4 =
      priv->gop_struct_h264.log2_max_frame_num_minus4;
  sps.pic_order_cnt_type = priv->gop_struct_h264.pic_order_cnt_type;
  sps.log2_max_pic_order_cnt_lsb_minus4 =
      priv->gop_struct_h264.log2_max_pic_order_cnt_lsb_minus4;
  sps.num_ref_frames = num_ref;
  sps.gaps_in_frame_num_value_allowed_flag = 0;
  sps.pic_width_in_mbs_minus1 = (resolution->Width / 16) - 1;
  sps.pic_height_in_map_units_minus1 = (resolution->Height / 16) - 1;
  sps.frame_mbs_only_flag = 1;

  if (sps.profile_idc != GST_H264_PROFILE_BASELINE)
    sps.direct_8x8_inference_flag = 1;

  if (resolution->Width != (UINT) info->width ||
      resolution->Height != (UINT) info->height) {
    sps.frame_cropping_flag = 1;
    sps.frame_crop_left_offset = 0;
    sps.frame_crop_right_offset = (resolution->Width - info->width) / 2;
    sps.frame_crop_top_offset = 0;
    sps.frame_crop_bottom_offset = (resolution->Height - info->height) / 2;
  }

  sps.vui_parameters_present_flag = 1;
  auto & vui = sps.vui_parameters;
  const auto colorimetry = &info->colorimetry;

  if (info->par_n > 0 && info->par_d > 0) {
    const auto it = std::find_if (par_map.begin (),
        par_map.end (),[&](const auto & par) {
          return par.first == info->par_n && par.second == info->par_d;
        }
    );

    if (it != par_map.end ()) {
      vui.aspect_ratio_info_present_flag = 1;
      vui.aspect_ratio_idc = (guint8) std::distance (par_map.begin (), it);
    } else if (info->par_n <= G_MAXUINT16 && info->par_d <= G_MAXUINT16) {
      vui.aspect_ratio_info_present_flag = 1;
      vui.aspect_ratio_idc = 0xff;
      vui.sar_width = info->par_n;
      vui.sar_height = info->par_d;
    }
  }

  vui.video_signal_type_present_flag = 1;
  /* Unspecified */
  vui.video_format = 5;
  vui.video_full_range_flag =
      colorimetry->range == GST_VIDEO_COLOR_RANGE_0_255 ? 1 : 0;
  vui.colour_description_present_flag = 1;
  vui.colour_primaries =
      gst_video_color_primaries_to_iso (colorimetry->primaries);
  vui.transfer_characteristics =
      gst_video_transfer_function_to_iso (colorimetry->transfer);
  vui.matrix_coefficients = gst_video_color_matrix_to_iso (colorimetry->matrix);
  if (info->fps_n > 0 && info->fps_d > 0) {
    vui.timing_info_present_flag = 1;
    vui.num_units_in_tick = info->fps_d;
    vui.time_scale = 2 * (guint) info->fps_n;
  }

  /* TODO: pic_struct_present_flag = 1 for picture timeing SEI */

  guint nal_size = G_N_ELEMENTS (sps_buf);
  GstH264BitWriterResult write_ret =
      gst_h264_bit_writer_sps (&sps, TRUE, sps_buf, &nal_size);
  if (write_ret != GST_H264_BIT_WRITER_OK) {
    GST_ERROR_OBJECT (self, "Couldn't build SPS");
    return FALSE;
  }

  priv->sps.bytes.resize (G_N_ELEMENTS (sps_buf));
  guint written_size = priv->sps.bytes.size ();
  write_ret = gst_h264_bit_writer_convert_to_nal (4, FALSE, TRUE, FALSE,
      sps_buf, nal_size * 8, priv->sps.bytes.data (), &written_size);
  if (write_ret != GST_H264_BIT_WRITER_OK) {
    GST_ERROR_OBJECT (self, "Couldn't build SPS bytes");
    return FALSE;
  }
  priv->sps.bytes.resize (written_size);

  return TRUE;
}

static gboolean
gst_d3d12_h264_enc_build_pps (GstD3D12H264Enc * self, guint num_ref)
{
  auto priv = self->priv;

  /* Driver does not seem to use num_ref_idx_active_override_flag.
   * Needs multiple PPS to signal ref pics */
  /* TODO: make more PPS for L1 ref pics */
  guint num_pps = MAX (1, num_ref);
  priv->pps.resize (num_pps);
  for (size_t i = 0; i < priv->pps.size (); i++) {
    guint8 pps_buf[1024] = { 0, };
    auto & d3d12_pps = priv->pps[i];
    d3d12_pps.Clear ();

    auto & pps = d3d12_pps.pps;

    pps.id = i;
    pps.sequence = &priv->sps.sps;
    if ((priv->config_h264.ConfigurationFlags &
            D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_H264_FLAG_ENABLE_CABAC_ENCODING)
        != 0) {
      pps.entropy_coding_mode_flag = 1;
    } else {
      pps.entropy_coding_mode_flag = 0;
    }

    pps.pic_order_present_flag = 0;
    pps.num_slice_groups_minus1 = 0;
    pps.num_ref_idx_l0_active_minus1 = i;
    pps.num_ref_idx_l1_active_minus1 = 0;
    pps.weighted_pred_flag = 0;
    pps.weighted_bipred_idc = 0;
    pps.pic_init_qp_minus26 = 0;
    pps.pic_init_qs_minus26 = 0;
    pps.chroma_qp_index_offset = 0;
    pps.deblocking_filter_control_present_flag = 1;
    if ((priv->config_h264.ConfigurationFlags &
            D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_H264_FLAG_USE_CONSTRAINED_INTRAPREDICTION)
        != 0) {
      pps.constrained_intra_pred_flag = 1;
    } else {
      pps.constrained_intra_pred_flag = 0;
    }
    pps.redundant_pic_cnt_present_flag = 0;
    if ((priv->config_h264.ConfigurationFlags &
            D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_H264_FLAG_USE_ADAPTIVE_8x8_TRANSFORM)
        != 0) {
      /* double check. This is not allowed for baseline and main profiles */
      pps.transform_8x8_mode_flag = 1;
    } else {
      pps.transform_8x8_mode_flag = 0;
    }

    pps.pic_scaling_matrix_present_flag = 0;
    pps.second_chroma_qp_index_offset = 0;

    guint nal_size = G_N_ELEMENTS (pps_buf);
    d3d12_pps.bytes.resize (nal_size);
    GstH264BitWriterResult write_ret =
        gst_h264_bit_writer_pps (&pps, TRUE, pps_buf, &nal_size);
    if (write_ret != GST_H264_BIT_WRITER_OK) {
      GST_ERROR_OBJECT (self, "Couldn't build PPS");
      return FALSE;
    }

    guint written_size = d3d12_pps.bytes.size ();
    write_ret = gst_h264_bit_writer_convert_to_nal (4, FALSE, TRUE, FALSE,
        pps_buf, nal_size * 8, d3d12_pps.bytes.data (), &written_size);
    if (write_ret != GST_H264_BIT_WRITER_OK) {
      GST_ERROR_OBJECT (self, "Couldn't build PPS bytes");
      return FALSE;
    }

    d3d12_pps.bytes.resize (written_size);
  }

  return TRUE;
}

static gboolean
gst_d3d12_h264_enc_get_max_ref_frames (GstD3D12H264Enc * self)
{
  auto priv = self->priv;
  const auto & pic_ctrl_support = priv->pic_ctrl_support;

  guint max_ref_frames = MIN (pic_ctrl_support.MaxL0ReferencesForP,
      pic_ctrl_support.MaxDPBCapacity);
  guint ref_frames = priv->ref_frames;

  if (max_ref_frames == 0) {
    GST_INFO_OBJECT (self,
        "Hardware does not support inter prediction, forcing all-intra");
    ref_frames = 0;
  } else if (priv->gop_size == 1) {
    GST_INFO_OBJECT (self, "User requested all-intra coding");
    ref_frames = 0;
  } else {
    /* TODO: at least 2 ref frames if B frame is enabled */
    if (ref_frames != 0)
      ref_frames = MIN (ref_frames, max_ref_frames);
    else
      ref_frames = 1;
  }

  return ref_frames;
}

static gboolean
gst_d3d12_h264_enc_update_gop (GstD3D12H264Enc * self,
    ID3D12VideoDevice * video_device, GstD3D12EncoderConfig * config,
    D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAGS * seq_flags)
{
  auto priv = self->priv;

  if (seq_flags && !priv->gop_updated)
    return TRUE;

  auto ref_frames = gst_d3d12_h264_enc_get_max_ref_frames (self);
  auto gop_size = priv->gop_size;
  if (ref_frames == 0)
    gop_size = 1;

  priv->last_pps_id = 0;

  auto prev_gop_struct = priv->gop.GetGopStruct ();
  auto prev_ref_frames = priv->selected_ref_frames;

  priv->selected_ref_frames = ref_frames;
  priv->gop.Init (gop_size);
  priv->gop_struct_h264 = priv->gop.GetGopStruct ();

  if (seq_flags) {
    if (prev_ref_frames != ref_frames ||
        memcmp (&prev_gop_struct, &priv->gop_struct_h264,
            sizeof (priv->gop_struct_h264)) != 0) {
      GST_DEBUG_OBJECT (self, "Gop struct updated");
      *seq_flags |=
          D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAG_GOP_SEQUENCE_CHANGE;
    }
  }

  GST_DEBUG_OBJECT (self,
      "Configured GOP struct, GOPLength: %u, PPicturePeriod: %u, "
      "pic_order_cnt_type: %d, log2_max_frame_num_minus4: %d, "
      "log2_max_pic_order_cnt_lsb_minus4: %d", priv->gop_struct_h264.GOPLength,
      priv->gop_struct_h264.PPicturePeriod,
      priv->gop_struct_h264.pic_order_cnt_type,
      priv->gop_struct_h264.log2_max_frame_num_minus4,
      priv->gop_struct_h264.log2_max_pic_order_cnt_lsb_minus4);

  priv->gop_updated = FALSE;

  return TRUE;
}

/* called with prop_lock taken */
static gboolean
gst_d3d12_h264_enc_update_rate_control (GstD3D12H264Enc * self,
    ID3D12VideoDevice * video_device, GstD3D12EncoderConfig * config,
    D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAGS * seq_flags)
{
  auto priv = self->priv;
  const D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE rc_modes[] = {
    D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_VBR,
    D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_QVBR,
    D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CBR,
    D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP,
  };

  if (seq_flags && !priv->rc_updated)
    return TRUE;

  GstD3D12EncoderConfig prev_config = *config;

  config->rate_control.Flags = D3D12_VIDEO_ENCODER_RATE_CONTROL_FLAG_NONE;
  UINT64 bitrate = priv->bitrate;
  if (bitrate == 0)
    bitrate = DEFAULT_BITRATE;

  UINT64 max_bitrate = priv->max_bitrate;
  if (max_bitrate < bitrate) {
    if (bitrate >= G_MAXUINT64 / 2)
      max_bitrate = bitrate;
    else
      max_bitrate = bitrate * 2;
  }

  /* Property uses kbps, and API uses bps */
  bitrate *= 1000;
  max_bitrate *= 1000;

  /* Fill every rate control struct and select later */
  config->cqp.ConstantQP_FullIntracodedFrame = priv->qp_i;
  config->cqp.ConstantQP_InterPredictedFrame_PrevRefOnly = priv->qp_p;
  config->cqp.ConstantQP_InterPredictedFrame_BiDirectionalRef = priv->qp_b;

  config->cbr.InitialQP = priv->qp_init;
  config->cbr.MinQP = priv->qp_min;
  config->cbr.MaxQP = priv->qp_max;
  config->cbr.TargetBitRate = bitrate;

  config->vbr.InitialQP = priv->qp_init;
  config->vbr.MinQP = priv->qp_min;
  config->vbr.MaxQP = priv->qp_max;
  config->vbr.TargetAvgBitRate = bitrate;
  config->vbr.PeakBitRate = max_bitrate;

  config->qvbr.InitialQP = priv->qp_init;
  config->qvbr.MinQP = priv->qp_min;
  config->qvbr.MaxQP = priv->qp_max;
  config->qvbr.TargetAvgBitRate = bitrate;
  config->qvbr.PeakBitRate = max_bitrate;
  config->qvbr.ConstantQualityTarget = priv->qvbr_quality;

  D3D12_FEATURE_DATA_VIDEO_ENCODER_RATE_CONTROL_MODE feature_data = { };
  feature_data.Codec = D3D12_VIDEO_ENCODER_CODEC_H264;
  feature_data.RateControlMode = priv->rc_mode;

  auto hr = video_device->CheckFeatureSupport
      (D3D12_FEATURE_VIDEO_ENCODER_RATE_CONTROL_MODE,
      &feature_data, sizeof (feature_data));
  if (SUCCEEDED (hr) && feature_data.IsSupported) {
    priv->selected_rc_mode = priv->rc_mode;
  } else {
    GST_INFO_OBJECT (self, "Requested rate control mode is not supported");

    for (guint i = 0; i < G_N_ELEMENTS (rc_modes); i++) {
      feature_data.RateControlMode = rc_modes[i];
      hr = video_device->CheckFeatureSupport
          (D3D12_FEATURE_VIDEO_ENCODER_RATE_CONTROL_MODE, &feature_data,
          sizeof (feature_data));
      if (SUCCEEDED (hr) && feature_data.IsSupported) {
        priv->selected_rc_mode = rc_modes[i];
        break;
      } else {
        feature_data.IsSupported = FALSE;
      }
    }

    if (!feature_data.IsSupported) {
      GST_ERROR_OBJECT (self, "Couldn't find support rate control mode");
      return FALSE;
    }
  }

  GST_INFO_OBJECT (self, "Requested rate control mode %d, selected %d",
      priv->rc_mode, priv->selected_rc_mode);

  config->rate_control.Mode = priv->selected_rc_mode;
  switch (priv->selected_rc_mode) {
    case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP:
      config->rate_control.ConfigParams.DataSize = sizeof (config->cqp);
      config->rate_control.ConfigParams.pConfiguration_CQP = &config->cqp;
      break;
    case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CBR:
      config->rate_control.ConfigParams.DataSize = sizeof (config->cbr);
      config->rate_control.ConfigParams.pConfiguration_CBR = &config->cbr;
      break;
    case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_VBR:
      config->rate_control.ConfigParams.DataSize = sizeof (config->vbr);
      config->rate_control.ConfigParams.pConfiguration_VBR = &config->vbr;
      break;
    case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_QVBR:
      config->rate_control.ConfigParams.DataSize = sizeof (config->qvbr);
      config->rate_control.ConfigParams.pConfiguration_QVBR = &config->qvbr;
      break;
    default:
      g_assert_not_reached ();
      return FALSE;
  }

  if (seq_flags) {
    if (prev_config.rate_control.Mode != config->rate_control.Mode) {
      GST_DEBUG_OBJECT (self, "Rate control mode changed %d -> %d",
          prev_config.rate_control.Mode, config->rate_control.Mode);
      *seq_flags |=
          D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAG_RATE_CONTROL_CHANGE;
    } else {
      void *prev, *cur;
      switch (config->rate_control.Mode) {
        case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP:
          prev = &prev_config.cqp;
          cur = &config->cqp;
          break;
        case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CBR:
          prev = &prev_config.cbr;
          cur = &config->cbr;
          break;
        case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_VBR:
          prev = &prev_config.vbr;
          cur = &config->vbr;
          break;
        case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_QVBR:
          prev = &prev_config.qvbr;
          cur = &config->cbr;
          break;
        default:
          g_assert_not_reached ();
          return FALSE;
      }

      if (memcmp (prev, cur, config->rate_control.ConfigParams.DataSize) != 0) {
        GST_DEBUG_OBJECT (self, "Rate control params updated");
        *seq_flags |=
            D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAG_RATE_CONTROL_CHANGE;
      }
    }
  }

  priv->rc_updated = FALSE;

  return TRUE;
}

static gboolean
gst_d3d12_h264_enc_update_slice (GstD3D12H264Enc * self,
    ID3D12VideoDevice * video_device, GstD3D12EncoderConfig * config,
    D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAGS * seq_flags,
    D3D12_VIDEO_ENCODER_SUPPORT_FLAGS * support_flags)
{
  auto priv = self->priv;

  if (seq_flags && !priv->slice_updated)
    return TRUE;

  auto encoder = GST_D3D12_ENCODER (self);
  auto prev_mode = priv->selected_slice_mode;
  auto prev_slice = priv->layout_slices;

  priv->selected_slice_mode =
      D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_FULL_FRAME;
  priv->layout_slices.NumberOfSlicesPerFrame = 1;
  config->max_subregions = 1;

  D3D12_FEATURE_DATA_VIDEO_ENCODER_SUPPORT support = { };
  D3D12_FEATURE_DATA_VIDEO_ENCODER_RESOLUTION_SUPPORT_LIMITS limits = { };
  D3D12_VIDEO_ENCODER_PROFILE_H264 suggested_profile = priv->profile_h264;
  D3D12_VIDEO_ENCODER_LEVELS_H264 suggested_level;

  support.Codec = D3D12_VIDEO_ENCODER_CODEC_H264;
  support.InputFormat = DXGI_FORMAT_NV12;
  support.CodecConfiguration = config->codec_config;
  support.CodecGopSequence = config->gop_struct;
  support.RateControl = config->rate_control;
  /* TODO: add intra-refresh support */
  support.IntraRefresh = D3D12_VIDEO_ENCODER_INTRA_REFRESH_MODE_NONE;
  support.ResolutionsListCount = 1;
  support.pResolutionList = &config->resolution;
  support.MaxReferenceFramesInDPB = priv->selected_ref_frames;
  support.pResolutionDependentSupport = &limits;
  support.SuggestedProfile.DataSize = sizeof (suggested_profile);
  support.SuggestedProfile.pH264Profile = &suggested_profile;
  support.SuggestedLevel.DataSize = sizeof (suggested_level);
  support.SuggestedLevel.pH264LevelSetting = &suggested_level;

  HRESULT hr;
  if (priv->slice_mode !=
      D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_FULL_FRAME
      && priv->slice_partition > 0) {
    /* TODO: fallback to other mode if possible */
    D3D12_FEATURE_DATA_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE
        feature_layout = { };
    feature_layout.Codec = D3D12_VIDEO_ENCODER_CODEC_H264;
    feature_layout.Profile = config->profile_desc;
    feature_layout.Level = config->level;
    feature_layout.SubregionMode = priv->slice_mode;
    hr = video_device->CheckFeatureSupport
        (D3D12_FEATURE_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE,
        &feature_layout, sizeof (feature_layout));
    if (!gst_d3d12_result (hr, encoder->device) || !feature_layout.IsSupported) {
      GST_WARNING_OBJECT (self, "Requested slice mode is not supported");
    } else {
      support.SubregionFrameEncoding = priv->slice_mode;
      hr = video_device->CheckFeatureSupport
          (D3D12_FEATURE_VIDEO_ENCODER_SUPPORT, &support, sizeof (support));
      if (gst_d3d12_result (hr, encoder->device)
          && CHECK_SUPPORT_FLAG (support.SupportFlags, GENERAL_SUPPORT_OK)
          && support.ValidationFlags == D3D12_VIDEO_ENCODER_VALIDATION_FLAG_NONE
          && limits.MaxSubregionsNumber > 1
          && limits.SubregionBlockPixelsSize > 0) {
        switch (priv->slice_mode) {
          case D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_BYTES_PER_SUBREGION:
            priv->selected_slice_mode =
                priv->slice_mode;
            /* Don't know how many slices would be generated */
            config->max_subregions = limits.MaxSubregionsNumber;
            *support_flags = support.SupportFlags;
            break;
          case D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_SQUARE_UNITS_PER_SUBREGION_ROW_UNALIGNED:
          {
            auto total_mbs =
                (config->resolution.Width / limits.SubregionBlockPixelsSize) *
                (config->resolution.Height / limits.SubregionBlockPixelsSize);
            if (priv->slice_partition >= total_mbs) {
              GST_DEBUG_OBJECT (self,
                  "Requested MBs per slice exceeds total MBs per frame");
            } else {
              priv->selected_slice_mode = priv->slice_mode;

              auto min_mbs_per_slice = (guint) std::ceil ((float) total_mbs
                  / limits.MaxSubregionsNumber);

              if (min_mbs_per_slice > priv->slice_partition) {
                GST_WARNING_OBJECT (self, "Too small number of MBs per slice");
                priv->layout_slices.NumberOfCodingUnitsPerSlice =
                    min_mbs_per_slice;
                config->max_subregions = limits.MaxSubregionsNumber;
              } else {
                priv->layout_slices.NumberOfCodingUnitsPerSlice =
                    priv->slice_partition;
                config->max_subregions = std::ceil ((float) total_mbs
                    / priv->slice_partition);
              }

              *support_flags = support.SupportFlags;
            }
            break;
          }
          case D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_UNIFORM_PARTITIONING_ROWS_PER_SUBREGION:
          {
            auto total_rows = config->resolution.Height /
                limits.SubregionBlockPixelsSize;
            if (priv->slice_partition >= total_rows) {
              GST_DEBUG_OBJECT (self,
                  "Requested rows per slice exceeds total rows per frame");
            } else {
              priv->selected_slice_mode = priv->slice_mode;

              auto min_rows_per_slice = (guint) std::ceil ((float) total_rows
                  / limits.MaxSubregionsNumber);

              if (min_rows_per_slice > priv->slice_partition) {
                GST_WARNING_OBJECT (self, "Too small number of rows per slice");
                priv->layout_slices.NumberOfRowsPerSlice = min_rows_per_slice;
                config->max_subregions = limits.MaxSubregionsNumber;
              } else {
                priv->layout_slices.NumberOfRowsPerSlice =
                    priv->slice_partition;
                config->max_subregions = (guint) std::ceil ((float) total_rows
                    / priv->slice_partition);
              }

              *support_flags = support.SupportFlags;
            }
            break;
          }
          case D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_UNIFORM_PARTITIONING_SUBREGIONS_PER_FRAME:
          {
            if (priv->slice_partition > 1) {
              priv->selected_slice_mode = priv->slice_mode;

              if (priv->slice_partition > limits.MaxSubregionsNumber) {
                GST_WARNING_OBJECT (self, "Too many slices per frame");
                priv->layout_slices.NumberOfSlicesPerFrame =
                    limits.MaxSubregionsNumber;
                config->max_subregions = limits.MaxSubregionsNumber;
              } else {
                priv->layout_slices.NumberOfSlicesPerFrame =
                    priv->slice_partition;
                config->max_subregions = priv->slice_partition;
              }

              *support_flags = support.SupportFlags;
            }
            break;
          }
          default:
            break;
        }
      }
    }
  }

  if (seq_flags && (prev_mode != priv->selected_slice_mode ||
          prev_slice.NumberOfSlicesPerFrame !=
          priv->layout_slices.NumberOfSlicesPerFrame)) {
    GST_DEBUG_OBJECT (self, "Slice mode updated");
    *seq_flags |=
        D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAG_SUBREGION_LAYOUT_CHANGE;
  }

  priv->slice_updated = FALSE;

  return TRUE;
}

static gboolean
gst_d3d12_h264_enc_reconfigure (GstD3D12H264Enc * self,
    ID3D12VideoDevice * video_device, GstD3D12EncoderConfig * config,
    D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAGS * seq_flags)
{
  auto encoder = GST_D3D12_ENCODER (self);
  auto priv = self->priv;
  auto prev_config = *config;

  if (!gst_d3d12_h264_enc_update_gop (self, video_device, config, seq_flags))
    return FALSE;

  if (!gst_d3d12_h264_enc_update_rate_control (self,
          video_device, config, seq_flags)) {
    return FALSE;
  }

  D3D12_FEATURE_DATA_VIDEO_ENCODER_SUPPORT support = { };
  D3D12_FEATURE_DATA_VIDEO_ENCODER_RESOLUTION_SUPPORT_LIMITS limits = { };
  D3D12_VIDEO_ENCODER_PROFILE_H264 suggested_profile = priv->profile_h264;

  support.Codec = D3D12_VIDEO_ENCODER_CODEC_H264;
  support.InputFormat = DXGI_FORMAT_NV12;
  support.CodecConfiguration = config->codec_config;
  support.CodecGopSequence = config->gop_struct;
  support.RateControl = config->rate_control;
  /* TODO: add intra-refresh support */
  support.IntraRefresh = D3D12_VIDEO_ENCODER_INTRA_REFRESH_MODE_NONE;
  support.SubregionFrameEncoding = priv->selected_slice_mode;
  support.ResolutionsListCount = 1;
  support.pResolutionList = &config->resolution;
  support.MaxReferenceFramesInDPB = priv->selected_ref_frames;
  support.pResolutionDependentSupport = &limits;
  support.SuggestedProfile.DataSize = sizeof (suggested_profile);
  support.SuggestedProfile.pH264Profile = &suggested_profile;
  support.SuggestedLevel = config->level;

  auto hr = video_device->CheckFeatureSupport
      (D3D12_FEATURE_VIDEO_ENCODER_SUPPORT, &support, sizeof (support));

  /* This is our minimum/simplest configuration
   * TODO: negotiate again depending on validation flags */
  if (!gst_d3d12_result (hr, encoder->device) ||
      !CHECK_SUPPORT_FLAG (support.SupportFlags, GENERAL_SUPPORT_OK) ||
      (support.ValidationFlags != D3D12_VIDEO_ENCODER_VALIDATION_FLAG_NONE)) {
    GST_ERROR_OBJECT (self, "Couldn't query encoder support");
    return FALSE;
  }

  /* Update rate control flags based on support flags */
  if (priv->frame_analysis) {
    if (CHECK_SUPPORT_FLAG (support.SupportFlags,
            RATE_CONTROL_FRAME_ANALYSIS_AVAILABLE)) {
      GST_INFO_OBJECT (self, "Frame analysis is enabled as requested");
      config->rate_control.Flags |=
          D3D12_VIDEO_ENCODER_RATE_CONTROL_FLAG_ENABLE_FRAME_ANALYSIS;
    } else {
      GST_INFO_OBJECT (self, "Frame analysis is not supported");
    }
  }

  if (priv->qp_init > 0) {
    switch (priv->selected_rc_mode) {
      case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CBR:
      case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_VBR:
      case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_QVBR:
        if (CHECK_SUPPORT_FLAG (support.SupportFlags,
                RATE_CONTROL_INITIAL_QP_AVAILABLE)) {
          GST_INFO_OBJECT (self, "Initial QP %d is enabled as requested",
              priv->qp_init);
          config->rate_control.Flags |=
              D3D12_VIDEO_ENCODER_RATE_CONTROL_FLAG_ENABLE_INITIAL_QP;
        } else {
          GST_INFO_OBJECT (self, "Initial QP is not supported");
        }
        break;
      default:
        break;
    }
  }

  if (priv->qp_max >= priv->qp_min && priv->qp_min > 0) {
    switch (priv->selected_rc_mode) {
      case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CBR:
      case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_VBR:
      case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_QVBR:
        if (CHECK_SUPPORT_FLAG (support.SupportFlags,
                RATE_CONTROL_ADJUSTABLE_QP_RANGE_AVAILABLE)) {
          GST_INFO_OBJECT (self, "QP range [%d, %d] is enabled as requested",
              priv->qp_min, priv->qp_max);
          config->rate_control.Flags |=
              D3D12_VIDEO_ENCODER_RATE_CONTROL_FLAG_ENABLE_QP_RANGE;
        } else {
          GST_INFO_OBJECT (self, "QP range is not supported");
        }
        break;
      default:
        break;
    }
  }

  if (seq_flags) {
    if (prev_config.rate_control.Flags != config->rate_control.Flags) {
      GST_DEBUG_OBJECT (self, "Rate control flag updated");
      *seq_flags |=
          D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAG_RATE_CONTROL_CHANGE;
    }
  }

  if (!gst_d3d12_h264_enc_update_slice (self, video_device, config,
          seq_flags, &support.SupportFlags)) {
    return FALSE;
  }

  config->support_flags = support.SupportFlags;

  if (!seq_flags ||
      (*seq_flags &
          D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAG_GOP_SEQUENCE_CHANGE) != 0) {
    priv->gop.ForceKeyUnit ();
    gst_d3d12_h264_enc_build_sps (self, &priv->info, &config->resolution,
        priv->selected_ref_frames);
    gst_d3d12_h264_enc_build_pps (self, priv->selected_ref_frames);

    bool array_of_textures = !CHECK_SUPPORT_FLAG (config->support_flags,
        RECONSTRUCTED_FRAMES_REQUIRE_TEXTURE_ARRAYS);
    auto dpb = std::make_unique < GstD3D12H264EncDpb > (encoder->device,
        config->resolution.Width, config->resolution.Height,
        priv->selected_ref_frames,
        array_of_textures);
    if (!dpb->IsValid ()) {
      GST_ERROR_OBJECT (self, "Couldn't create dpb");
      return FALSE;
    }

    GST_DEBUG_OBJECT (self, "New DPB configured");

    priv->dpb = nullptr;
    priv->dpb = std::move (dpb);
  }

  return TRUE;
}

static gboolean
gst_d3d12_h264_enc_new_sequence (GstD3D12Encoder * encoder,
    ID3D12VideoDevice * video_device, GstVideoCodecState * state,
    GstD3D12EncoderConfig * config)
{
  auto self = GST_D3D12_H264_ENC (encoder);
  auto priv = self->priv;
  auto info = &state->info;

  std::lock_guard < std::mutex > lk (priv->prop_lock);

  priv->dpb = nullptr;
  priv->info = state->info;

  config->profile_desc.DataSize = sizeof (D3D12_VIDEO_ENCODER_PROFILE_H264);
  config->profile_desc.pH264Profile = &priv->profile_h264;

  config->codec_config.DataSize =
      sizeof (D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_H264);
  config->codec_config.pH264Config = &priv->config_h264;

  config->level.DataSize = sizeof (D3D12_VIDEO_ENCODER_LEVELS_H264);
  config->level.pH264LevelSetting = &priv->level_h264;

  config->layout.DataSize =
      sizeof
      (D3D12_VIDEO_ENCODER_PICTURE_CONTROL_SUBREGIONS_LAYOUT_DATA_SLICES);
  config->layout.pSlicesPartition_H264 = &priv->layout_slices;

  config->gop_struct.DataSize =
      sizeof (D3D12_VIDEO_ENCODER_SEQUENCE_GOP_STRUCTURE_H264);
  config->gop_struct.pH264GroupOfPictures = &priv->gop_struct_h264;

  config->resolution.Width = GST_ROUND_UP_16 (info->width);
  config->resolution.Height = GST_ROUND_UP_16 (info->height);

  priv->selected_profile = GST_H264_PROFILE_MAIN;
  priv->profile_h264 = D3D12_VIDEO_ENCODER_PROFILE_H264_MAIN;

  auto allowed_caps =
      gst_pad_get_allowed_caps (GST_VIDEO_ENCODER_SRC_PAD (encoder));
  if (allowed_caps && !gst_caps_is_any (allowed_caps)) {
    allowed_caps = gst_caps_fixate (gst_caps_make_writable (allowed_caps));
    auto s = gst_caps_get_structure (allowed_caps, 0);
    auto profile_str = gst_structure_get_string (s, "profile");
    if (g_strcmp0 (profile_str, "high") == 0) {
      priv->selected_profile = GST_H264_PROFILE_HIGH;
      priv->profile_h264 = D3D12_VIDEO_ENCODER_PROFILE_H264_HIGH;
    } else if (g_strcmp0 (profile_str, "constrained-baseline") == 0) {
      priv->selected_profile = GST_H264_PROFILE_BASELINE;
      priv->profile_h264 = D3D12_VIDEO_ENCODER_PROFILE_H264_MAIN;
    } else {
      priv->selected_profile = GST_H264_PROFILE_MAIN;
      priv->profile_h264 = D3D12_VIDEO_ENCODER_PROFILE_H264_MAIN;
    }
  }

  gst_clear_caps (&allowed_caps);

  const gchar *profile_str = nullptr;
  switch (priv->selected_profile) {
    case GST_H264_PROFILE_BASELINE:
      profile_str = "constrained-baseline";
      break;
    case GST_H264_PROFILE_MAIN:
      profile_str = "main";
      break;
    case GST_H264_PROFILE_HIGH:
      profile_str = "high";
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  auto caps = gst_caps_new_simple ("video/x-h264",
      "alignment", G_TYPE_STRING, "au", "profile", G_TYPE_STRING, profile_str,
      "stream-format", G_TYPE_STRING, "byte-stream", nullptr);
  auto output_state =
      gst_video_encoder_set_output_state (GST_VIDEO_ENCODER (self), caps,
      state);
  gst_video_codec_state_unref (output_state);

  D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_PICTURE_CONTROL_SUPPORT
      feature_pic_ctrl = { };
  feature_pic_ctrl.Codec = D3D12_VIDEO_ENCODER_CODEC_H264;
  feature_pic_ctrl.Profile.DataSize = sizeof (priv->profile_h264);
  feature_pic_ctrl.Profile.pH264Profile = &priv->profile_h264;
  feature_pic_ctrl.PictureSupport.DataSize = sizeof (priv->pic_ctrl_support);
  feature_pic_ctrl.PictureSupport.pH264Support = &priv->pic_ctrl_support;
  auto hr = video_device->CheckFeatureSupport
      (D3D12_FEATURE_VIDEO_ENCODER_CODEC_PICTURE_CONTROL_SUPPORT,
      &feature_pic_ctrl, sizeof (feature_pic_ctrl));
  if (!gst_d3d12_result (hr, encoder->device) || !feature_pic_ctrl.IsSupported) {
    GST_ERROR_OBJECT (self, "Couldn't query picture control support");
    return FALSE;
  }

  if (info->fps_n > 0 && info->fps_d > 0) {
    config->rate_control.TargetFrameRate.Numerator = info->fps_n;
    config->rate_control.TargetFrameRate.Denominator = info->fps_d;
  } else {
    config->rate_control.TargetFrameRate.Numerator = 30;
    config->rate_control.TargetFrameRate.Denominator = 1;
  }

  priv->config_h264.ConfigurationFlags =
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_H264_FLAG_NONE;
  priv->config_h264.DirectModeConfig =
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_H264_DIRECT_MODES_DISABLED;
  priv->config_h264.DisableDeblockingFilterConfig =
      D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_H264_SLICES_DEBLOCKING_MODE_0_ALL_LUMA_CHROMA_SLICE_BLOCK_EDGES_ALWAYS_FILTERED;

  if (priv->selected_profile != GST_H264_PROFILE_BASELINE) {
    priv->config_h264.ConfigurationFlags |=
        D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_H264_FLAG_ENABLE_CABAC_ENCODING;
  }

  return gst_d3d12_h264_enc_reconfigure (self, video_device, config, nullptr);
}

static gboolean
gst_d3d12_h264_enc_foreach_caption_meta (GstBuffer * buffer, GstMeta ** meta,
    GArray * cc_sei)
{
  if ((*meta)->info->api != GST_VIDEO_CAPTION_META_API_TYPE)
    return TRUE;

  auto cc_meta = (GstVideoCaptionMeta *) (*meta);
  if (cc_meta->caption_type != GST_VIDEO_CAPTION_TYPE_CEA708_RAW)
    return TRUE;

  GstH264SEIMessage sei = { };
  sei.payloadType = GST_H264_SEI_REGISTERED_USER_DATA;
  auto rud = &sei.payload.registered_user_data;

  rud->country_code = 181;
  rud->size = cc_meta->size + 10;

  auto data = (guint8 *) g_malloc (rud->size);
  data[0] = 0;                  /* 16-bits itu_t_t35_provider_code */
  data[1] = 49;
  data[2] = 'G';                /* 32-bits ATSC_user_identifier */
  data[3] = 'A';
  data[4] = '9';
  data[5] = '4';
  data[6] = 3;                  /* 8-bits ATSC1_data_user_data_type_code */
  /* 8-bits:
   * 1 bit process_em_data_flag (0)
   * 1 bit process_cc_data_flag (1)
   * 1 bit additional_data_flag (0)
   * 5-bits cc_count
   */
  data[7] = ((cc_meta->size / 3) & 0x1f) | 0x40;
  data[8] = 255;                /* 8 bits em_data, unused */
  memcpy (data + 9, cc_meta->data, cc_meta->size);
  data[cc_meta->size + 9] = 255;        /* 8 marker bits */

  rud->data = data;

  g_array_append_val (cc_sei, sei);

  return TRUE;
}

static gboolean
gst_d3d12_h264_enc_start_frame (GstD3D12Encoder * encoder,
    ID3D12VideoDevice * video_device, GstVideoCodecFrame * frame,
    D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_DESC * seq_ctrl,
    D3D12_VIDEO_ENCODER_PICTURE_CONTROL_DESC * picture_ctrl,
    D3D12_VIDEO_ENCODER_RECONSTRUCTED_PICTURE * recon_pic,
    GstD3D12EncoderConfig * config, gboolean * need_new_session)
{
  auto self = GST_D3D12_H264_ENC (encoder);
  auto priv = self->priv;
  static guint8 aud_data[] = {
    0x00, 0x00, 0x00, 0x01, 0x09, 0xf0
  };

  *need_new_session = FALSE;

  std::lock_guard < std::mutex > lk (priv->prop_lock);
  seq_ctrl->Flags = D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAG_NONE;

  /* Reset GOP struct on force-keyunit */
  if (GST_VIDEO_CODEC_FRAME_IS_FORCE_KEYFRAME (frame)) {
    GST_DEBUG_OBJECT (self, "Force keyframe requested");
    priv->gop.ForceKeyUnit ();
  }

  auto prev_level = priv->level_h264;
  if (!gst_d3d12_h264_enc_reconfigure (self, video_device, config,
          &seq_ctrl->Flags)) {
    GST_ERROR_OBJECT (self, "Reconfigure failed");
    return FALSE;
  }

  if (seq_ctrl->Flags != D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAG_NONE) {
    *need_new_session =
        gst_d3d12_encoder_check_needs_new_session (config->support_flags,
        seq_ctrl->Flags);
  }

  if (priv->level_h264 != prev_level)
    *need_new_session = TRUE;

  if (*need_new_session) {
    seq_ctrl->Flags = D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAG_NONE;

    GST_DEBUG_OBJECT (self, "Needs new session, forcing IDR");
    priv->gop.ForceKeyUnit ();
  }

  priv->gop.FillPicCtrl (priv->pic_control_h264);

  if (priv->pic_control_h264.FrameType ==
      D3D12_VIDEO_ENCODER_FRAME_TYPE_H264_IDR_FRAME) {
    GST_LOG_OBJECT (self, "Sync point at frame %" G_GUINT64_FORMAT,
        priv->display_order);
    GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);
  }

  seq_ctrl->IntraRefreshConfig.Mode =
      D3D12_VIDEO_ENCODER_INTRA_REFRESH_MODE_NONE;
  seq_ctrl->IntraRefreshConfig.IntraRefreshDuration = 0;
  seq_ctrl->RateControl = config->rate_control;
  seq_ctrl->PictureTargetResolution = config->resolution;
  seq_ctrl->SelectedLayoutMode = priv->selected_slice_mode;
  seq_ctrl->FrameSubregionsLayoutData = config->layout;
  seq_ctrl->CodecGopSequence = config->gop_struct;

  picture_ctrl->IntraRefreshFrameIndex = 0;
  /* TODO: b frame can be non-reference picture */
  picture_ctrl->Flags = priv->selected_ref_frames > 0 ?
      D3D12_VIDEO_ENCODER_PICTURE_CONTROL_FLAG_USED_AS_REFERENCE_PICTURE :
      D3D12_VIDEO_ENCODER_PICTURE_CONTROL_FLAG_NONE;
  picture_ctrl->PictureControlCodecData.DataSize =
      sizeof (priv->pic_control_h264);
  picture_ctrl->PictureControlCodecData.pH264PicData = &priv->pic_control_h264;

  if (!priv->dpb->StartFrame (picture_ctrl->Flags ==
          D3D12_VIDEO_ENCODER_PICTURE_CONTROL_FLAG_USED_AS_REFERENCE_PICTURE,
          &priv->pic_control_h264, recon_pic, &picture_ctrl->ReferenceFrames,
          priv->display_order)) {
    GST_ERROR_OBJECT (self, "Start frame failed");
    return FALSE;
  }

  priv->display_order++;

  priv->pic_control_h264.Flags =
      D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_H264_FLAG_NONE;
  /* FIXME: count L1 too */
  priv->pic_control_h264.pic_parameter_set_id =
      priv->pic_control_h264.List0ReferenceFramesCount > 1 ?
      priv->pic_control_h264.List0ReferenceFramesCount - 1 : 0;
  priv->pic_control_h264.adaptive_ref_pic_marking_mode_flag = 0;
  priv->pic_control_h264.RefPicMarkingOperationsCommandsCount = 0;
  priv->pic_control_h264.pRefPicMarkingOperationsCommands = nullptr;
  priv->pic_control_h264.List0RefPicModificationsCount = 0;
  priv->pic_control_h264.pList0RefPicModifications = nullptr;
  priv->pic_control_h264.List1RefPicModificationsCount = 0;
  priv->pic_control_h264.pList1RefPicModifications = nullptr;
  priv->pic_control_h264.QPMapValuesCount = 0;
  priv->pic_control_h264.pRateControlQPMap = nullptr;

  if (priv->pic_control_h264.FrameType ==
      D3D12_VIDEO_ENCODER_FRAME_TYPE_H264_IDR_FRAME) {
    auto buf_size = priv->sps.bytes.size () + priv->pps[0].bytes.size ();
    if (priv->aud)
      buf_size += sizeof (aud_data);

    auto output_buf = gst_buffer_new_and_alloc (buf_size);
    GstMapInfo map_info;
    gst_buffer_map (output_buf, &map_info, GST_MAP_WRITE);
    auto data = (guint8 *) map_info.data;

    if (priv->aud) {
      memcpy (data, aud_data, sizeof (aud_data));
      data += sizeof (aud_data);
    }

    memcpy (data, priv->sps.bytes.data (), priv->sps.bytes.size ());
    data += priv->sps.bytes.size ();

    memcpy (data, priv->pps[0].bytes.data (), priv->pps[0].bytes.size ());
    gst_buffer_unmap (output_buf, &map_info);
    frame->output_buffer = output_buf;

    priv->last_pps_id = 0;
  } else if (priv->pic_control_h264.pic_parameter_set_id != priv->last_pps_id) {
    const auto & cur_pps =
        priv->pps[priv->pic_control_h264.pic_parameter_set_id];
    auto buf_size = cur_pps.bytes.size ();

    if (priv->aud)
      buf_size += sizeof (aud_data);

    auto output_buf = gst_buffer_new_and_alloc (buf_size);
    GstMapInfo map_info;
    gst_buffer_map (output_buf, &map_info, GST_MAP_WRITE);
    auto data = (guint8 *) map_info.data;

    if (priv->aud) {
      memcpy (data, aud_data, sizeof (aud_data));
      data += sizeof (aud_data);
    }

    memcpy (data, cur_pps.bytes.data (), cur_pps.bytes.size ());
    gst_buffer_unmap (output_buf, &map_info);
    frame->output_buffer = output_buf;

    priv->last_pps_id = priv->pic_control_h264.pic_parameter_set_id;
  } else if (priv->aud) {
    auto buf_size = sizeof (aud_data);
    auto output_buf = gst_buffer_new_and_alloc (buf_size);
    GstMapInfo map_info;
    gst_buffer_map (output_buf, &map_info, GST_MAP_WRITE);
    memcpy (map_info.data, aud_data, sizeof (aud_data));
    gst_buffer_unmap (output_buf, &map_info);
    frame->output_buffer = output_buf;
  }

  if (priv->cc_insert != GST_D3D12_ENCODER_SEI_DISABLED) {
    g_array_set_size (priv->cc_sei, 0);
    gst_buffer_foreach_meta (frame->input_buffer,
        (GstBufferForeachMetaFunc) gst_d3d12_h264_enc_foreach_caption_meta,
        priv->cc_sei);
    if (priv->cc_sei->len > 0) {
      auto mem = gst_h264_create_sei_memory (4, priv->cc_sei);
      if (mem) {
        GST_TRACE_OBJECT (self, "Inserting CC SEI");

        if (!frame->output_buffer)
          frame->output_buffer = gst_buffer_new ();

        gst_buffer_append_memory (frame->output_buffer, mem);
      }
    }
  }

  return TRUE;
}

static gboolean
gst_d3d12_h264_enc_end_frame (GstD3D12Encoder * encoder)
{
  auto self = GST_D3D12_H264_ENC (encoder);
  auto priv = self->priv;

  priv->dpb->EndFrame ();

  return TRUE;
}

void
gst_d3d12_h264_enc_register (GstPlugin * plugin, GstD3D12Device * device,
    ID3D12VideoDevice * video_device, guint rank)
{
  HRESULT hr;
  std::vector < std::string > profiles;

  GST_DEBUG_CATEGORY_INIT (gst_d3d12_h264_enc_debug,
      "d3d12h264enc", 0, "d3d12h264enc");

  D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC feature_codec = { };
  feature_codec.Codec = D3D12_VIDEO_ENCODER_CODEC_H264;
  hr = video_device->CheckFeatureSupport (D3D12_FEATURE_VIDEO_ENCODER_CODEC,
      &feature_codec, sizeof (feature_codec));

  if (FAILED (hr) || !feature_codec.IsSupported) {
    GST_INFO_OBJECT (device, "Device does not support H.264 encoding");
    return;
  }

  D3D12_FEATURE_DATA_VIDEO_ENCODER_PROFILE_LEVEL feature_profile_level = { };
  D3D12_VIDEO_ENCODER_PROFILE_H264 profile_h264 =
      D3D12_VIDEO_ENCODER_PROFILE_H264_MAIN;
  D3D12_VIDEO_ENCODER_LEVELS_H264 level_h264_min;
  D3D12_VIDEO_ENCODER_LEVELS_H264 level_h264_max;

  feature_profile_level.Codec = D3D12_VIDEO_ENCODER_CODEC_H264;
  feature_profile_level.Profile.DataSize = sizeof (profile_h264);
  feature_profile_level.Profile.pH264Profile = &profile_h264;
  feature_profile_level.MinSupportedLevel.DataSize = sizeof (level_h264_min);
  feature_profile_level.MinSupportedLevel.pH264LevelSetting = &level_h264_min;
  feature_profile_level.MaxSupportedLevel.DataSize = sizeof (level_h264_max);
  feature_profile_level.MaxSupportedLevel.pH264LevelSetting = &level_h264_max;

  D3D12_FEATURE_DATA_VIDEO_ENCODER_INPUT_FORMAT feature_input_format = { };
  feature_input_format.Codec = D3D12_VIDEO_ENCODER_CODEC_H264;
  feature_input_format.Profile = feature_profile_level.Profile;

  hr = video_device->CheckFeatureSupport
      (D3D12_FEATURE_VIDEO_ENCODER_PROFILE_LEVEL, &feature_profile_level,
      sizeof (feature_profile_level));
  if (FAILED (hr) || !feature_profile_level.IsSupported) {
    GST_WARNING_OBJECT (device, "Main profile is not supported");
    return;
  }

  feature_input_format.Format = DXGI_FORMAT_NV12;
  hr = video_device->CheckFeatureSupport
      (D3D12_FEATURE_VIDEO_ENCODER_INPUT_FORMAT, &feature_input_format,
      sizeof (feature_input_format));
  if (FAILED (hr) || !feature_input_format.IsSupported) {
    GST_WARNING_OBJECT (device, "NV12 format is not supported");
    return;
  }

  profiles.push_back ("constrained-baseline");
  profiles.push_back ("main");
  GST_INFO_OBJECT (device, "Main profile is supported, level [%d, %d]",
      level_h264_min, level_h264_max);

  profile_h264 = D3D12_VIDEO_ENCODER_PROFILE_H264_HIGH;
  hr = video_device->CheckFeatureSupport
      (D3D12_FEATURE_VIDEO_ENCODER_PROFILE_LEVEL, &feature_profile_level,
      sizeof (feature_profile_level));
  if (SUCCEEDED (hr) && feature_profile_level.IsSupported) {
    profiles.push_back ("high");
    GST_INFO_OBJECT (device, "High profile is supported, level [%d, %d]",
        level_h264_min, level_h264_max);
  }

  if (profiles.empty ()) {
    GST_WARNING_OBJECT (device, "Couldn't find supported profile");
    return;
  }

  D3D12_FEATURE_DATA_VIDEO_ENCODER_OUTPUT_RESOLUTION_RATIOS_COUNT ratios_count
      = { };
  ratios_count.Codec = D3D12_VIDEO_ENCODER_CODEC_H264;
  hr = video_device->CheckFeatureSupport
      (D3D12_FEATURE_VIDEO_ENCODER_OUTPUT_RESOLUTION_RATIOS_COUNT,
      &ratios_count, sizeof (ratios_count));
  if (FAILED (hr)) {
    GST_WARNING_OBJECT (device,
        "Couldn't query output resolution ratios count");
    return;
  }

  std::vector < D3D12_VIDEO_ENCODER_PICTURE_RESOLUTION_RATIO_DESC > ratios;

  D3D12_FEATURE_DATA_VIDEO_ENCODER_OUTPUT_RESOLUTION feature_resolution = { };
  feature_resolution.Codec = D3D12_VIDEO_ENCODER_CODEC_H264;
  feature_resolution.ResolutionRatiosCount = ratios_count.ResolutionRatiosCount;
  if (ratios_count.ResolutionRatiosCount > 0) {
    ratios.resize (ratios_count.ResolutionRatiosCount);
    feature_resolution.pResolutionRatios = &ratios[0];
  }

  hr = video_device->CheckFeatureSupport
      (D3D12_FEATURE_VIDEO_ENCODER_OUTPUT_RESOLUTION, &feature_resolution,
      sizeof (feature_resolution));
  if (FAILED (hr) || !feature_resolution.IsSupported) {
    GST_WARNING_OBJECT (device, "Couldn't query output resolution");
    return;
  }

  GST_INFO_OBJECT (device,
      "Device supported resolution %ux%u - %ux%u, align requirement %u, %u",
      feature_resolution.MinResolutionSupported.Width,
      feature_resolution.MinResolutionSupported.Height,
      feature_resolution.MaxResolutionSupported.Width,
      feature_resolution.MaxResolutionSupported.Height,
      feature_resolution.ResolutionWidthMultipleRequirement,
      feature_resolution.ResolutionHeightMultipleRequirement);

  guint rc_support = 0;
  D3D12_FEATURE_DATA_VIDEO_ENCODER_RATE_CONTROL_MODE feature_rate_control = { };
  feature_rate_control.Codec = D3D12_VIDEO_ENCODER_CODEC_H264;
  feature_rate_control.RateControlMode =
      D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP;

  hr = video_device->CheckFeatureSupport
      (D3D12_FEATURE_VIDEO_ENCODER_RATE_CONTROL_MODE, &feature_rate_control,
      sizeof (feature_rate_control));
  if (SUCCEEDED (hr) && feature_rate_control.IsSupported) {
    GST_INFO_OBJECT (device, "CQP suported");
    rc_support |= (1 << D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP);
  }

  feature_rate_control.RateControlMode =
      D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CBR;
  hr = video_device->CheckFeatureSupport
      (D3D12_FEATURE_VIDEO_ENCODER_RATE_CONTROL_MODE, &feature_rate_control,
      sizeof (feature_rate_control));
  if (SUCCEEDED (hr) && feature_rate_control.IsSupported) {
    GST_INFO_OBJECT (device, "CBR suported");
    rc_support |= (1 << D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CBR);
  }

  feature_rate_control.RateControlMode =
      D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_VBR;
  hr = video_device->CheckFeatureSupport
      (D3D12_FEATURE_VIDEO_ENCODER_RATE_CONTROL_MODE, &feature_rate_control,
      sizeof (feature_rate_control));
  if (SUCCEEDED (hr) && feature_rate_control.IsSupported) {
    GST_INFO_OBJECT (device, "VBR suported");
    rc_support |= (1 << D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_VBR);
  }

  feature_rate_control.RateControlMode =
      D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_QVBR;
  hr = video_device->CheckFeatureSupport
      (D3D12_FEATURE_VIDEO_ENCODER_RATE_CONTROL_MODE, &feature_rate_control,
      sizeof (feature_rate_control));
  if (SUCCEEDED (hr) && feature_rate_control.IsSupported) {
    GST_INFO_OBJECT (device, "VBR suported");
    rc_support |= (1 << D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_QVBR);
  }

  if (!rc_support) {
    GST_WARNING_OBJECT (device, "Couldn't find supported rate control mode");
    return;
  }

  profile_h264 = D3D12_VIDEO_ENCODER_PROFILE_H264_MAIN;
  D3D12_FEATURE_DATA_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE
      feature_layout = { };
  feature_layout.Codec = D3D12_VIDEO_ENCODER_CODEC_H264;
  feature_layout.Profile.DataSize = sizeof (profile_h264);
  feature_layout.Profile.pH264Profile = &profile_h264;
  feature_layout.Level.DataSize = sizeof (D3D12_VIDEO_ENCODER_LEVELS_H264);

  D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE layout_modes[] = {
    D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_FULL_FRAME,
    D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_BYTES_PER_SUBREGION,
    D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_SQUARE_UNITS_PER_SUBREGION_ROW_UNALIGNED,
    D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_UNIFORM_PARTITIONING_ROWS_PER_SUBREGION,
    D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_UNIFORM_PARTITIONING_SUBREGIONS_PER_FRAME,
  };

  guint slice_mode_support = 0;
  for (guint i = 0; i < G_N_ELEMENTS (layout_modes); i++) {
    feature_layout.SubregionMode = layout_modes[i];
    for (guint level = (guint) level_h264_min; level <= (guint) level_h264_max;
        level++) {
      auto level_h264 = (D3D12_VIDEO_ENCODER_LEVELS_H264) level;
      feature_layout.Level.pH264LevelSetting = &level_h264;
      hr = video_device->CheckFeatureSupport
          (D3D12_FEATURE_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE,
          &feature_layout, sizeof (feature_layout));
      if (SUCCEEDED (hr) && feature_layout.IsSupported) {
        slice_mode_support |= (1 << layout_modes[i]);
        break;
      }
    }
  }

  if (!slice_mode_support) {
    GST_WARNING_OBJECT (device, "No supported subregion layout");
    return;
  }

  if (slice_mode_support & (1 <<
          D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_FULL_FRAME)
      == 0) {
    GST_WARNING_OBJECT (device, "Full frame encoding is not supported");
    return;
  }

  auto subregions =
      g_flags_to_string (GST_TYPE_D3D12_ENCODER_SUBREGION_LAYOUT_SUPPORT,
      slice_mode_support);
  GST_INFO_OBJECT (device, "Supported subregion modes: \"%s\"", subregions);
  g_free (subregions);

  D3D12_VIDEO_ENCODER_CODEC_PICTURE_CONTROL_SUPPORT_H264 picture_ctrl_h264;
  D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_PICTURE_CONTROL_SUPPORT
      feature_pic_ctrl = { };

  feature_pic_ctrl.Codec = D3D12_VIDEO_ENCODER_CODEC_H264;
  feature_pic_ctrl.Profile.DataSize = sizeof (profile_h264);
  feature_pic_ctrl.Profile.pH264Profile = &profile_h264;
  feature_pic_ctrl.PictureSupport.DataSize = sizeof (picture_ctrl_h264);
  feature_pic_ctrl.PictureSupport.pH264Support = &picture_ctrl_h264;

  hr = video_device->CheckFeatureSupport
      (D3D12_FEATURE_VIDEO_ENCODER_CODEC_PICTURE_CONTROL_SUPPORT,
      &feature_pic_ctrl, sizeof (feature_pic_ctrl));
  if (FAILED (hr) || !feature_pic_ctrl.IsSupported) {
    GST_WARNING_OBJECT (device, "Couldn't query picture control support");
    return;
  }

  GST_INFO_OBJECT (device, "MaxL0ReferencesForP: %u, MaxL0ReferencesForB: %u, "
      "MaxL1ReferencesForB: %u, MaxLongTermReferences: %u, MaxDPBCapacity %u",
      picture_ctrl_h264.MaxL0ReferencesForP,
      picture_ctrl_h264.MaxL0ReferencesForB,
      picture_ctrl_h264.MaxL1ReferencesForB,
      picture_ctrl_h264.MaxLongTermReferences,
      picture_ctrl_h264.MaxDPBCapacity);

  std::string resolution_str = "width = (int) [" +
      std::to_string (feature_resolution.MinResolutionSupported.Width) + ", " +
      std::to_string (feature_resolution.MaxResolutionSupported.Width) +
      "], height = (int) [" +
      std::to_string (feature_resolution.MinResolutionSupported.Height) + ", " +
      std::to_string (feature_resolution.MaxResolutionSupported.Height) + " ]";
  std::string sink_caps_str = "video/x-raw, format = (string) NV12, " +
      resolution_str + ", interlace-mode = (string) progressive";

  std::string src_caps_str = "video/x-h264, " + resolution_str +
      ", stream-format = (string) byte-stream, alignment = (string) au, ";
  if (profiles.size () == 1) {
    src_caps_str += "profile = (string) " + profiles[0];
  } else {
    src_caps_str += "profile = (string) { ";
    std::reverse (profiles.begin (), profiles.end ());
    for (size_t i = 0; i < profiles.size (); i++) {
      if (i != 0)
        src_caps_str += ", ";
      src_caps_str += profiles[i];
    }
    src_caps_str += " }";
  }

  auto sysmem_caps = gst_caps_from_string (sink_caps_str.c_str ());
  auto sink_caps = gst_caps_copy (sysmem_caps);
  gst_caps_set_features_simple (sink_caps,
      gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_D3D12_MEMORY, nullptr));
  gst_caps_append (sink_caps, sysmem_caps);
  auto src_caps = gst_caps_from_string (src_caps_str.c_str ());

  GST_MINI_OBJECT_FLAG_SET (sink_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (src_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  GstD3D12H264EncClassData *cdata = new GstD3D12H264EncClassData ();
  g_object_get (device, "adapter-luid", &cdata->luid,
      "device-id", &cdata->device_id, "vendor-id", &cdata->vendor_id,
      "description", &cdata->description, nullptr);
  cdata->sink_caps = sink_caps;
  cdata->src_caps = src_caps;
  cdata->rc_support = rc_support;
  cdata->slice_mode_support = slice_mode_support;

  GType type;
  gchar *type_name;
  gchar *feature_name;
  guint index = 0;
  GTypeInfo type_info = {
    sizeof (GstD3D12H264EncClass),
    nullptr,
    nullptr,
    (GClassInitFunc) gst_d3d12_h264_enc_class_init,
    nullptr,
    nullptr,
    sizeof (GstD3D12H264Enc),
    0,
    (GInstanceInitFunc) gst_d3d12_h264_enc_init,
  };

  type_info.class_data = cdata;

  type_name = g_strdup ("GstD3D12H264Enc");
  feature_name = g_strdup ("d3d12h264enc");
  while (g_type_from_name (type_name)) {
    index++;
    g_free (type_name);
    g_free (feature_name);
    type_name = g_strdup_printf ("GstD3D12H264Device%dEnc", index);
    feature_name = g_strdup_printf ("d3d12h264device%denc", index);
  }

  type = g_type_register_static (GST_TYPE_D3D12_ENCODER,
      type_name, &type_info, (GTypeFlags) 0);

  if (rank > 0 && index != 0)
    rank--;

  if (index != 0)
    gst_element_type_set_skip_documentation (type);

  if (!gst_element_register (plugin, feature_name, rank, type))
    GST_WARNING ("Failed to register plugin '%s'", type_name);

  g_free (type_name);
  g_free (feature_name);
}

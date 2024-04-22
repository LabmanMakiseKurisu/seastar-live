// Rec. ITU-T H.264 (02/2016)
// 7.3.2.1.1 Sequence parameter set data syntax (p66)

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "bitstream.h"
#include "h264-sps.h"

int h264_sps(bitstream_t* stream, struct h264_sps_t* sps)
{
	int i;
	memset(sps, 0, sizeof(struct h264_sps_t));
	sps->chroma_format_idc = 1;
	sps->profile_idc = (uint8_t)bitstream_read_bits(stream, 8);
	sps->constraint_set_flag = (uint8_t)bitstream_read_bits(stream, 8);
	sps->level_idc = (uint8_t)bitstream_read_bits(stream, 8);
	sps->seq_parameter_set_id = (uint8_t)bitstream_read_ue(stream);
	if( sps->profile_idc == 100 || sps->profile_idc == 110 ||
		sps->profile_idc == 122 || sps->profile_idc == 244 || sps->profile_idc == 44  ||
		sps->profile_idc == 83  || sps->profile_idc == 86  || sps->profile_idc == 118 ||
		sps->profile_idc == 128 || sps->profile_idc == 138 || sps->profile_idc == 139 ||
		sps->profile_idc == 134 || sps->profile_idc == 135)
	{
		sps->chroma_format_idc = (uint8_t)bitstream_read_ue(stream);
		if(3 == sps->chroma_format_idc)
			sps->chroma.separate_colour_plane_flag = bitstream_read_bit(stream);
		sps->chroma.bit_depth_luma_minus8 = (uint8_t)bitstream_read_ue(stream);
		sps->chroma.bit_depth_chroma_minus8 = (uint8_t)bitstream_read_ue(stream);
		sps->chroma.qpprime_y_zero_transform_bypass_flag = bitstream_read_bit(stream);
		sps->chroma.seq_scaling_matrix_present_flag = bitstream_read_bit(stream);
		if(sps->chroma.seq_scaling_matrix_present_flag)
		{
			for (i = 0; i < ((sps->chroma_format_idc != 3) ? 8 : 12); i++)
			{
				sps->chroma.seq_scaling_list_present_flag[ i ] = bitstream_read_bit(stream);
				// if(sps->chroma.seq_scaling_list_present_flag[ i ])
				// {
				// 	if(i < 6)
				// 	{
				// 		h264_scaling_list_4x4(stream, sps->chroma.ScalingList4x4[i], &sps->chroma.UseDefaultScalingMatrix4x4Flag[i]);
				// 	}
				// 	else
				// 	{
				// 		h264_scaling_list_8x8(stream, sps->chroma.ScalingList8x8[i-6], &sps->chroma.UseDefaultScalingMatrix8x8Flag[i-6]);
				// 	}
				// }
			}
		}
	}

	sps->log2_max_frame_num_minus4 = (uint8_t)bitstream_read_ue(stream);
	sps->pic_order_cnt_type = (uint8_t)bitstream_read_ue(stream);
	if(0 == sps->pic_order_cnt_type)
	{
		sps->log2_max_pic_order_cnt_lsb_minus4 = (uint8_t)bitstream_read_ue(stream);
	}
	else if(1 == sps->pic_order_cnt_type)
	{
		sps->delta_pic_order_always_zero_flag = bitstream_read_bit(stream);
		sps->offset_for_non_ref_pic = (int32_t)bitstream_read_se(stream);
		sps->offset_for_top_to_bottom_field = (int32_t)bitstream_read_se(stream);
		sps->num_ref_frames_in_pic_order_cnt_cycle = (uint8_t)bitstream_read_ue(stream);
		sps->offset_for_ref_frame = (int32_t*)malloc(sps->num_ref_frames_in_pic_order_cnt_cycle * sizeof(int32_t));
		for(i=0; i<sps->num_ref_frames_in_pic_order_cnt_cycle; i++)
			sps->offset_for_ref_frame[i] = (int32_t)bitstream_read_se(stream);
	}

	sps->max_num_ref_frames = (uint32_t)bitstream_read_ue(stream);
	sps->gaps_in_frame_num_value_allowed_flag = bitstream_read_bit(stream);
	sps->pic_width_in_mbs_minus1 = (uint32_t)bitstream_read_ue(stream);
	sps->pic_height_in_map_units_minus1 = (uint32_t)bitstream_read_ue(stream);
	sps->frame_mbs_only_flag = bitstream_read_bit(stream);
	if(!sps->frame_mbs_only_flag)
		sps->mb_adaptive_frame_field_flag = bitstream_read_bit(stream);
	sps->direct_8x8_inference_flag = bitstream_read_bit(stream);
	sps->frame_cropping_flag = bitstream_read_bit(stream);
	if(sps->frame_cropping_flag)
	{
		sps->frame_cropping.frame_crop_left_offset	= (int32_t)bitstream_read_ue(stream);
		sps->frame_cropping.frame_crop_right_offset	= (int32_t)bitstream_read_ue(stream);
		sps->frame_cropping.frame_crop_top_offset	= (int32_t)bitstream_read_ue(stream);
		sps->frame_cropping.frame_crop_bottom_offset= (int32_t)bitstream_read_ue(stream);
	}
	sps->vui_parameters_present_flag = bitstream_read_bit(stream);
	// if(sps->vui_parameters_present_flag)
	// {
	// 	struct h264_vui_t vui;
	// 	memset(&vui, 0, sizeof(struct h264_vui_t));
	// 	h264_vui(stream, &vui);
	// }

	return 0;
}

static int h264_crop_unit(const struct h264_sps_t* sps, int* x, int *y)
{
	const int SubWidthC[] = { 0 /*4:0:0*/, 2 /*4:2:0*/, 2 /*4:2:2*/, 1 /*4:4:4*/ };
	const int SubHeightC[] = { 0 /*4:0:0*/, 2 /*4:2:0*/, 1 /*4:2:2*/, 1 /*4:4:4*/ };
	int chroma_array_type;
	int frame_mbs_only_flag;

	frame_mbs_only_flag = sps->frame_mbs_only_flag ? 1 : 0;

	// Depending on the value of separate_colour_plane_flag, the value of the variable ChromaArrayType is assigned as follows:
	// - If separate_colour_plane_flag is equal to 0, ChromaArrayType is set equal to chroma_format_idc.
	// - Otherwise (separate_colour_plane_flag is equal to 1), ChromaArrayType is set equal to 0.
	chroma_array_type = sps->chroma.separate_colour_plane_flag ? 0 : sps->chroma_format_idc;
	if (chroma_array_type > 3)
		return -1;

	/*
	The variables CropUnitX and CropUnitY are derived as follows:
	- If ChromaArrayType is equal to 0, CropUnitX and CropUnitY are derived as:
		CropUnitX = 1
		CropUnitY = 2 - frame_mbs_only_flag
	- Otherwise (ChromaArrayType is equal to 1, 2, or 3), CropUnitX and CropUnitY are derived as:
		CropUnitX = SubWidthC
		CropUnitY = SubHeightC * ( 2 - frame_mbs_only_flag )
	*/
	if (0 == chroma_array_type)
	{
		*x = 1;
		*y = 2 - frame_mbs_only_flag;
	}
	else
	{
		*x = SubWidthC[chroma_array_type];
		*y = SubHeightC[chroma_array_type] * (2 - frame_mbs_only_flag);
	}

	return 0;
}

int h264_codec_rect(const struct h264_sps_t* sps, int* x, int *y, int *w, int *h)
{
	int dx, dy;
	int pic_width_in_mbs;
	int pic_height_in_mbs;
	int frame_mbs_only_flag;
	if (0 != h264_crop_unit(sps, &dx, &dy))
		return -1;

	frame_mbs_only_flag = sps->frame_mbs_only_flag ? 1 : 0;
	pic_width_in_mbs = sps->pic_width_in_mbs_minus1 + 1;
	pic_height_in_mbs = (sps->pic_height_in_map_units_minus1 + 1) * (2 - frame_mbs_only_flag);

	*x = 0;
	*y = 0;
	*w = pic_width_in_mbs * 16;
	*h = pic_height_in_mbs * 16;
	return 0;
}

int h264_display_rect(const struct h264_sps_t* sps, int* x, int *y, int *w, int *h)
{
	int dx, dy;
	int pic_width_in_mbs;
	int pic_height_in_mbs;
	int frame_mbs_only_flag;
	if (0 != h264_crop_unit(sps, &dx, &dy))
		return -1;

	frame_mbs_only_flag = sps->frame_mbs_only_flag ? 1 : 0;
	pic_width_in_mbs = sps->pic_width_in_mbs_minus1 + 1;
	pic_height_in_mbs = (sps->pic_height_in_map_units_minus1 + 1) * (2 - frame_mbs_only_flag);

	*x = sps->frame_cropping_flag ? sps->frame_cropping.frame_crop_left_offset * dx : 0;
	*y = sps->frame_cropping_flag ? sps->frame_cropping.frame_crop_top_offset * dy : 0;
	*w = sps->frame_cropping_flag ? sps->frame_cropping.frame_crop_right_offset * dx : 0;
	*h = sps->frame_cropping_flag ? sps->frame_cropping.frame_crop_bottom_offset * dy : 0;
	*w = pic_width_in_mbs * 16 - *w - *x;
	*h = pic_height_in_mbs * 16 - *h - *y;
	return 0;
}
/*
 * Copyright (c) 2016 Florent Revest, <florent.revest@free-electrons.com>
 *               2007 Intel Corporation. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "sunxi_cedrus_drv_video.h"
#include "picture.h"
#include "buffer.h"
#include "context.h"
#include "surface.h"
#include "va_config.h"

#include "mpeg2.h"
#include "mpeg4.h"

#include <assert.h>
#include <string.h>

#include <errno.h>

#include <sys/ioctl.h>

#include <linux/videodev2.h>

#define OUTPUT_BUFFER_DATA_TO_FILE

#ifdef OUTPUT_BUFFER_DATA_TO_FILE
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <err.h>

int fp = -1;
const char *file_path = "/tmp/vaapi_cedrus_buffer_output.bin";
#endif

/*
 * A Picture is an encoded input frame made of several buffers. A single input
 * can contain slice data, headers and IQ matrix. Each Picture is assigned a
 * request ID when created and each corresponding buffer might be turned into a
 * v4l buffers or extended control when rendered. Finally they are submitted to
 * kernel space when reaching EndPicture.
 */

VAStatus sunxi_cedrus_BeginPicture(VADriverContextP ctx, VAContextID context,
		VASurfaceID render_target)
{
	INIT_DRIVER_DATA
	VAStatus vaStatus = VA_STATUS_SUCCESS;
	object_context_p obj_context;
	object_surface_p obj_surface;

	sunxi_cedrus_msg("%s\n", __FUNCTION__);

	obj_context = CONTEXT(context);
	assert(obj_context);

	obj_surface = SURFACE(render_target);
	assert(obj_surface);

	if(obj_surface->status == VASurfaceRendering)
		sunxi_cedrus_SyncSurface(ctx, render_target);

	obj_surface->status = VASurfaceRendering;
	obj_surface->request = (obj_context->num_rendered_surfaces)%INPUT_BUFFERS_NB+1;
	obj_surface->input_buf_index = obj_context->num_rendered_surfaces%INPUT_BUFFERS_NB;
	obj_context->num_rendered_surfaces ++;

	obj_context->current_render_target = obj_surface->base.id;

	return vaStatus;
}

VAStatus sunxi_cedrus_RenderPicture(VADriverContextP ctx, VAContextID context,
		VABufferID *buffers, int num_buffers)
{
	INIT_DRIVER_DATA
	VAStatus vaStatus = VA_STATUS_SUCCESS;
	object_context_p obj_context;
	object_surface_p obj_surface;
	object_config_p obj_config;
	int i;
#ifdef OUTPUT_BUFFER_DATA_TO_FILE
	uint32_t buffer_size;
//	uint8_t buffer_type;
	ssize_t ret;
#endif

	sunxi_cedrus_msg("%s\n", __FUNCTION__);
	obj_context = CONTEXT(context);
	assert(obj_context);

	obj_config = CONFIG(obj_context->config_id);
	if (NULL == obj_config)
	{
		vaStatus = VA_STATUS_ERROR_INVALID_CONFIG;
		return vaStatus;
	}

	obj_surface = SURFACE(obj_context->current_render_target);
	assert(obj_surface);

	/* verify that we got valid buffer references */
	sunxi_cedrus_msg("Picture with %d buffers\n", num_buffers);
	for(i = 0; i < num_buffers; i++)
	{
		object_buffer_p obj_buffer = BUFFER(buffers[i]);
		assert(obj_buffer);
		if (NULL == obj_buffer)
		{
			vaStatus = VA_STATUS_ERROR_INVALID_BUFFER;
			break;
		}

#ifdef OUTPUT_BUFFER_DATA_TO_FILE
		if (fp == -1) {
			fp = open(file_path, O_APPEND | O_CREAT | O_RDWR);
			if (fp < 0)
				sunxi_cedrus_msg("Could not open file\n");
		}
		if (fp >= 0) {
			buffer_size = (obj_buffer->size * obj_buffer->num_elements);
			sunxi_cedrus_msg("Add buffer with id %d, size %d, num_elements %d\n", 
				obj_buffer->type, buffer_size, obj_buffer->num_elements);
			write(fp, &obj_buffer->type, 4);
			write(fp, &buffer_size, 4);
			write(fp, &obj_buffer->num_elements, 4);
			ret = write(fp, obj_buffer->buffer_data, buffer_size);
			if (ret != buffer_size) {
				sunxi_cedrus_msg("ARGH! ret=%d, buffer_size=%d\n", ret, buffer_size);
				err(1, "ARGH! ret=%d, buffer_size=%d\n", ret, buffer_size);
			}
		}
#endif

		switch(obj_config->profile) {
			case VAProfileMPEG2Simple:
			case VAProfileMPEG2Main:
				if(obj_buffer->type == VASliceDataBufferType)
					vaStatus = sunxi_cedrus_render_mpeg2_slice_data(ctx, obj_context, obj_surface, obj_buffer);
				else if(obj_buffer->type == VAPictureParameterBufferType)
					vaStatus = sunxi_cedrus_render_mpeg2_picture_parameter(ctx, obj_context, obj_surface, obj_buffer);
				break;
			case VAProfileMPEG4Simple:
			case VAProfileMPEG4AdvancedSimple:
			case VAProfileMPEG4Main:
				if(obj_buffer->type == VASliceDataBufferType)
					vaStatus = sunxi_cedrus_render_mpeg4_slice_data(ctx, obj_context, obj_surface, obj_buffer);
				else if(obj_buffer->type == VAPictureParameterBufferType)
					vaStatus = sunxi_cedrus_render_mpeg4_picture_parameter(ctx, obj_context, obj_surface, obj_buffer);
				else if(obj_buffer->type == VASliceParameterBufferType)
					vaStatus = sunxi_cedrus_render_mpeg4_slice_parameter(ctx, obj_context, obj_surface, obj_buffer);
				break;
			default:
				break;
		}
	}

	return vaStatus;
}

VAStatus sunxi_cedrus_EndPicture(VADriverContextP ctx, VAContextID context)
{
	INIT_DRIVER_DATA
	VAStatus vaStatus = VA_STATUS_SUCCESS;
	object_context_p obj_context;
	object_surface_p obj_surface;
	struct v4l2_buffer out_buf, cap_buf;
	struct v4l2_plane plane[1];
	struct v4l2_plane planes[2];
	struct v4l2_ext_control ctrl;
	struct v4l2_ext_controls extCtrls;
	object_config_p obj_config;

	obj_context = CONTEXT(context);
	assert(obj_context);

	sunxi_cedrus_msg("%s current_render_target = %d\n", 
		__FUNCTION__, obj_context->current_render_target);
	obj_surface = SURFACE(obj_context->current_render_target);
	assert(obj_surface);

	obj_config = CONFIG(obj_context->config_id);
	if (NULL == obj_config)
	{
		vaStatus = VA_STATUS_ERROR_INVALID_CONFIG;
		return vaStatus;
	}

	/*
	 * The real rendering is done in EndPicture instead of RenderPicture
	 * because the v4l2 driver expects to have the full corresponding
	 * extended control when a buffer is queued and we don't know in which
	 * order the different RenderPicture will be called.
	 */

	memset(plane, 0, sizeof(struct v4l2_plane));
	memset(planes, 0, 2 * sizeof(struct v4l2_plane));

	memset(&(out_buf), 0, sizeof(out_buf));
	out_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	out_buf.memory = V4L2_MEMORY_MMAP;
	out_buf.index = obj_surface->input_buf_index;
	out_buf.length = 1;
	out_buf.m.planes = plane;
	out_buf.request = obj_surface->request;

	switch(obj_config->profile) {
		case VAProfileMPEG2Simple:
		case VAProfileMPEG2Main:
			sunxi_cedrus_msg("Using a VAProfileMPEG2*(simple/main\n");
			out_buf.m.planes[0].bytesused = obj_context->mpeg2_frame_hdr.slice_len/8;
			ctrl.id = V4L2_CID_MPEG_VIDEO_MPEG2_FRAME_HDR;
			ctrl.ptr = &obj_context->mpeg2_frame_hdr;
			ctrl.size = sizeof(obj_context->mpeg2_frame_hdr);
			break;
		case VAProfileMPEG4Simple:
		case VAProfileMPEG4AdvancedSimple:
		case VAProfileMPEG4Main:
			sunxi_cedrus_msg("Using a VAProfileMPEG4*(simple/advancedsimple/main\n");
			out_buf.m.planes[0].bytesused = obj_context->mpeg4_frame_hdr.slice_len/8;			
			ctrl.id = V4L2_CID_MPEG_VIDEO_MPEG4_FRAME_HDR;
			ctrl.ptr = &obj_context->mpeg4_frame_hdr;
			ctrl.size = sizeof(obj_context->mpeg4_frame_hdr);
			break;
		default:
			sunxi_cedrus_msg("Error, this is an unkonw VAProfile\n");
			out_buf.m.planes[0].bytesused = 0;
			ctrl.id = V4L2_CID_MPEG_VIDEO_MPEG2_FRAME_HDR;
			ctrl.ptr = NULL;
			ctrl.size = 0;
			break;
	}

	memset(&(cap_buf), 0, sizeof(cap_buf));
	cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	cap_buf.memory = V4L2_MEMORY_MMAP;
	cap_buf.index = obj_surface->output_buf_index;
	cap_buf.length = 2;
	cap_buf.m.planes = planes;

	assert(ioctl(driver_data->mem2mem_fd, VIDIOC_QUERYBUF, &cap_buf)==0);

	extCtrls.controls = &ctrl;
	extCtrls.count = 1;
	extCtrls.request = obj_surface->request;
	assert(ioctl(driver_data->mem2mem_fd, VIDIOC_S_EXT_CTRLS, &extCtrls)==0);

	if(ioctl(driver_data->mem2mem_fd, VIDIOC_QBUF, &cap_buf)) {
		obj_surface->status = VASurfaceSkipped;
		sunxi_cedrus_msg("Error when queuing output: %s\n", strerror(errno));
		return VA_STATUS_ERROR_UNKNOWN;
	}
	if(ioctl(driver_data->mem2mem_fd, VIDIOC_QBUF, &out_buf)) {
		obj_surface->status = VASurfaceSkipped;
		sunxi_cedrus_msg("Error when queuing input: %s\n", strerror(errno));
		ioctl(driver_data->mem2mem_fd, VIDIOC_DQBUF, &cap_buf);
		return VA_STATUS_ERROR_UNKNOWN;
	}

	/* For now, assume that we are done with rendering right away */
	obj_context->current_render_target = -1;

	return vaStatus;
}



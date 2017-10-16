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
#include "image.h"
#include "surface.h"
#include "buffer.h"

#include <assert.h>
#include <string.h>

#include "tiled_yuv.h"

/*
 * An Image is a standard data structure containing rendered frames in a usable
 * pixel format. Here we only use NV12 buffers which are converted from sunxi's
 * proprietary tiled pixel format with tiled_yuv when deriving an Image from a
 * Surface.
 */
/* Thomas: todo add support for XRGB8888
 * From ARMSOC: "Initialized a depth-32 visual for XRGB8888"
 * We want to be able to output the format reported by ARMSOC
 * Which means the surface should become this format.
 * Internal yuv420 buffers are already allocated by this driver.
 * Add additional buffers for the color space conversion from yuv420
 * to xrgb8888.
 */

VAStatus sunxi_cedrus_QueryImageFormats(VADriverContextP ctx,
		VAImageFormat *format_list, int *num_formats)
{
	sunxi_cedrus_msg("%s();\n", __FUNCTION__);
	format_list[0].fourcc = VA_FOURCC_XRGB;	
	format_list[1].fourcc = VA_FOURCC_NV12;
	*num_formats = 2;
	return VA_STATUS_SUCCESS;
}

VAStatus sunxi_cedrus_CreateImage(VADriverContextP ctx, VAImageFormat *format,
		int width, int height, VAImage *image)
{
	INIT_DRIVER_DATA
	int sizeY, sizeUV;
	object_image_p obj_img;

	sunxi_cedrus_msg("%s();\n", __FUNCTION__);

	image->format = *format;
	image->buf = VA_INVALID_ID;
	image->width = width;
	image->height = height;

	sunxi_cedrus_msg("Creating an image in sunxi_cedrus_CreateImage\n");
	sunxi_cedrus_msg("\tformat->fourcc 0x%x (%d)\n", format->fourcc, format->fourcc);
	sunxi_cedrus_msg("\tformat->byte_order 0x%x (%d)\n", format->byte_order, format->byte_order);
	sunxi_cedrus_msg("\tformat->bits_per_pixel 0x%x (%d)\n", format->bits_per_pixel, format->bits_per_pixel);
	sunxi_cedrus_msg("\tformat->depth 0x%x (%d)\n", format->depth, format->depth);
	sunxi_cedrus_msg("\tformat->red_mask 0x%x (%d)\n", format->red_mask, format->red_mask);
	sunxi_cedrus_msg("\tformat->green_mask 0x%x (%d)\n", format->green_mask, format->green_mask);
	sunxi_cedrus_msg("\tformat->blue_mask 0x%x (%d)\n", format->blue_mask, format->blue_mask);

	sunxi_cedrus_msg("\twidth = %d, heigt = %d\n", width, height);
	
	sizeY    = image->width * image->height;
	sizeUV   = ((image->width+1) * (image->height+1)/2);

	image->num_planes = 2;
	image->pitches[0] = (image->width+31)&~31;
	image->pitches[1] = (image->width+31)&~31;
	image->offsets[0] = 0;
	image->offsets[1] = sizeY;
	image->data_size  = sizeY + sizeUV;

	image->image_id = object_heap_allocate(&driver_data->image_heap);
	if (image->image_id == VA_INVALID_ID)
		return VA_STATUS_ERROR_ALLOCATION_FAILED;
	obj_img = IMAGE(image->image_id);

	if (sunxi_cedrus_CreateBuffer(ctx, 0, VAImageBufferType, image->data_size,
	    1, NULL, &image->buf) != VA_STATUS_SUCCESS)
		return VA_STATUS_ERROR_ALLOCATION_FAILED;
	obj_img->buf = image->buf;

	return VA_STATUS_SUCCESS;
}

VAStatus sunxi_cedrus_DeriveImage(VADriverContextP ctx, VASurfaceID surface,
		VAImage *image)
{
	INIT_DRIVER_DATA
	object_surface_p obj_surface;
	VAImageFormat fmt;
	object_buffer_p obj_buffer;
	VAStatus ret;

	sunxi_cedrus_msg("%s();\n", __FUNCTION__);

	obj_surface = SURFACE(surface);
//	fmt.fourcc = VA_FOURCC_NV12;
	fmt.fourcc = VA_FOURCC_XRGB;

	ret = sunxi_cedrus_CreateImage(ctx, &fmt, obj_surface->width,
			obj_surface->height, image);
	if(ret != VA_STATUS_SUCCESS)
		return ret;

	obj_buffer = BUFFER(image->buf);
	if (NULL == obj_buffer)
		return VA_STATUS_ERROR_ALLOCATION_FAILED;

	uint32_t i;
	uint32_t color = 0xffff0000;
	
	for (i = 0; i < 100*100; i++) {
		memcpy(driver_data->luma_bufs[obj_surface->output_buf_index], &color, 4);
	}

	/* TODO: Use an appropriate DRM plane instead */
	tiled_to_planar(driver_data->luma_bufs[obj_surface->output_buf_index], obj_buffer->buffer_data, image->pitches[0], image->width, image->height);
	tiled_to_planar(driver_data->chroma_bufs[obj_surface->output_buf_index], obj_buffer->buffer_data + image->width*image->height, image->pitches[1], image->width, image->height/2);

	//This will detile the buffers
	//Now send the buffers here to the mixer processor.


	return VA_STATUS_SUCCESS;
}

VAStatus sunxi_cedrus_DestroyImage(VADriverContextP ctx, VAImageID image)
{
	INIT_DRIVER_DATA
	object_image_p obj_img;

	sunxi_cedrus_msg("%s();\n", __FUNCTION__);

	obj_img = IMAGE(image);
	assert(obj_img);

	sunxi_cedrus_DestroyBuffer(ctx, obj_img->buf);
	return VA_STATUS_SUCCESS;
}

VAStatus sunxi_cedrus_SetImagePalette(VADriverContextP ctx, VAImageID image,
		unsigned char *palette)
{ return VA_STATUS_SUCCESS; }

VAStatus sunxi_cedrus_GetImage(VADriverContextP ctx, VASurfaceID surface,
		int x, int y, unsigned int width, unsigned int height,
		VAImageID image)
{ return VA_STATUS_SUCCESS; }

VAStatus sunxi_cedrus_PutImage(VADriverContextP ctx, VASurfaceID surface,
		VAImageID image, int src_x, int src_y, unsigned int src_width,
		unsigned int src_height, int dest_x, int dest_y,
		unsigned int dest_width, unsigned int dest_height)
{ return VA_STATUS_SUCCESS; }

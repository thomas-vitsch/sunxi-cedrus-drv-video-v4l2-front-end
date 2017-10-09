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
#include "buffer.h"
#include "context.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

/*
 * A Buffer is a memory zone used to handle all kind of data, for example an IQ
 * matrix or image buffer (which are allocated using realloc) or slice data
 * (which are mmapped from v4l's kernel space)
 */

VAStatus sunxi_cedrus_CreateBuffer(VADriverContextP ctx, VAContextID context,
		VABufferType type, unsigned int size, unsigned int num_elements,
		void *data, VABufferID *buf_id)
{
	INIT_DRIVER_DATA
	VAStatus vaStatus = VA_STATUS_SUCCESS;
	int bufferID;
	struct v4l2_plane plane[1];
	object_buffer_p obj_buffer;

	memset(plane, 0, sizeof(struct v4l2_plane));

	sunxi_cedrus_msg("sunxi_cedrus_CreateBuffer called with num_elements %d and data_ptr%p\n", 
		num_elements, data);

	/* Validate type */
	switch (type)
	{
		case VAPictureParameterBufferType:
			sunxi_cedrus_msg("create buffer : VAPictureParameterBufferType\n");
			break;
		case VAIQMatrixBufferType: /* Ignored */
			sunxi_cedrus_msg("create buffer : VAIQMatrixBufferType\n");
			break;
		case VASliceParameterBufferType:
			sunxi_cedrus_msg("create buffer : VASliceParameterBufferType\n");
			break;
		case VASliceDataBufferType:
			sunxi_cedrus_msg("create buffer : VASliceDataBufferType\n");
			break;
		case VAImageBufferType:
			sunxi_cedrus_msg("create buffer : VAImageBufferType\n");
			/* Ok */
			break;
		default:
			sunxi_cedrus_msg("create buffer : unsupported\n");
			vaStatus = VA_STATUS_ERROR_UNSUPPORTED_BUFFERTYPE;
			return vaStatus;
	}

	bufferID = object_heap_allocate(&driver_data->buffer_heap);
	obj_buffer = BUFFER(bufferID);
	if (NULL == obj_buffer)
	{
		sunxi_cedrus_msg("VA_STATUS_ERROR_ALLOCATION_FAILED 1\n");
		vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
		return vaStatus;
	}

	obj_buffer->buffer_data = NULL;
	obj_buffer->type = type;

	if(obj_buffer->type == VASliceDataBufferType) {
		object_context_p obj_context;

		obj_context = CONTEXT(context);
		assert(obj_context);

		struct v4l2_buffer buf;
		memset(&(buf), 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = obj_context->num_rendered_surfaces%INPUT_BUFFERS_NB;
		buf.length = 1;
		buf.m.planes = plane;

		assert(ioctl(driver_data->mem2mem_fd, VIDIOC_QUERYBUF, &buf)==0);

		sunxi_cedrus_msg("obj_buffer->buffer_data mmap with size \
		    * num_elements = %d\n", size * num_elements);
		sunxi_cedrus_msg("obj_buffer.length = %d\n", buf.length);
		sunxi_cedrus_msg("plane_size which is max data bytes the plane will contain is \
		     = %d\n", plane->length);
		    	    
		obj_buffer->buffer_data = mmap(NULL, size * num_elements,
				PROT_READ | PROT_WRITE, MAP_SHARED,
				driver_data->mem2mem_fd, buf.m.planes[0].m.mem_offset);
	} else
		obj_buffer->buffer_data = realloc(obj_buffer->buffer_data, size * num_elements);

	if (obj_buffer->buffer_data == NULL) {
		sunxi_cedrus_msg("VA_STATUS_ERROR_ALLOCATION_FAILED 2\n");
		return VA_STATUS_ERROR_ALLOCATION_FAILED;
	}

	if (VA_STATUS_SUCCESS == vaStatus)
	{
		obj_buffer->max_num_elements = num_elements;
		obj_buffer->num_elements = num_elements;
		obj_buffer->size = size;

		sunxi_cedrus_msg("Thomas: data size = %d\n", size);

		if (data) {
			/* Thomas 
			 * If this check fails look at src/context.h:37
			 * #define INPUT_BUFFER_MAX_SIZE
			 * this define may be increased to allow for 
			 * bigger data buffers 
			 * This assert is here to give a better error than
			 * a Segmentation fault which shows an error in gdb
			 * with the memcpy library.
			 */
			if (type == VASliceDataBufferType)
				assert(size <= plane->length);
				
			memcpy(obj_buffer->buffer_data, data,
					size * num_elements);
		}
		else
			sunxi_cedrus_msg("Data == NULL\n");
	}
	

	if (VA_STATUS_SUCCESS == vaStatus)
		*buf_id = bufferID;

	sunxi_cedrus_msg("End of %s\n", __FUNCTION__);

	return vaStatus;
}

VAStatus sunxi_cedrus_BufferSetNumElements(VADriverContextP ctx,
		VABufferID buf_id, unsigned int num_elements)
{
	INIT_DRIVER_DATA
	VAStatus vaStatus = VA_STATUS_SUCCESS;
	object_buffer_p obj_buffer = BUFFER(buf_id);
	assert(obj_buffer);

	sunxi_cedrus_msg("sunxi_cedrus_BufferSetNumElements: num_elements = %d \
		for buffer_id %d\n", num_elements, buf_id); 

	if ((num_elements < 0) || (num_elements > obj_buffer->max_num_elements))
		vaStatus = VA_STATUS_ERROR_UNKNOWN;
	if (VA_STATUS_SUCCESS == vaStatus)
		obj_buffer->num_elements = num_elements;

	return vaStatus;
}

VAStatus sunxi_cedrus_MapBuffer(VADriverContextP ctx, VABufferID buf_id,
		void **pbuf)
{
	INIT_DRIVER_DATA
	VAStatus vaStatus = VA_STATUS_ERROR_UNKNOWN;
	object_buffer_p obj_buffer = BUFFER(buf_id);
	assert(obj_buffer);
	
	sunxi_cedrus_msg("sunxi_cedrus_MapBuffer\n");
	
	if (NULL == obj_buffer)
	{
		vaStatus = VA_STATUS_ERROR_INVALID_BUFFER;
		return vaStatus;
	}

	if (NULL != obj_buffer->buffer_data)
	{
		*pbuf = obj_buffer->buffer_data;
		vaStatus = VA_STATUS_SUCCESS;
	}
	return vaStatus;
}

VAStatus sunxi_cedrus_UnmapBuffer(VADriverContextP ctx, VABufferID buf_id)
{
	/* Do nothing */
	sunxi_cedrus_msg("sunxi_cedrus_UnmapBuffer\n");
	return VA_STATUS_SUCCESS;
}

void sunxi_cedrus_destroy_buffer(struct sunxi_cedrus_driver_data *driver_data,
		object_buffer_p obj_buffer)
{
	if (NULL != obj_buffer->buffer_data)
	{
		if(obj_buffer->type == VASliceDataBufferType)
			munmap(obj_buffer->buffer_data, obj_buffer->size);
		else
			free(obj_buffer->buffer_data);

		obj_buffer->buffer_data = NULL;
	}

	object_heap_free(&driver_data->buffer_heap, (object_base_p) obj_buffer);
}

VAStatus sunxi_cedrus_DestroyBuffer(VADriverContextP ctx, VABufferID buffer_id)
{
	INIT_DRIVER_DATA
	object_buffer_p obj_buffer = BUFFER(buffer_id);
	assert(obj_buffer);

	sunxi_cedrus_destroy_buffer(driver_data, obj_buffer);
	return VA_STATUS_SUCCESS;
}

/* sunxi-cedrus doesn't support buffer info */
VAStatus sunxi_cedrus_BufferInfo(VADriverContextP ctx, VABufferID buf_id,
		VABufferType *type, unsigned int *size,
		unsigned int *num_elements)
{ return VA_STATUS_ERROR_UNIMPLEMENTED; }

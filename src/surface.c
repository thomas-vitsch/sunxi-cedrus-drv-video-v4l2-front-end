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
#include "surface.h"
#include "fe.h" 

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

#include <X11/Xlib.h>
#include <misc/sunxi_front_end.h>

/*
 * A Surface is an internal data structure never handled by the VA's user
 * containing the output of a rendering. Usualy, a bunch of surfaces are created
 * at the begining of decoding and they are then used alternatively. When
 * created, a surface is assigned a corresponding v4l capture buffer and it is
 * kept until the end of decoding. Syncing a surface waits for the v4l buffer to
 * be available and then dequeue it.
 *
 * Note: since a Surface is kept private from the VA's user, it can ask to
 * directly render a Surface on screen in an X Drawable. Some kind of
 * implementation is available in PutSurface but this is only for development
 * purpose.
 */

VAStatus sunxi_cedrus_CreateSurfaces(VADriverContextP ctx, int width,
		int height, int format, int num_surfaces, VASurfaceID *surfaces)
{
	INIT_DRIVER_DATA
	VAStatus vaStatus = VA_STATUS_SUCCESS;
	int i;
	struct v4l2_buffer buf;
	struct v4l2_plane planes[2];
	struct v4l2_create_buffers create_bufs;
	struct v4l2_format fmt;

	sunxi_cedrus_msg("%s();\n", __FUNCTION__);

	sunxi_cedrus_msg("CreateSurface num_surfaces %d with width %d, height %d\n",
		num_surfaces, width, height);

	memset(planes, 0, 2 * sizeof(struct v4l2_plane));

	/* We only support one format */
	if (VA_RT_FORMAT_YUV420 != format) 
		return VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT;

	/* Set format for capture */
	memset(&(fmt), 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	fmt.fmt.pix_mp.width = width;
	fmt.fmt.pix_mp.height = height;
	fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_SUNXI;
	fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
	fmt.fmt.pix_mp.num_planes = 2;
	assert(ioctl(driver_data->mem2mem_fd, VIDIOC_S_FMT, &fmt)==0);
	
	memset (&create_bufs, 0, sizeof (struct v4l2_create_buffers));
	create_bufs.count = num_surfaces;
	create_bufs.memory = V4L2_MEMORY_MMAP;
	create_bufs.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	assert(ioctl(driver_data->mem2mem_fd, VIDIOC_G_FMT, &create_bufs.format)==0);

	assert(ioctl(driver_data->mem2mem_fd, VIDIOC_CREATE_BUFS, &create_bufs)==0);
	driver_data->num_dst_bufs = create_bufs.count;

	for (i = 0; i < create_bufs.count; i++)
	{
		int surfaceID = object_heap_allocate(&driver_data->surface_heap);
		object_surface_p obj_surface = SURFACE(surfaceID);
		if (NULL == obj_surface)
		{
			vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
			break;
		}
		obj_surface->surface_id = surfaceID;
		surfaces[i] = surfaceID;

		memset(&(buf), 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = create_bufs.index + i;
		buf.length = 2;
		buf.m.planes = planes;

		assert(ioctl(driver_data->mem2mem_fd, VIDIOC_QUERYBUF, &buf)==0);

		driver_data->luma_bufs[buf.index] = mmap(NULL, buf.m.planes[0].length,
				PROT_READ | PROT_WRITE, MAP_SHARED,
				driver_data->mem2mem_fd, buf.m.planes[0].m.mem_offset);
		assert(driver_data->luma_bufs[buf.index] != MAP_FAILED);

		driver_data->chroma_bufs[buf.index] = mmap(NULL, buf.m.planes[1].length,
				PROT_READ | PROT_WRITE, MAP_SHARED,
				driver_data->mem2mem_fd, buf.m.planes[1].m.mem_offset);
		assert(driver_data->chroma_bufs[buf.index] != MAP_FAILED);

		obj_surface->input_buf_index = 0;
		obj_surface->output_buf_index = create_bufs.index + i;

		obj_surface->width = width;
		obj_surface->height = height;
		obj_surface->status = VASurfaceReady;
	}


	/* Error recovery */
	if (VA_STATUS_SUCCESS != vaStatus)
	{
		/* surfaces[i-1] was the last successful allocation */
		for(; i--;)
		{
			object_surface_p obj_surface = SURFACE(surfaces[i]);
			surfaces[i] = VA_INVALID_SURFACE;
			assert(obj_surface);
			object_heap_free(&driver_data->surface_heap, (object_base_p) obj_surface);
		}
	}
	
	sunxi_cedrus_fe_test(ctx);	
	return vaStatus;
}

VAStatus sunxi_cedrus_DestroySurfaces(VADriverContextP ctx,
		VASurfaceID *surface_list, int num_surfaces)
{
	INIT_DRIVER_DATA
	int i;
	
	sunxi_cedrus_msg("%s();\n", __FUNCTION__);
	
	for(i = num_surfaces; i--;)
	{
		object_surface_p obj_surface = SURFACE(surface_list[i]);
		assert(obj_surface);
		object_heap_free(&driver_data->surface_heap, (object_base_p) obj_surface);
	}
	return VA_STATUS_SUCCESS;
}

VAStatus sunxi_cedrus_SyncSurface(VADriverContextP ctx,
		VASurfaceID render_target)
{
	INIT_DRIVER_DATA
	object_surface_p obj_surface;
	struct v4l2_buffer buf;
	struct v4l2_plane plane[1];
	struct v4l2_plane planes[2];
        fd_set read_fds;
	struct timeval tv = {0, 300000};
	
	sunxi_cedrus_msg("%s();\n", __FUNCTION__);

	memset(plane, 0, sizeof(struct v4l2_plane));
	memset(planes, 0, 2 * sizeof(struct v4l2_plane));

	obj_surface = SURFACE(render_target);
	assert(obj_surface);

	FD_ZERO(&read_fds);
	FD_SET(driver_data->mem2mem_fd, &read_fds);
	if(obj_surface->status != VASurfaceSkipped)
		select(driver_data->mem2mem_fd + 1, &read_fds, NULL, NULL, &tv);
	else
		return VA_STATUS_ERROR_UNKNOWN;

	//Test if we can throw the buffer back to the Allwinner Display Engine 
	// Frontend Kernel Module.
//	assert(ioctl(driver_data->de_frontend_fd, SFE_IOCTL_UPDATE_BUFFER, &buf)==0);
//	assert(ioctl(driver_data->mem2mem_output_fd, VIDIOC_QBUF, &buf)==0);

	memset(&(buf), 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = obj_surface->input_buf_index;
	buf.length = 1;
	buf.m.planes = plane;

	if(ioctl(driver_data->mem2mem_fd, VIDIOC_DQBUF, &buf)) {
		sunxi_cedrus_msg("Error when dequeuing input: %s\n", strerror(errno));
		return VA_STATUS_ERROR_UNKNOWN;
	}

	sunxi_cedrus_msg("\n\nDequeued V4L2_OUT_BUFFER\n");
	sunxi_cedrus_msg("\tbuf.index = %d, buf.m.planes[0].bytesused %d \n", 
		buf.index, buf.m.planes[0].bytesused);

	memset(&(buf), 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = obj_surface->output_buf_index;
	buf.length = 2;
	buf.m.planes = planes;

	obj_surface->status = VASurfaceReady;

//	if(buf.index == 2)
	sunxi_cedrus_fe_throw_buffer(ctx, buf);


	if(ioctl(driver_data->mem2mem_fd, VIDIOC_DQBUF, &buf)) {
		sunxi_cedrus_msg("Error when dequeuing output: %s\n", strerror(errno));
		return VA_STATUS_ERROR_UNKNOWN;
	}

	sunxi_cedrus_msg("\n\nDequeued V4L2_CAP_BUFFER\n");
	
	int p;
	for (p = 0; p < 2; p++) {
		//Do we have a decoded frame here?????
		sunxi_cedrus_msg("\t plane %d, buf.index = %d, .bytesused %d, .length %d, .m.mem_offset %d, .m.userptr %p, .m.fd %d, .m.data_offset %d \n", p,
			buf.index, buf.m.planes[p].bytesused, buf.m.planes[p].length, buf.m.planes[p].m.mem_offset, buf.m.planes[p].m.userptr, buf.m.planes[p].m.fd, buf.m.planes[p].data_offset);
	}
	
	
	return VA_STATUS_SUCCESS;
}

VAStatus sunxi_cedrus_QuerySurfaceStatus(VADriverContextP ctx,
		VASurfaceID render_target, VASurfaceStatus *status)
{
	INIT_DRIVER_DATA
	VAStatus vaStatus = VA_STATUS_SUCCESS;
	object_surface_p obj_surface;

	sunxi_cedrus_msg("%s();\n", __FUNCTION__);
	obj_surface = SURFACE(render_target);
	assert(obj_surface);

	*status = obj_surface->status;

	return vaStatus;
}

/* WARNING: This is for development purpose only!!! */
VAStatus sunxi_cedrus_PutSurface(VADriverContextP ctx, VASurfaceID surface,
		void *draw, short srcx, short srcy, unsigned short srcw,
		unsigned short srch, short destx, short desty,
		unsigned short destw, unsigned short desth,
		VARectangle *cliprects, unsigned int number_cliprects,
		unsigned int flags)
{
	INIT_DRIVER_DATA
	GC gc;
	Display *display;
	const XID xid = (XID)(uintptr_t)draw;
	XColor xcolor;
	int screen;
	Colormap cm;
	int colorratio = 65535 / 255;
	int x, y;
	object_surface_p obj_surface;

	sunxi_cedrus_msg("%s();\n", __FUNCTION__);

	obj_surface = SURFACE(surface);
	assert(obj_surface);

	display = XOpenDisplay(getenv("DISPLAY"));
	if (display == NULL) {
		sunxi_cedrus_msg("Cannot connect to X server\n");
		exit(1);
	}

	sunxi_cedrus_msg("warning: using vaPutSurface with sunxi-cedrus is not recommended\n");
	screen = DefaultScreen(display);
	gc =  XCreateGC(display, RootWindow(display, screen), 0, NULL);
	XSync(display, False);

	cm = DefaultColormap(display, screen);
	xcolor.flags = DoRed | DoGreen | DoBlue;

	for(x=destx; x < destx+destw; x++) {
		for(y=desty; y < desty+desth; y++) {
			char lum = driver_data->luma_bufs[obj_surface->output_buf_index][x+srcw*y];
			xcolor.red = xcolor.green = xcolor.blue = lum*colorratio;
			XAllocColor(display, cm, &xcolor);
			XSetForeground(display, gc, xcolor.pixel);
			XDrawPoint(display, xid, gc, x, y);
		}
	}

	XFlush(display);
	XCloseDisplay(display);
	return VA_STATUS_SUCCESS;
}

VAStatus sunxi_cedrus_LockSurface(VADriverContextP ctx, VASurfaceID surface,
		unsigned int *fourcc, unsigned int *luma_stride,
		unsigned int *chroma_u_stride, unsigned int *chroma_v_stride,
		unsigned int *luma_offset, unsigned int *chroma_u_offset,
		unsigned int *chroma_v_offset, unsigned int *buffer_name,
		void **buffer)
{ return VA_STATUS_ERROR_UNIMPLEMENTED; }

VAStatus sunxi_cedrus_UnlockSurface(VADriverContextP ctx, VASurfaceID surface)
{ return VA_STATUS_ERROR_UNIMPLEMENTED; }

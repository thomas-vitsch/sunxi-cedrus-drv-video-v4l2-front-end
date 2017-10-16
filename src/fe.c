
#include "fe.h"
#include "sunxi_cedrus_drv_video.h"
#include "context.h"

#include <linux/videodev2.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <assert.h>
#include <va/va_backend.h>

void create_capture_buffers(VADriverContextP ctx)
{
	struct v4l2_format fmt;
	struct v4l2_create_buffers create_bufs;
	struct v4l2_buffer buf;
	struct v4l2_plane planes[2];

	INIT_DRIVER_DATA

	//Set capture format	
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	fmt.fmt.pix_mp.width = 1920;
	fmt.fmt.pix_mp.height = 1080;
	fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_SUNXI;
	fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
	fmt.fmt.pix_mp.num_planes = 2;
	assert(ioctl(driver_data->mem2mem_output_fd, VIDIOC_S_FMT, &fmt)==0);

	//Create capture buffers
	memset(&create_bufs, 0, sizeof(struct v4l2_create_buffers));
	create_bufs.count = 1; 
	create_bufs.memory = V4L2_MEMORY_MMAP;
	create_bufs.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	assert(ioctl(driver_data->mem2mem_output_fd, VIDIOC_G_FMT, &create_bufs.format)==0);
	assert(ioctl(driver_data->mem2mem_output_fd, VIDIOC_CREATE_BUFS, &create_bufs)==0);	

	//Query the status of the buffer.
	memset(&buf, 0, sizeof(struct v4l2_buffer));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = 0;
	buf.length = 2; //Length? Is this Y & UV? Planes?
	buf.m.planes = planes;
	assert(ioctl(driver_data->mem2mem_output_fd, VIDIOC_QUERYBUF, &buf)==0);
	//We could mmap this buffer now and fill it with data if we wanted to.	
	//////// Pointer naar dit buffer ophalen.
	////driver_data->luma_bufs[buf.index] = mmap(NULL, buf.m.planes[0].length,
	////    PROT_READ | PROT_WRITE, MAP_SHARED,
        ////    driver_data->mem2mem_fd, buf.m.planes[0].m.mem_offset);
	////assert(driver_data->luma_bufs[buf.index] != MAP_FAILED);
	////////
}

void test_req_bufs(VADriverContextP ctx){
	struct v4l2_requestbuffers req_bufs;

	INIT_DRIVER_DATA

	struct v4l2_format fmt;

	//Set capture format	
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	fmt.fmt.pix_mp.width = 1920;
	fmt.fmt.pix_mp.height = 1080;
	fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_SUNXI;
	fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
	fmt.fmt.pix_mp.num_planes = 2;
	fmt.fmt.pix_mp.plane_fmt[0].sizeimage = INPUT_BUFFER_MAX_SIZE;
	assert(ioctl(driver_data->mem2mem_output_fd, VIDIOC_S_FMT, &fmt)==0);

	memset(&req_bufs, 0, sizeof(struct v4l2_requestbuffers));
	req_bufs.count = 1;
	req_bufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	req_bufs.memory = V4L2_MEMORY_USERPTR;
	assert(ioctl(driver_data->mem2mem_output_fd, VIDIOC_REQBUFS, &req_bufs)==0);
}

void create_output_buffers(VADriverContextP ctx)
{
	struct v4l2_format fmt;
	struct v4l2_create_buffers create_bufs;
	struct v4l2_buffer buf;
	struct v4l2_plane planes[2];

	INIT_DRIVER_DATA

	//Set capture format	
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	fmt.fmt.pix_mp.width = 1920;
	fmt.fmt.pix_mp.height = 1080;
	fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_SUNXI;
	fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
	fmt.fmt.pix_mp.num_planes = 2;
	fmt.fmt.pix_mp.plane_fmt[0].sizeimage = INPUT_BUFFER_MAX_SIZE;
	assert(ioctl(driver_data->mem2mem_output_fd, VIDIOC_S_FMT, &fmt)==0);

	//Create capture buffers
	memset(&create_bufs, 0, sizeof(struct v4l2_create_buffers));
	create_bufs.count = 1; 
	create_bufs.memory = V4L2_MEMORY_MMAP;
	create_bufs.format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	assert(ioctl(driver_data->mem2mem_output_fd, VIDIOC_G_FMT, &create_bufs.format)==0);
	assert(ioctl(driver_data->mem2mem_output_fd, VIDIOC_CREATE_BUFS, &create_bufs)==0);	

	//Query the status of the buffer.
	memset(&buf, 0, sizeof(struct v4l2_buffer));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = 0;
	buf.length = 2; //Length? Is this Y & UV? Planes?
	buf.m.planes = planes;
	assert(ioctl(driver_data->mem2mem_output_fd, VIDIOC_QUERYBUF, &buf)==0);
	//We could mmap this buffer now and fill it with data if we wanted to.	
	//////// Pointer naar dit buffer ophalen.
	////driver_data->luma_bufs[buf.index] = mmap(NULL, buf.m.planes[0].length,
	////    PROT_READ | PROT_WRITE, MAP_SHARED,
        ////    driver_data->mem2mem_fd, buf.m.planes[0].m.mem_offset);
	////assert(driver_data->luma_bufs[buf.index] != MAP_FAILED);
	////////
}

int sunxi_cedrus_fe_test(VADriverContextP ctx)
{
	enum v4l2_buf_type buf_type;

	INIT_DRIVER_DATA

	//query of capabilities is aready done in sunxi_cedrus_drv_video.c:244

	//Capture == let a video_device capture frames into the capture buffers.
	create_capture_buffers(ctx);
	
	//Output == is input for the video device.
//	create_output_buffers(ctx);
	test_req_bufs(ctx);

	
	//The buffers are created.
	//Start streaming.
	//Whenever the cedrus driver has processed something queue it to the frontend???
	buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	assert(ioctl(driver_data->mem2mem_output_fd, VIDIOC_STREAMON, &buf_type)==0);

	buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	assert(ioctl(driver_data->mem2mem_output_fd, VIDIOC_STREAMON, &buf_type)==0);


	//Okay we are streaming now, how do I throw an existing buffer to it.
		

	
	return 0;
}


/* Applications call the VIDIOC_QBUF ioctl to enqueue an empty (capturing) 
 * or filled (output) buffer in the driver's incoming queue. The semantics 
 * depend on the selected I/O method.
 *
 * To enqueue a memory mapped buffer applications set the type field of a 
 * struct v4l2_buffer to the same buffer type as previously struct v4l2_format 
 * type and struct v4l2_requestbuffers type, the memory field to 
 * V4L2_MEMORY_MMAP and the index field. Valid index numbers range from zero to 
 * the number of buffers allocated with 
 * VIDIOC_REQBUFS (struct v4l2_requestbuffers count) minus one. The contents of 
 * the struct v4l2_buffer returned by a VIDIOC_QUERYBUF ioctl will do as well. 
 * When the buffer is intended for output (type is V4L2_BUF_TYPE_VIDEO_OUTPUT 
 * or V4L2_BUF_TYPE_VBI_OUTPUT) applications must also initialize the bytesused, 
 * field and timestamp fields. See Section 3.5, “Buffers” for details. 
 * When VIDIOC_QBUF is called with a pointer to this structure the driver sets 
 * the V4L2_BUF_FLAG_MAPPED and V4L2_BUF_FLAG_QUEUED flags and clears the 
 * V4L2_BUF_FLAG_DONE flag in the flags field, or it returns an EINVAL error 
 * code.
 *
 * To enqueue a user pointer buffer applications set the type field of a struct 
 * v4l2_buffer to the same buffer type as previously struct v4l2_format type 
 * and struct v4l2_requestbuffers type, the memory field to V4L2_MEMORY_USERPTR 
 * and the m.userptr field to the address of the buffer and length to its size. 
 * When the buffer is intended for output additional fields must be set as 
 * above. When VIDIOC_QBUF is called with a pointer to this structure the driver 
 * sets the V4L2_BUF_FLAG_QUEUED flag and clears the V4L2_BUF_FLAG_MAPPED and 
 * V4L2_BUF_FLAG_DONE flags in the flags field, or it returns an error code. 
 * This ioctl locks the memory pages of the buffer in physical memory, they 
 * cannot be swapped out to disk. Buffers remain locked until dequeued, until 
 * the VIDIOC_STREAMOFF or VIDIOC_REQBUFS ioctl are called, or until the device 
 * is closed.
 *
 * Applications call the VIDIOC_DQBUF ioctl to dequeue a filled (capturing) or 
 * displayed (output) buffer from the driver's outgoing queue. They just set 
 * the type and memory fields of a struct v4l2_buffer as above, when 
 * VIDIOC_DQBUF is called with a pointer to this structure the driver fills 
 * the remaining fields or returns an error code.
 *
 * By default VIDIOC_DQBUF blocks when no buffer is in the outgoing queue. 
 * When the O_NONBLOCK flag was given to the open() function, VIDIOC_DQBUF 
 * returns immediately with an EAGAIN error code when no buffer is available.
 *
 * The v4l2_buffer structure is specified in Section 3.5, “Buffers”.
 *
 * https://linuxtv.org/downloads/legacy/video4linux/API/V4L2_API/spec/rn01re54.html 
 */
 
 
int sunxi_cedrus_fe_throw_buffer(VADriverContextP ctx, struct v4l2_buffer cedrus_cap_buf)
{
	struct v4l2_buffer out_buf, cap_buf;
	struct v4l2_plane planes[2], planes_out[2];

	INIT_DRIVER_DATA

	memset(planes, 0, 2 * sizeof(struct v4l2_plane));

	memset(&out_buf, 0, sizeof(struct v4l2_buffer));
	out_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	out_buf.memory = V4L2_MEMORY_USERPTR;
	out_buf.index = 0;
	out_buf.length = 2;
	out_buf.m.planes = planes;
//	out_buf.m.userptr = driver_data->luma_bufs[cedrus_cap_buf.index];
//	out_buf.m.fd = cap_buf.m.fd;
	
	
//	out_buf.request = obj_surface->request; /* ?????? */
	out_buf.m.planes[0].bytesused = cedrus_cap_buf.m.planes[0].bytesused;
	out_buf.m.planes[1].bytesused = cedrus_cap_buf.m.planes[1].bytesused;

//	out_buf.m.planes[0].m.fd = cedrus_cap_buf.m.planes[0].m.fd;
//	out_buf.m.planes[1].m.fd = cedrus_cap_buf.m.planes[0].m.fd;
	out_buf.m.planes[0].m.userptr = (unsigned long)driver_data->luma_bufs[cedrus_cap_buf.index];
	out_buf.m.planes[1].m.userptr = (unsigned long)driver_data->chroma_bufs[cedrus_cap_buf.index];

	/* The minimum plane length is checked against "a" value. which 
	 * appears to be the mul of the width and height. */
//	out_buf.m.planes[0].length = cedrus_cap_buf.m.planes[0].length;
//	out_buf.m.planes[1].length = cedrus_cap_buf.m.planes[1].length;
	out_buf.m.planes[0].length = 1080*1920;
	out_buf.m.planes[1].length = 1080*1920;

//	ioctl(driver_data->mem2mem_output_fd, VIDIOC_QUERYBUF, &out_buf);
	

	/////////////////////////////////////////////////////////////////
	if(ioctl(driver_data->mem2mem_output_fd, VIDIOC_QBUF, &out_buf)) {
		sunxi_cedrus_msg("Error: FE, could not QBUF\n" );
		ioctl(driver_data->mem2mem_output_fd, VIDIOC_DQBUF, &out_buf);
		return -1;
	}

//	ioctl(driver_data->mem2mem_output_fd, VIDIOC_DQBUF, &out_buf);

	//TEST, check if there needs to be a queued CAP buffer.
	memset(planes_out, 0, 2 * sizeof(struct v4l2_plane));
	
	memset(&cap_buf, 0, sizeof(struct v4l2_buffer));
	cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	cap_buf.memory = V4L2_MEMORY_MMAP;
	cap_buf.index = 0;
	cap_buf.length = 2; //Length? Is this Y & UV? Planes?
	cap_buf.m.planes = planes_out;
	
	assert(ioctl(driver_data->mem2mem_output_fd, VIDIOC_QUERYBUF, &cap_buf)==0);
	if(ioctl(driver_data->mem2mem_output_fd, VIDIOC_QBUF, &cap_buf)) {
		sunxi_cedrus_msg("Error: FE, could not QBUF\n" );
		ioctl(driver_data->mem2mem_output_fd, VIDIOC_DQBUF, &cap_buf);
		return -1;
	}


	sunxi_cedrus_msg("Before dequeueing stufff\n");
	ioctl(driver_data->mem2mem_output_fd, VIDIOC_DQBUF, &out_buf);
	ioctl(driver_data->mem2mem_output_fd, VIDIOC_DQBUF, &cap_buf);

//	assert(ioctl(driver_data->mem2mem_output_fd, VIDIOC_QBUF, &cap_buf)==0);
	
	return 0;
}


/* HINTS
 *
 * SURFACE can get a reference one of the 3 buffers which are allocated userland.
 * Surface == Capture buffer
 */


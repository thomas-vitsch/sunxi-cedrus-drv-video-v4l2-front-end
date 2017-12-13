Sunxi Cedrus VA backend + WIP Example code implementing Allwinner A20
frontend.
=======================
This driver is based upon the cedrus VA backend.
The kernel module @ https://github.com/thomas-vitsch/v4l2_front_end is
needed. And the ve_player @https://github.com/thomas-vitsch/ve_player is
needed.

This VA backend enables the ve_player to send encoded MPEG2 buffers to the
cedrus driver, when cedrus returns tiled YUV420 frames, these are sent to
another device (the frontend of the display engine of the allwinner a20).
This device detiles and does the color space conversion. The backend of the
display engine will then use layer2 in the backend to display the result.

This removes the need for detiling the cedrus output with the CPU.

Sunxi Cedrus VA backend
=======================

This libVA video driver is designed to work with the v4l2 "sunxi-cedrus" kernel
driver. However, the only sunxi-cedrus specific part is the format conversion
from tiled to planar, otherwise it would be generic enough to be used with other
v4l2 drivers using the "Frame API".

You can try this driver with VLC but don't forget to tell libVA to use this
backend:

	export LIBVA_DRIVER_NAME=sunxi_cedrus
	vlc big_buck_bunny_480p_MPEG2_MP2_25fps_1800K.MPG

Sample media files can be found here:

	http://samplemedia.linaro.org/MPEG2/
	http://samplemedia.linaro.org/MPEG4/SVT/

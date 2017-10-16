#ifndef _FE_H_
#define _FE_H_

#include <va/va_backend.h>

int sunxi_cedrus_fe_test(VADriverContextP ctx);
int sunxi_cedrus_fe_throw_buffer(VADriverContextP ctx, struct v4l2_buffer cedrus_cap_buf);

#endif /* _FE_H_ */

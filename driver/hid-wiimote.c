/*
 * HID driver for Nintendo Wiimote devices
 * Copyright (c) 2011 David Herrmann
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/atomic.h>
#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/hid.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>

#define USB_VENDOR_ID_NINTENDO 0x057e
#define USB_DEVICE_ID_NINTENDO_WIIMOTE 0x0306

#define WIIMOTE_VERSION "0.1"
#define WIIMOTE_NAME "Nintendo Wii Remote"
#define WIIMOTE_BUFSIZE 32

struct wiimote_buf {
	__u8 data[HID_MAX_BUFFER_SIZE];
	size_t size;
};

struct wiimote_state {
	spinlock_t lock;
	__u8 flags;
	__u8 accel_split[2];

	/* synchronous cmd requests */
	struct mutex sync;
	struct completion ready;
	int cmd;
	__u32 opt;

	/* results of synchronous requests */
	__u8 cmd_battery;
	__u8 cmd_err;
	__u8 *cmd_read_buf;
	__u8 cmd_read_size;
};

struct wiimote_data {
	atomic_t ready;
	struct hid_device *hdev;
	struct input_dev *input;

	spinlock_t qlock;
	__u8 head;
	__u8 tail;
	struct wiimote_buf outq[WIIMOTE_BUFSIZE];
	struct work_struct worker;

	struct wiimote_state state;

#ifdef CONFIG_DEBUG_FS
	struct dentry *debug_eeprom;
#endif
};

#define WIIPROTO_FLAG_LED1		0x01
#define WIIPROTO_FLAG_LED2		0x02
#define WIIPROTO_FLAG_LED3		0x04
#define WIIPROTO_FLAG_LED4		0x08
#define WIIPROTO_FLAG_RUMBLE		0x10
#define WIIPROTO_FLAG_ACCEL		0x20
#define WIIPROTO_FLAG_IR_BASIC		0x40
#define WIIPROTO_FLAG_IR_EXT		0x80
#define WIIPROTO_FLAG_IR_FULL		0xc0 /* IR_BASIC | IR_EXT */
#define WIIPROTO_FLAGS_LEDS (WIIPROTO_FLAG_LED1 | WIIPROTO_FLAG_LED2 | \
					WIIPROTO_FLAG_LED3 | WIIPROTO_FLAG_LED4)
#define WIIPROTO_FLAGS_IR (WIIPROTO_FLAG_IR_BASIC | WIIPROTO_FLAG_IR_EXT | \
							WIIPROTO_FLAG_IR_FULL)

enum wiiproto_reqs {
	WIIPROTO_REQ_NULL = 0x0,
	WIIPROTO_REQ_RUMBLE = 0x10,
	WIIPROTO_REQ_LED = 0x11,
	WIIPROTO_REQ_DRM = 0x12,
	WIIPROTO_REQ_IR1 = 0x13,
	WIIPROTO_REQ_SREQ = 0x15,
	WIIPROTO_REQ_WMEM = 0x16,
	WIIPROTO_REQ_RMEM = 0x17,
	WIIPROTO_REQ_IR2 = 0x1a,
	WIIPROTO_REQ_STATUS = 0x20,
	WIIPROTO_REQ_DATA = 0x21,
	WIIPROTO_REQ_RETURN = 0x22,
	WIIPROTO_REQ_DRM_K = 0x30,
	WIIPROTO_REQ_DRM_KA = 0x31,
	WIIPROTO_REQ_DRM_KE = 0x32,
	WIIPROTO_REQ_DRM_KAI = 0x33,
	WIIPROTO_REQ_DRM_KEE = 0x34,
	WIIPROTO_REQ_DRM_KAE = 0x35,
	WIIPROTO_REQ_DRM_KIE = 0x36,
	WIIPROTO_REQ_DRM_KAIE = 0x37,
	WIIPROTO_REQ_DRM_E = 0x3d,
	WIIPROTO_REQ_DRM_SKAI1 = 0x3e,
	WIIPROTO_REQ_DRM_SKAI2 = 0x3f,
};

enum wiiproto_keys {
	WIIPROTO_KEY_LEFT,
	WIIPROTO_KEY_RIGHT,
	WIIPROTO_KEY_UP,
	WIIPROTO_KEY_DOWN,
	WIIPROTO_KEY_PLUS,
	WIIPROTO_KEY_MINUS,
	WIIPROTO_KEY_ONE,
	WIIPROTO_KEY_TWO,
	WIIPROTO_KEY_A,
	WIIPROTO_KEY_B,
	WIIPROTO_KEY_HOME,
	WIIPROTO_KEY_COUNT
};

static __u16 wiiproto_keymap[] = {
	KEY_LEFT,	/* WIIPROTO_KEY_LEFT */
	KEY_RIGHT,	/* WIIPROTO_KEY_RIGHT */
	KEY_UP,		/* WIIPROTO_KEY_UP */
	KEY_DOWN,	/* WIIPROTO_KEY_DOWN */
	KEY_NEXT,	/* WIIPROTO_KEY_PLUS */
	KEY_PREVIOUS,	/* WIIPROTO_KEY_MINUS */
	BTN_1,		/* WIIPROTO_KEY_ONE */
	BTN_2,		/* WIIPROTO_KEY_TWO */
	BTN_A,		/* WIIPROTO_KEY_A */
	BTN_B,		/* WIIPROTO_KEY_B */
	BTN_MODE,	/* WIIPROTO_KEY_HOME */
};

#define dev_to_wii(pdev) hid_get_drvdata(container_of(pdev, struct hid_device, \
									dev))

/* requires the state.lock spinlock to be held */
static inline bool wiimote_cmd_pending(struct wiimote_data *wdata, int cmd,
								__u32 opt)
{
	return wdata->state.cmd == cmd && wdata->state.opt == opt;
}

/* requires the state.lock spinlock to be held */
static inline void wiimote_cmd_complete(struct wiimote_data *wdata)
{
	wdata->state.cmd = WIIPROTO_REQ_NULL;
	complete(&wdata->state.ready);
}

static inline int wiimote_cmd_acquire(struct wiimote_data *wdata)
{
	return mutex_lock_interruptible(&wdata->state.sync) ? -ERESTARTSYS : 0;
}

/* requires the state.lock spinlock to be held */
static inline void wiimote_cmd_set(struct wiimote_data *wdata, int cmd,
								__u32 opt)
{
	INIT_COMPLETION(wdata->state.ready);
	wdata->state.cmd = cmd;
	wdata->state.opt = opt;
}

static inline void wiimote_cmd_release(struct wiimote_data *wdata)
{
	mutex_unlock(&wdata->state.sync);
}

static inline int wiimote_cmd_wait(struct wiimote_data *wdata)
{
	int ret;

	ret = wait_for_completion_interruptible_timeout(&wdata->state.ready, HZ);
	if (ret < 0)
		return -ERESTARTSYS;
	else if (ret == 0)
		return -EIO;
	else
		return 0;
}

static ssize_t wiimote_hid_send(struct hid_device *hdev, __u8 *buffer,
								size_t count)
{
	__u8 *buf;
	ssize_t ret;

	if (!hdev->hid_output_raw_report)
		return -ENODEV;

	buf = kmemdup(buffer, count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = hdev->hid_output_raw_report(hdev, buf, count, HID_OUTPUT_REPORT);

	kfree(buf);
	return ret;
}

static void wiimote_worker(struct work_struct *work)
{
	struct wiimote_data *wdata = container_of(work, struct wiimote_data,
									worker);
	unsigned long flags;

	spin_lock_irqsave(&wdata->qlock, flags);

	while (wdata->head != wdata->tail) {
		spin_unlock_irqrestore(&wdata->qlock, flags);
		wiimote_hid_send(wdata->hdev, wdata->outq[wdata->tail].data,
						wdata->outq[wdata->tail].size);
		spin_lock_irqsave(&wdata->qlock, flags);

		wdata->tail = (wdata->tail + 1) % WIIMOTE_BUFSIZE;
	}

	spin_unlock_irqrestore(&wdata->qlock, flags);
}

static void wiimote_queue(struct wiimote_data *wdata, const __u8 *buffer,
								size_t count)
{
	unsigned long flags;
	__u8 newhead;

	if (count > HID_MAX_BUFFER_SIZE) {
		hid_warn(wdata->hdev, "Sending too large output report\n");
		return;
	}

	/*
	 * Copy new request into our output queue and check whether the
	 * queue is full. If it is full, discard this request.
	 * If it is empty we need to start a new worker that will
	 * send out the buffer to the hid device.
	 * If the queue is not empty, then there must be a worker
	 * that is currently sending out our buffer and this worker
	 * will reschedule itself until the queue is empty.
	 */

	spin_lock_irqsave(&wdata->qlock, flags);

	memcpy(wdata->outq[wdata->head].data, buffer, count);
	wdata->outq[wdata->head].size = count;
	newhead = (wdata->head + 1) % WIIMOTE_BUFSIZE;

	if (wdata->head == wdata->tail) {
		wdata->head = newhead;
		schedule_work(&wdata->worker);
	} else if (newhead != wdata->tail) {
		wdata->head = newhead;
	} else {
		hid_warn(wdata->hdev, "Output queue is full");
	}

	spin_unlock_irqrestore(&wdata->qlock, flags);
}

/*
 * This sets the rumble bit on the given output report if rumble is
 * currently enabled.
 * \cmd1 must point to the second byte in the output report => &cmd[1]
 * This must be called on nearly every output report before passing it
 * into the output queue!
 */
static inline void wiiproto_keep_rumble(struct wiimote_data *wdata, __u8 *cmd1)
{
	if (wdata->state.flags & WIIPROTO_FLAG_RUMBLE)
		*cmd1 |= 0x01;
}

static void wiiproto_req_rumble(struct wiimote_data *wdata, __u8 rumble)
{
	__u8 cmd[2];

	rumble = !!rumble;
	if (rumble == !!(wdata->state.flags & WIIPROTO_FLAG_RUMBLE))
		return;

	if (rumble)
		wdata->state.flags |= WIIPROTO_FLAG_RUMBLE;
	else
		wdata->state.flags &= ~WIIPROTO_FLAG_RUMBLE;

	cmd[0] = WIIPROTO_REQ_RUMBLE;
	cmd[1] = 0;

	wiiproto_keep_rumble(wdata, &cmd[1]);
	wiimote_queue(wdata, cmd, sizeof(cmd));
}

static void wiiproto_req_leds(struct wiimote_data *wdata, int leds)
{
	__u8 cmd[2];

	leds &= WIIPROTO_FLAGS_LEDS;
	if ((wdata->state.flags & WIIPROTO_FLAGS_LEDS) == leds)
		return;
	wdata->state.flags = (wdata->state.flags & ~WIIPROTO_FLAGS_LEDS) | leds;

	cmd[0] = WIIPROTO_REQ_LED;
	cmd[1] = 0;

	if (leds & WIIPROTO_FLAG_LED1)
		cmd[1] |= 0x10;
	if (leds & WIIPROTO_FLAG_LED2)
		cmd[1] |= 0x20;
	if (leds & WIIPROTO_FLAG_LED3)
		cmd[1] |= 0x40;
	if (leds & WIIPROTO_FLAG_LED4)
		cmd[1] |= 0x80;

	wiiproto_keep_rumble(wdata, &cmd[1]);
	wiimote_queue(wdata, cmd, sizeof(cmd));
}

/*
 * Check what peripherals of the wiimote are currently
 * active and select a proper DRM that supports all of
 * the requested data inputs.
 */
static __u8 select_drm(struct wiimote_data *wdata)
{
	__u8 ir = wdata->state.flags & WIIPROTO_FLAGS_IR;
	if (ir == WIIPROTO_FLAG_IR_BASIC) {
		if (wdata->state.flags & WIIPROTO_FLAG_ACCEL)
			return WIIPROTO_REQ_DRM_KAIE;
		else
			return WIIPROTO_REQ_DRM_KIE;
	} else if (ir == WIIPROTO_FLAG_IR_EXT) {
		return WIIPROTO_REQ_DRM_KAI;
	} else if (ir == WIIPROTO_FLAG_IR_FULL) {
		return WIIPROTO_REQ_DRM_SKAI1;
	} else {
		if (wdata->state.flags & WIIPROTO_FLAG_ACCEL)
			return WIIPROTO_REQ_DRM_KA;
		else
			return WIIPROTO_REQ_DRM_K;
	}
}

static void wiiproto_req_drm(struct wiimote_data *wdata, __u8 drm)
{
	__u8 cmd[3];

	if (drm == WIIPROTO_REQ_NULL)
		drm = select_drm(wdata);

	cmd[0] = WIIPROTO_REQ_DRM;
	cmd[1] = 0;
	cmd[2] = drm;

	wiiproto_keep_rumble(wdata, &cmd[1]);
	wiimote_queue(wdata, cmd, sizeof(cmd));
}

static void wiiproto_req_status(struct wiimote_data *wdata)
{
	__u8 cmd[2];

	cmd[0] = WIIPROTO_REQ_SREQ;
	cmd[1] = 0;

	wiiproto_keep_rumble(wdata, &cmd[1]);
	wiimote_queue(wdata, cmd, sizeof(cmd));
}

static void wiiproto_req_accel(struct wiimote_data *wdata, __u8 accel)
{
	accel = !!accel;
	if (accel == !!(wdata->state.flags & WIIPROTO_FLAG_ACCEL))
		return;

	if (accel)
		wdata->state.flags |= WIIPROTO_FLAG_ACCEL;
	else
		wdata->state.flags &= ~WIIPROTO_FLAG_ACCEL;

	wiiproto_req_drm(wdata, WIIPROTO_REQ_NULL);
}

static void wiiproto_req_ir1(struct wiimote_data *wdata, __u8 flags)
{
	__u8 cmd[2];

	cmd[0] = WIIPROTO_REQ_IR1;
	cmd[1] = flags;

	wiiproto_keep_rumble(wdata, &cmd[1]);
	wiimote_queue(wdata, cmd, sizeof(cmd));
}

static void wiiproto_req_ir2(struct wiimote_data *wdata, __u8 flags)
{
	__u8 cmd[2];

	cmd[0] = WIIPROTO_REQ_IR2;
	cmd[1] = flags;

	wiiproto_keep_rumble(wdata, &cmd[1]);
	wiimote_queue(wdata, cmd, sizeof(cmd));
}

#define wiiproto_req_wreg(wdata, os, buf, sz) \
			wiiproto_req_wmem((wdata), false, (os), (buf), (sz))

#define wiiproto_req_weeprom(wdata, os, buf, sz) \
			wiiproto_req_wmem((wdata), true, (os), (buf), (sz))

static void wiiproto_req_wmem(struct wiimote_data *wdata, bool eeprom,
				__u32 offset, const __u8 *buf, __u8 size)
{
	__u8 cmd[22];

	if (size > 16 || size == 0) {
		hid_warn(wdata->hdev, "Invalid length %d wmem request\n", size);
		return;
	}

	memset(cmd, 0, sizeof(cmd));
	cmd[0] = WIIPROTO_REQ_WMEM;
	cmd[2] = (offset >> 16) & 0xff;
	cmd[3] = (offset >> 8) & 0xff;
	cmd[4] = offset & 0xff;
	cmd[5] = size;
	memcpy(&cmd[6], buf, size);

	if (!eeprom)
		cmd[1] |= 0x04;

	wiiproto_keep_rumble(wdata, &cmd[1]);
	wiimote_queue(wdata, cmd, sizeof(cmd));
}

#define wiiproto_req_rreg(wdata, os, sz) \
			wiiproto_req_rmem((wdata), false, (os), (sz))

#define wiiproto_req_reeprom(wdata, os, sz) \
			wiiproto_req_rmem((wdata), true, (os), (sz))

static void wiiproto_req_rmem(struct wiimote_data *wdata, bool eeprom,
						__u32 offset, __u16 size)
{
	__u8 cmd[7];

	if (size == 0) {
		hid_warn(wdata->hdev, "Invalid length %d rmem request\n", size);
		return;
	}

	cmd[0] = WIIPROTO_REQ_RMEM;
	cmd[1] = 0;
	cmd[2] = (offset >> 16) & 0xff;
	cmd[3] = (offset >> 8) & 0xff;
	cmd[4] = offset & 0xff;
	cmd[5] = (size >> 8) & 0xff;
	cmd[6] = size & 0xff;

	if (!eeprom)
		cmd[1] |= 0x04;

	wiiproto_keep_rumble(wdata, &cmd[1]);
	wiimote_queue(wdata, cmd, sizeof(cmd));
}

static int wiimote_init_ir(struct wiimote_data *wdata, __u16 mode)
{
	int ret;
	unsigned long flags;
	__u8 format = 0;
	static const __u8 data_enable[] = { 0x01 };
	static const __u8 data_sens1[] = { 0x02, 0x00, 0x00, 0x71, 0x01,
						0x00, 0xaa, 0x00, 0x64 };
	static const __u8 data_sens2[] = { 0x63, 0x03 };
	static const __u8 data_fin[] = { 0x08 };

	ret = wiimote_cmd_acquire(wdata);
	if (ret)
		return ret;

	spin_lock_irqsave(&wdata->state.lock, flags);

	if (mode == (wdata->state.flags & WIIPROTO_FLAGS_IR)) {
		spin_unlock_irqrestore(&wdata->state.lock, flags);
		goto unlock;
	}

	if (mode == 0) {
		wdata->state.flags &= ~WIIPROTO_FLAGS_IR;
		wiiproto_req_ir1(wdata, 0);
		wiiproto_req_ir2(wdata, 0);
		wiiproto_req_drm(wdata, WIIPROTO_REQ_NULL);
		spin_unlock_irqrestore(&wdata->state.lock, flags);
		goto unlock;
	}

	/* send PIXEL CLOCK ENABLE cmd first */
	wiimote_cmd_set(wdata, WIIPROTO_REQ_IR1, 0);
	wiiproto_req_ir1(wdata, 0x06);

	spin_unlock_irqrestore(&wdata->state.lock, flags);

	ret = wiimote_cmd_wait(wdata);
	if (ret || (ret = wdata->state.cmd_err))
		goto unlock;

	/* enable IR LOGIC then */
	spin_lock_irqsave(&wdata->state.lock, flags);
	wiimote_cmd_set(wdata, WIIPROTO_REQ_IR2, 0);
	wiiproto_req_ir2(wdata, 0x06);
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	ret = wiimote_cmd_wait(wdata);
	if (ret || (ret = wdata->state.cmd_err))
		goto unlock;

	/* enable IR cam but do not make it send data, yet */
	spin_lock_irqsave(&wdata->state.lock, flags);
	wiimote_cmd_set(wdata, WIIPROTO_REQ_WMEM, 0);
	wiiproto_req_wreg(wdata, 0xb00030, data_enable, sizeof(data_enable));
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	ret = wiimote_cmd_wait(wdata);
	if (ret || (ret = wdata->state.cmd_err))
		goto unlock;

	/* write first sensitivity block */
	spin_lock_irqsave(&wdata->state.lock, flags);
	wiimote_cmd_set(wdata, WIIPROTO_REQ_WMEM, 0);
	wiiproto_req_wreg(wdata, 0xb00000, data_sens1, sizeof(data_sens1));
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	ret = wiimote_cmd_wait(wdata);
	if (ret || (ret = wdata->state.cmd_err))
		goto unlock;

	/* write second sensitivity block */
	spin_lock_irqsave(&wdata->state.lock, flags);
	wiimote_cmd_set(wdata, WIIPROTO_REQ_WMEM, 0);
	wiiproto_req_wreg(wdata, 0xb0001a, data_sens2, sizeof(data_sens2));
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	ret = wiimote_cmd_wait(wdata);
	if (ret || (ret = wdata->state.cmd_err))
		goto unlock;

	/* put IR cam into desired state */
	switch (mode) {
		case WIIPROTO_FLAG_IR_FULL:
			format = 5;
			break;
		case WIIPROTO_FLAG_IR_EXT:
			format = 3;
			break;
		case WIIPROTO_FLAG_IR_BASIC:
			format = 1;
			break;
	}
	spin_lock_irqsave(&wdata->state.lock, flags);
	wiimote_cmd_set(wdata, WIIPROTO_REQ_WMEM, 0);
	wiiproto_req_wreg(wdata, 0xb00033, &format, sizeof(format));
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	ret = wiimote_cmd_wait(wdata);
	if (ret || (ret = wdata->state.cmd_err))
		goto unlock;

	/* make IR cam send data */
	spin_lock_irqsave(&wdata->state.lock, flags);
	wiimote_cmd_set(wdata, WIIPROTO_REQ_WMEM, 0);
	wiiproto_req_wreg(wdata, 0xb00030, data_fin, sizeof(data_fin));
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	ret = wiimote_cmd_wait(wdata);
	if (ret || (ret = wdata->state.cmd_err))
		goto unlock;

	/* request new DRM mode compatible to IR mode */
	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.flags &= ~WIIPROTO_FLAGS_IR;
	wdata->state.flags |= mode;
	wiiproto_req_drm(wdata, 0);
	spin_unlock_irqrestore(&wdata->state.lock, flags);

unlock:
	wiimote_cmd_release(wdata);
	return ret > 0 ? -EIO : ret;
}

#define wiifs_led_show_set(num)						\
static ssize_t wiifs_led_show_##num(struct device *dev,			\
			struct device_attribute *attr, char *buf)	\
{									\
	struct wiimote_data *wdata = dev_to_wii(dev);			\
	unsigned long flags;						\
	int state;							\
									\
	if (!atomic_read(&wdata->ready))				\
		return -EBUSY;						\
									\
	spin_lock_irqsave(&wdata->state.lock, flags);			\
	state = !!(wdata->state.flags & WIIPROTO_FLAG_LED##num);	\
	spin_unlock_irqrestore(&wdata->state.lock, flags);		\
									\
	return sprintf(buf, "%d\n", state);				\
}									\
static ssize_t wiifs_led_set_##num(struct device *dev,			\
	struct device_attribute *attr, const char *buf, size_t count)	\
{									\
	struct wiimote_data *wdata = dev_to_wii(dev);			\
	int tmp = simple_strtoul(buf, NULL, 10);			\
	unsigned long flags;						\
	__u8 state;							\
									\
	if (!atomic_read(&wdata->ready))				\
		return -EBUSY;						\
									\
	spin_lock_irqsave(&wdata->state.lock, flags);			\
									\
	state = wdata->state.flags;					\
									\
	if (tmp)							\
		wiiproto_req_leds(wdata, state | WIIPROTO_FLAG_LED##num);\
	else								\
		wiiproto_req_leds(wdata, state & ~WIIPROTO_FLAG_LED##num);\
									\
	spin_unlock_irqrestore(&wdata->state.lock, flags);		\
									\
	return count;							\
}									\
static DEVICE_ATTR(led##num, S_IRUGO | S_IWUSR, wiifs_led_show_##num,	\
						wiifs_led_set_##num)

wiifs_led_show_set(1);
wiifs_led_show_set(2);
wiifs_led_show_set(3);
wiifs_led_show_set(4);

static ssize_t wiifs_rumble_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct wiimote_data *wdata = dev_to_wii(dev);
	unsigned long flags;
	int state;

	if (!atomic_read(&wdata->ready))
		return -EBUSY;

	spin_lock_irqsave(&wdata->state.lock, flags);
	state = !!(wdata->state.flags & WIIPROTO_FLAG_RUMBLE);
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	return sprintf(buf, "%d\n", state);
}

static ssize_t wiifs_rumble_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct wiimote_data *wdata = dev_to_wii(dev);
	int tmp = simple_strtoul(buf, NULL, 10);
	unsigned long flags;

	if (!atomic_read(&wdata->ready))
		return -EBUSY;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wiiproto_req_rumble(wdata, tmp);
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	return count;
}

static DEVICE_ATTR(rumble, S_IRUGO | S_IWUSR, wiifs_rumble_show,
							wiifs_rumble_set);

static ssize_t wiifs_accel_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct wiimote_data *wdata = dev_to_wii(dev);
	unsigned long flags;
	int state;

	if (!atomic_read(&wdata->ready))
		return -EBUSY;

	spin_lock_irqsave(&wdata->state.lock, flags);
	state = !!(wdata->state.flags & WIIPROTO_FLAG_ACCEL);
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	return sprintf(buf, "%d\n", state);
}

static ssize_t wiifs_accel_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct wiimote_data *wdata = dev_to_wii(dev);
	int tmp = simple_strtoul(buf, NULL, 10);
	unsigned long flags;

	if (!atomic_read(&wdata->ready))
		return -EBUSY;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wiiproto_req_accel(wdata, tmp);
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	return count;
}

static DEVICE_ATTR(accelerometer, S_IRUGO | S_IWUSR, wiifs_accel_show,
							wiifs_accel_set);

static ssize_t wiifs_battery_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct wiimote_data *wdata = dev_to_wii(dev);
	unsigned long flags;
	int state, ret;

	if (!atomic_read(&wdata->ready))
		return -EBUSY;
	/* smp_rmb: Make sure wdata->xy is available when wdata->ready is 1 */
	smp_rmb();

	ret = wiimote_cmd_acquire(wdata);
	if (ret)
		return ret;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wiimote_cmd_set(wdata, WIIPROTO_REQ_SREQ, 0);
	wiiproto_req_status(wdata);
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	ret = wiimote_cmd_wait(wdata);
	state = wdata->state.cmd_battery;
	wiimote_cmd_release(wdata);

	if (ret)
		return ret;

	return sprintf(buf, "%d\n", state);
}

static DEVICE_ATTR(battery, S_IRUGO, wiifs_battery_show, NULL);

static ssize_t wiifs_ir_show(struct device *dev, struct device_attribute *attr,
								char *buf)
{
	struct wiimote_data *wdata = dev_to_wii(dev);
	unsigned long flags;
	const char *mode;
	__u8 flag;

	if (!atomic_read(&wdata->ready))
		return -EBUSY;

	spin_lock_irqsave(&wdata->state.lock, flags);
	flag = wdata->state.flags & WIIPROTO_FLAGS_IR;
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	switch (flag) {
		case WIIPROTO_FLAG_IR_FULL:
			mode = "full";
			break;
		case WIIPROTO_FLAG_IR_EXT:
			mode = "extended";
			break;
		case WIIPROTO_FLAG_IR_BASIC:
			mode = "basic";
			break;
		default:
			mode = "off";
			break;
	}


	return sprintf(buf, "%s\n", mode);
}

static ssize_t wiifs_ir_set(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct wiimote_data *wdata = dev_to_wii(dev);
	int ret;
	__u8 mode;

	if (count == 0)
		return -EINVAL;

	if (!strncasecmp("basic", buf, 5))
		mode = WIIPROTO_FLAG_IR_BASIC;
	else if (!strncasecmp("extended", buf, 8))
		mode = WIIPROTO_FLAG_IR_EXT;
	else if (!strncasecmp("full", buf, 4))
		mode = WIIPROTO_FLAG_IR_FULL;
	else if (!strncasecmp("off", buf, 3))
		mode = 0;
	else
		return -EINVAL;

	if (!atomic_read(&wdata->ready))
		return -EBUSY;
	/* smp_rmb: Make sure wdata->xy is available when wdata->ready is 1 */
	smp_rmb();

	ret = wiimote_init_ir(wdata, mode);
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(ir, S_IRUGO | S_IWUSR, wiifs_ir_show, wiifs_ir_set);

#ifdef CONFIG_DEBUG_FS

static int wiifs_eeprom_open(struct inode *i, struct file *f)
{
	f->private_data = i->i_private;
	return 0;
}

static ssize_t wiifs_eeprom_read(struct file *f, char __user *u, size_t s,
								loff_t *off)
{
	struct wiimote_data *wdata = f->private_data;
	unsigned long flags;
	ssize_t ret;
	char *buf;
	__u16 size;

	if (s == 0)
		return -EINVAL;
	if (*off > 0xffffff)
		return 0;
	if (s > 16)
		s = 16;

	if (!atomic_read(&wdata->ready))
		return -EBUSY;
	/* smp_rmb: Make sure wdata->xy is available when wdata->ready is 1 */
	smp_rmb();

	buf = kmalloc(s, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = wiimote_cmd_acquire(wdata);
	if (ret)
		goto error;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.cmd_read_size = s;
	wdata->state.cmd_read_buf = buf;
	wiimote_cmd_set(wdata, WIIPROTO_REQ_RMEM, *off & 0xffff);
	wiiproto_req_reeprom(wdata, *off, s);
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	ret = wiimote_cmd_wait(wdata);
	if (!ret)
		size = wdata->state.cmd_read_size;
	wiimote_cmd_release(wdata);

	if (ret)
		goto error;
	if (size == 0) {
		ret = -EIO;
		goto error;
	}
	if (copy_to_user(u, buf, size)) {
		ret = -EFAULT;
		goto error;
	}

	*off += size;
	ret = size;

error:
	kfree(buf);
	return ret;
}

static const struct file_operations wiifs_eeprom_fops = {
	.owner = THIS_MODULE,
	.open = wiifs_eeprom_open,
	.read = wiifs_eeprom_read,
	.llseek = generic_file_llseek,
};

static void wiimote_debugfs_init(struct wiimote_data *wdata)
{
	wdata->debug_eeprom = debugfs_create_file("eeprom", S_IRUSR,
			wdata->hdev->debug_dir, wdata, &wiifs_eeprom_fops);
}

static void wiimote_debugfs_deinit(struct wiimote_data *wdata)
{
	if (wdata->debug_eeprom)
		debugfs_remove(wdata->debug_eeprom);
}

#else /* CONFIG_DEBUG_FS */

static void wiimote_debugfs_init(void *s)
{
}

static void wiimote_debugfs_deinit(void *s)
{
}

#endif /* CONFIG_DEBUG_FS */

static int wiimote_input_event(struct input_dev *dev, unsigned int type,
						unsigned int code, int value)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);

	if (!atomic_read(&wdata->ready))
		return -EBUSY;
	/* smp_rmb: Make sure wdata->xy is available when wdata->ready is 1 */
	smp_rmb();

	return 0;
}

static void handler_keys(struct wiimote_data *wdata, const __u8 *payload)
{
	input_report_key(wdata->input, wiiproto_keymap[WIIPROTO_KEY_LEFT],
							!!(payload[0] & 0x01));
	input_report_key(wdata->input, wiiproto_keymap[WIIPROTO_KEY_RIGHT],
							!!(payload[0] & 0x02));
	input_report_key(wdata->input, wiiproto_keymap[WIIPROTO_KEY_DOWN],
							!!(payload[0] & 0x04));
	input_report_key(wdata->input, wiiproto_keymap[WIIPROTO_KEY_UP],
							!!(payload[0] & 0x08));
	input_report_key(wdata->input, wiiproto_keymap[WIIPROTO_KEY_PLUS],
							!!(payload[0] & 0x10));
	input_report_key(wdata->input, wiiproto_keymap[WIIPROTO_KEY_TWO],
							!!(payload[1] & 0x01));
	input_report_key(wdata->input, wiiproto_keymap[WIIPROTO_KEY_ONE],
							!!(payload[1] & 0x02));
	input_report_key(wdata->input, wiiproto_keymap[WIIPROTO_KEY_B],
							!!(payload[1] & 0x04));
	input_report_key(wdata->input, wiiproto_keymap[WIIPROTO_KEY_A],
							!!(payload[1] & 0x08));
	input_report_key(wdata->input, wiiproto_keymap[WIIPROTO_KEY_MINUS],
							!!(payload[1] & 0x10));
	input_report_key(wdata->input, wiiproto_keymap[WIIPROTO_KEY_HOME],
							!!(payload[1] & 0x80));
}

static void handler_accel(struct wiimote_data *wdata, const __u8 *payload)
{
	__u16 x, y, z;

	if (!(wdata->state.flags & WIIPROTO_FLAG_ACCEL))
		return;

	/*
	 * payload is: BB BB XX YY ZZ
	 * Buttons data contains LSBs
	 */

	x = payload[2] << 2;
	y = payload[3] << 2;
	z = payload[4] << 2;

	x |= (payload[0] >> 5) & 0x3;
	y |= (payload[1] >> 4) & 0x2;
	z |= (payload[1] >> 5) & 0x2;

	input_event(wdata->input, EV_ABS, ABS_X, x - 0x200);
	input_event(wdata->input, EV_ABS, ABS_Y, y - 0x200);
	input_event(wdata->input, EV_ABS, ABS_Z, z - 0x200);
}

#define ir_to_input0(wdata, ir, packed) __ir_to_input((wdata), (ir), (packed), \
							ABS_HAT0X, ABS_HAT0Y)
#define ir_to_input1(wdata, ir, packed) __ir_to_input((wdata), (ir), (packed), \
							ABS_HAT1X, ABS_HAT1Y)
#define ir_to_input2(wdata, ir, packed) __ir_to_input((wdata), (ir), (packed), \
							ABS_HAT2X, ABS_HAT2Y)
#define ir_to_input3(wdata, ir, packed) __ir_to_input((wdata), (ir), (packed), \
							ABS_HAT3X, ABS_HAT3Y)

static void __ir_to_input(struct wiimote_data *wdata, const __u8 *ir,
						bool packed, __u8 xid, __u8 yid)
{
	__u16 x, y;

	if (!(wdata->state.flags & WIIPROTO_FLAGS_IR))
		return;

	if (packed) {
		x = ir[1] << 2;
		y = ir[2] << 2;

		x |= ir[0] & 0x3;
		y |= (ir[0] >> 2) & 0x3;
	} else {
		x = ir[0] << 2;
		y = ir[1] << 2;

		x |= (ir[2] >> 4) & 0x3;
		y |= (ir[2] >> 6) & 0x3;
	}

	input_event(wdata->input, EV_ABS, xid, x);
	input_event(wdata->input, EV_ABS, yid, y);
}

static void handler_status(struct wiimote_data *wdata, const __u8 *payload)
{
	handler_keys(wdata, payload);

	/* on status reports the drm is reset so we need to resend the drm */
	wiiproto_req_drm(wdata, 0);

	if (wiimote_cmd_pending(wdata, WIIPROTO_REQ_SREQ, 0)) {
		wdata->state.cmd_battery = payload[5];
		wiimote_cmd_complete(wdata);
	}
}

static void handler_data(struct wiimote_data *wdata, const __u8 *payload)
{
	__u16 offset = payload[3] << 8 | payload[4];
	__u8 size = (payload[2] >> 4) + 1;

	handler_keys(wdata, payload);

	if (wiimote_cmd_pending(wdata, WIIPROTO_REQ_RMEM, offset)) {
		if (size > wdata->state.cmd_read_size)
			size = wdata->state.cmd_read_size;
		else
			wdata->state.cmd_read_size = size;
		memcpy(wdata->state.cmd_read_buf, &payload[5], size);
		wiimote_cmd_complete(wdata);
	}
}

static void handler_return(struct wiimote_data *wdata, const __u8 *payload)
{
	__u8 err = payload[3];
	__u8 cmd = payload[2];

	handler_keys(wdata, payload);

	if (err)
		hid_warn(wdata->hdev, "Remote error %hhu on req %hhu\n", err,
									cmd);

	if (wiimote_cmd_pending(wdata, cmd, 0)) {
		wdata->state.cmd_err = err;
		wiimote_cmd_complete(wdata);
	}
}

static void handler_drm_KA(struct wiimote_data *wdata, const __u8 *payload)
{
	handler_keys(wdata, payload);
	handler_accel(wdata, payload);
}

static void handler_drm_KE(struct wiimote_data *wdata, const __u8 *payload)
{
	handler_keys(wdata, payload);
}

static void handler_drm_KAI(struct wiimote_data *wdata, const __u8 *payload)
{
	handler_keys(wdata, payload);
	handler_accel(wdata, payload);
	ir_to_input0(wdata, &payload[5], false);
	ir_to_input1(wdata, &payload[8], false);
	ir_to_input2(wdata, &payload[11], false);
	ir_to_input3(wdata, &payload[14], false);
}

static void handler_drm_KEE(struct wiimote_data *wdata, const __u8 *payload)
{
	handler_keys(wdata, payload);
}

static void handler_drm_KIE(struct wiimote_data *wdata, const __u8 *payload)
{
	handler_keys(wdata, payload);
	ir_to_input0(wdata, &payload[2], false);
	ir_to_input1(wdata, &payload[4], true);
	ir_to_input2(wdata, &payload[7], false);
	ir_to_input3(wdata, &payload[9], true);
}

static void handler_drm_KAIE(struct wiimote_data *wdata, const __u8 *payload)
{
	handler_keys(wdata, payload);
	handler_accel(wdata, payload);
	ir_to_input0(wdata, &payload[5], false);
	ir_to_input1(wdata, &payload[7], true);
	ir_to_input2(wdata, &payload[10], false);
	ir_to_input3(wdata, &payload[12], true);
}

static void handler_drm_E(struct wiimote_data *wdata, const __u8 *payload)
{
}

static void handler_drm_SKAI1(struct wiimote_data *wdata, const __u8 *payload)
{
	handler_keys(wdata, payload);

	wdata->state.accel_split[0] = payload[2];
	wdata->state.accel_split[1] = (payload[0] >> 1) & (0x10 | 0x20);
	wdata->state.accel_split[1] |= (payload[1] << 1) & (0x40 | 0x80);

	ir_to_input0(wdata, &payload[3], false);
	ir_to_input1(wdata, &payload[12], false);
}

static void handler_drm_SKAI2(struct wiimote_data *wdata, const __u8 *payload)
{
	__u8 buf[5];

	handler_keys(wdata, payload);

	wdata->state.accel_split[1] |= (payload[0] >> 5) & (0x01 | 0x02);
	wdata->state.accel_split[1] |= (payload[1] >> 3) & (0x04 | 0x08);

	buf[0] = 0;
	buf[1] = 0;
	buf[2] = wdata->state.accel_split[0];
	buf[3] = payload[2];
	buf[4] = wdata->state.accel_split[1];
	handler_accel(wdata, buf);

	ir_to_input2(wdata, &payload[3], false);
	ir_to_input3(wdata, &payload[12], false);
}

struct wiiproto_handler {
	__u8 id;
	size_t size;
	void (*func)(struct wiimote_data *wdata, const __u8 *payload);
};

static struct wiiproto_handler handlers[] = {
	{ .id = WIIPROTO_REQ_STATUS, .size = 6, .func = handler_status },
	{ .id = WIIPROTO_REQ_DATA, .size = 21, .func = handler_data },
	{ .id = WIIPROTO_REQ_RETURN, .size = 4, .func = handler_return },
	{ .id = WIIPROTO_REQ_DRM_K, .size = 2, .func = handler_keys },
	{ .id = WIIPROTO_REQ_DRM_KA, .size = 5, .func = handler_drm_KA },
	{ .id = WIIPROTO_REQ_DRM_KE, .size = 10, .func = handler_drm_KE },
	{ .id = WIIPROTO_REQ_DRM_KAI, .size = 17, .func = handler_drm_KAI },
	{ .id = WIIPROTO_REQ_DRM_KEE, .size = 21, .func = handler_drm_KEE },
	{ .id = WIIPROTO_REQ_DRM_KIE, .size = 21, .func = handler_drm_KIE },
	{ .id = WIIPROTO_REQ_DRM_KAIE, .size = 21, .func = handler_drm_KAIE },
	{ .id = WIIPROTO_REQ_DRM_E, .size = 21, .func = handler_drm_E },
	{ .id = WIIPROTO_REQ_DRM_SKAI1, .size = 21, .func = handler_drm_SKAI1 },
	{ .id = WIIPROTO_REQ_DRM_SKAI2, .size = 21, .func = handler_drm_SKAI2 },
	{ .id = 0 }
};

static int wiimote_hid_event(struct hid_device *hdev, struct hid_report *report,
							u8 *raw_data, int size)
{
	struct wiimote_data *wdata = hid_get_drvdata(hdev);
	struct wiiproto_handler *h;
	int i;
	unsigned long flags;
	bool handled = false;

	if (!atomic_read(&wdata->ready))
		return -EBUSY;
	/* smp_rmb: Make sure wdata->xy is available when wdata->ready is 1 */
	smp_rmb();

	if (size < 1)
		return -EINVAL;

	spin_lock_irqsave(&wdata->state.lock, flags);

	for (i = 0; handlers[i].id; ++i) {
		h = &handlers[i];
		if (h->id == raw_data[0] && h->size < size) {
			h->func(wdata, &raw_data[1]);
			handled = true;
		}
	}

	spin_unlock_irqrestore(&wdata->state.lock, flags);

	if (!handled)
		hid_warn(hdev, "Unhandled report %hhu size %d\n", raw_data[0],
									size);
	else
		input_sync(wdata->input);

	return 0;
}

static struct wiimote_data *wiimote_create(struct hid_device *hdev)
{
	struct wiimote_data *wdata;
	int i;

	wdata = kzalloc(sizeof(*wdata), GFP_KERNEL);
	if (!wdata)
		return NULL;

	wdata->input = input_allocate_device();
	if (!wdata->input) {
		kfree(wdata);
		return NULL;
	}

	wdata->hdev = hdev;
	hid_set_drvdata(hdev, wdata);

	input_set_drvdata(wdata->input, wdata);
	wdata->input->event = wiimote_input_event;
	wdata->input->dev.parent = &wdata->hdev->dev;
	wdata->input->id.bustype = wdata->hdev->bus;
	wdata->input->id.vendor = wdata->hdev->vendor;
	wdata->input->id.product = wdata->hdev->product;
	wdata->input->id.version = wdata->hdev->version;
	wdata->input->name = WIIMOTE_NAME;

	set_bit(EV_KEY, wdata->input->evbit);
	for (i = 0; i < WIIPROTO_KEY_COUNT; ++i)
		set_bit(wiiproto_keymap[i], wdata->input->keybit);

	set_bit(EV_ABS, wdata->input->evbit);
	set_bit(ABS_X, wdata->input->absbit);
	set_bit(ABS_Y, wdata->input->absbit);
	set_bit(ABS_Z, wdata->input->absbit);
	input_set_abs_params(wdata->input, ABS_X, -500, 500, 2, 4);
	input_set_abs_params(wdata->input, ABS_Y, -500, 500, 2, 4);
	input_set_abs_params(wdata->input, ABS_Z, -500, 500, 2, 4);

	set_bit(ABS_HAT0X, wdata->input->absbit);
	set_bit(ABS_HAT0Y, wdata->input->absbit);
	set_bit(ABS_HAT1X, wdata->input->absbit);
	set_bit(ABS_HAT1Y, wdata->input->absbit);
	set_bit(ABS_HAT2X, wdata->input->absbit);
	set_bit(ABS_HAT2Y, wdata->input->absbit);
	set_bit(ABS_HAT3X, wdata->input->absbit);
	set_bit(ABS_HAT3Y, wdata->input->absbit);
	input_set_abs_params(wdata->input, ABS_HAT0X, 0, 1023, 2, 4);
	input_set_abs_params(wdata->input, ABS_HAT0Y, 0, 767, 2, 4);
	input_set_abs_params(wdata->input, ABS_HAT1X, 0, 1023, 2, 4);
	input_set_abs_params(wdata->input, ABS_HAT1Y, 0, 767, 2, 4);
	input_set_abs_params(wdata->input, ABS_HAT2X, 0, 1023, 2, 4);
	input_set_abs_params(wdata->input, ABS_HAT2Y, 0, 767, 2, 4);
	input_set_abs_params(wdata->input, ABS_HAT3X, 0, 1023, 2, 4);
	input_set_abs_params(wdata->input, ABS_HAT3Y, 0, 767, 2, 4);

	spin_lock_init(&wdata->qlock);
	INIT_WORK(&wdata->worker, wiimote_worker);

	spin_lock_init(&wdata->state.lock);
	init_completion(&wdata->state.ready);
	mutex_init(&wdata->state.sync);

	return wdata;
}

static void wiimote_destroy(struct wiimote_data *wdata)
{
	kfree(wdata);
}

static int wiimote_hid_probe(struct hid_device *hdev,
				const struct hid_device_id *id)
{
	struct wiimote_data *wdata;
	int ret;

	wdata = wiimote_create(hdev);
	if (!wdata) {
		hid_err(hdev, "Can't alloc device\n");
		return -ENOMEM;
	}

	ret = device_create_file(&hdev->dev, &dev_attr_led1);
	if (ret)
		goto err;
	ret = device_create_file(&hdev->dev, &dev_attr_led2);
	if (ret)
		goto err;
	ret = device_create_file(&hdev->dev, &dev_attr_led3);
	if (ret)
		goto err;
	ret = device_create_file(&hdev->dev, &dev_attr_led4);
	if (ret)
		goto err;
	ret = device_create_file(&hdev->dev, &dev_attr_rumble);
	if (ret)
		goto err;
	ret = device_create_file(&hdev->dev, &dev_attr_accelerometer);
	if (ret)
		goto err;
	ret = device_create_file(&hdev->dev, &dev_attr_battery);
	if (ret)
		goto err;
	ret = device_create_file(&hdev->dev, &dev_attr_ir);
	if (ret)
		goto err;

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "HID parse failed\n");
		goto err;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_HIDRAW);
	if (ret) {
		hid_err(hdev, "HW start failed\n");
		goto err;
	}

	ret = input_register_device(wdata->input);
	if (ret) {
		hid_err(hdev, "Cannot register input device\n");
		goto err_stop;
	}

	wiimote_debugfs_init(wdata);

	/* smp_wmb: Write wdata->xy first before wdata->ready is set to 1 */
	smp_wmb();
	atomic_set(&wdata->ready, 1);
	hid_info(hdev, "New device registered\n");

	/* by default set led1 after device initialization */
	spin_lock_irq(&wdata->state.lock);
	wiiproto_req_leds(wdata, WIIPROTO_FLAG_LED1);
	spin_unlock_irq(&wdata->state.lock);

	return 0;

err_stop:
	hid_hw_stop(hdev);
err:
	input_free_device(wdata->input);
	device_remove_file(&hdev->dev, &dev_attr_led1);
	device_remove_file(&hdev->dev, &dev_attr_led2);
	device_remove_file(&hdev->dev, &dev_attr_led3);
	device_remove_file(&hdev->dev, &dev_attr_led4);
	device_remove_file(&hdev->dev, &dev_attr_rumble);
	device_remove_file(&hdev->dev, &dev_attr_accelerometer);
	device_remove_file(&hdev->dev, &dev_attr_battery);
	device_remove_file(&hdev->dev, &dev_attr_ir);
	wiimote_destroy(wdata);
	return ret;
}

static void wiimote_hid_remove(struct hid_device *hdev)
{
	struct wiimote_data *wdata = hid_get_drvdata(hdev);

	hid_info(hdev, "Device removed\n");

	wiimote_debugfs_deinit(wdata);
	device_remove_file(&hdev->dev, &dev_attr_led1);
	device_remove_file(&hdev->dev, &dev_attr_led2);
	device_remove_file(&hdev->dev, &dev_attr_led3);
	device_remove_file(&hdev->dev, &dev_attr_led4);
	device_remove_file(&hdev->dev, &dev_attr_rumble);
	device_remove_file(&hdev->dev, &dev_attr_accelerometer);
	device_remove_file(&hdev->dev, &dev_attr_battery);
	device_remove_file(&hdev->dev, &dev_attr_ir);

	hid_hw_stop(hdev);
	input_unregister_device(wdata->input);

	cancel_work_sync(&wdata->worker);
	wiimote_destroy(wdata);
}

static const struct hid_device_id wiimote_hid_devices[] = {
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_NINTENDO,
				USB_DEVICE_ID_NINTENDO_WIIMOTE) },
	{ }
};
MODULE_DEVICE_TABLE(hid, wiimote_hid_devices);

static struct hid_driver wiimote_hid_driver = {
	.name = "wiimote",
	.id_table = wiimote_hid_devices,
	.probe = wiimote_hid_probe,
	.remove = wiimote_hid_remove,
	.raw_event = wiimote_hid_event,
};

static int __init wiimote_init(void)
{
	int ret;

	ret = hid_register_driver(&wiimote_hid_driver);
	if (ret)
		pr_err("Can't register wiimote hid driver\n");

	return ret;
}

static void __exit wiimote_exit(void)
{
	hid_unregister_driver(&wiimote_hid_driver);
}

module_init(wiimote_init);
module_exit(wiimote_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Herrmann <dh.herrmann@gmail.com>");
MODULE_DESCRIPTION(WIIMOTE_NAME " Device Driver");
MODULE_VERSION(WIIMOTE_VERSION);

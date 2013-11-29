/*
 *  Connecting Wii Nunchuk remote to Raspberry Pi.
 *
 *  Copyright (C) 2013  Peter Trsko <peter.trsko@unify.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include <linux/init.h>
#include <linux/input-polldev.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>


#define POLL_INTERVAL	30	/* msec */

#define nunchuk_init_msg(c, m, b, v1, v2) \
	do { \
		b[0] = v1; \
		b[1] = v2; \
		m.addr = c->addr; \
		m.flags = c->flags; \
		m.len = 2; \
		m.buf = b; \
	} while (0)

#define MASK_BUTTON_Z	0x01
#define MASK_BUTTON_C	0x02
#define MASK_ACCEL_X	0x0C
#define MASK_ACCEL_Y	0x40
#define MASK_ACCEL_Z	0xC0

#define BUTTON_Z(a)	(a & MASK_BUTTON_Z)
#define BUTTON_C(a)	((a & MASK_BUTTON_C) >> 1)

#define ACCEL_X(a, b)	((a << 2) | ((b & MASK_ACCEL_X) >> 2))
#define ACCEL_Y(a, b)	((a << 2) | ((b & MASK_ACCEL_X) >> 4))
#define ACCEL_Z(a, b)	((a << 2) | ((b & MASK_ACCEL_X) >> 6))

struct wii_nunchuk_dev {
	struct input_polled_dev *polled_dev;
	struct i2c_client *i2c_client;
};

struct wii_nunchuk_sensors {
	int joystickX;
	int joystickY;

	int accelX;
	int accelY;
	int accelZ;

	int buttonZ;
	int buttonC;
};

static const struct i2c_device_id wii_nunchuk_id[] = {
	{ "wii_nunchuk", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, wii_nunchuk_id);

static const struct of_device_id wii_nunchuk_dt_ids[] = {
	{ .compatible = "nintendo,wii_nunchuk", },
	{ }
};
MODULE_DEVICE_TABLE(of, wii_nunchuk_dt_ids);

static int wii_nunchuk_read_regs(struct i2c_client *client,
	struct wii_nunchuk_sensors *sensors)
{
	int ret = 0;

	u8 outbuf[1];
	u8 inbuf[6];

	mdelay(10);

	outbuf[0] = 0x00;

	ret = i2c_master_send(client, outbuf, 1);
	if (ret < 0)
		goto failure;
	else if (ret != 1)
		goto io_failure;

	mdelay(10);

	ret = i2c_master_recv(client, inbuf, 6);
	if (ret < 0)
		goto failure;
	else if (ret != 6)
		goto io_failure;

	sensors->joystickX = inbuf[0];
	sensors->joystickY = inbuf[1];

	sensors->accelX = ACCEL_X(inbuf[2], inbuf[5]);
	sensors->accelY = ACCEL_Y(inbuf[3], inbuf[5]);
	sensors->accelZ = ACCEL_Z(inbuf[4], inbuf[5]);

	sensors->buttonZ = BUTTON_Z(inbuf[5]);
	sensors->buttonC = BUTTON_C(inbuf[5]);

	return 0;

 io_failure:
	ret = -EIO;

 failure:
	return ret;
}

static void poll_regs(struct input_polled_dev *dev)
{
	struct wii_nunchuk_dev *nunchuk = dev->private;
	struct input_dev *input_dev = nunchuk->polled_dev->input;
	struct wii_nunchuk_sensors sensors;
	int ret;

	ret = wii_nunchuk_read_regs(nunchuk->i2c_client, &sensors);
	if (ret < 0) {
		dev_err(&nunchuk->i2c_client->dev,
			"Error while reading registers\n");
		return;
	}

	input_report_abs(input_dev, ABS_X, sensors.joystickX);
	input_report_abs(input_dev, ABS_Y, sensors.joystickY);

	input_event(input_dev, EV_KEY, BTN_Z, sensors.buttonZ);
	input_event(input_dev, EV_KEY, BTN_C, sensors.buttonC);

	input_sync(input_dev);
}

static int wii_nunchuk_polled_input_setup(struct i2c_client *client)
{
	struct input_polled_dev *polled_dev;
	struct input_dev *input;
	struct wii_nunchuk_dev *nunchuk;
	int err;

	nunchuk = kzalloc(sizeof(struct wii_nunchuk_dev), GFP_KERNEL);
	polled_dev = input_allocate_polled_device();
	if (!nunchuk || !polled_dev) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		err = -ENOMEM;
		goto failure_free_mem;
	}

	polled_dev->private = nunchuk;
	polled_dev->poll = poll_regs;
	polled_dev->poll_interval = POLL_INTERVAL;

	input = polled_dev->input;
	input->name = "Wii Nunchuk";
	input->phys = "wii/nunchuk";
	input->id.bustype = BUS_I2C;
	input->dev.parent = &client->dev;

	nunchuk->i2c_client = client;
	nunchuk->polled_dev = polled_dev;
	i2c_set_clientdata(client, nunchuk);

	set_bit(EV_KEY, input->evbit);
	set_bit(BTN_C, input->keybit);
	set_bit(BTN_Z, input->keybit);

	set_bit(EV_ABS, input->evbit);
	input_set_abs_params(input, ABS_X, 1, 256, 0, 0);
	input_set_abs_params(input, ABS_Y, 1, 256, 0, 0);

	err = input_register_polled_device(polled_dev);
	if (err)
		goto failure_free_mem;

	return 0;

 failure_free_mem:
	input_free_polled_device(polled_dev);
	kfree(nunchuk);

	return -err;
}

static int wii_nunchuk_setup(struct i2c_client *client)
{
	struct i2c_msg msg;
	int ret = 0;
	u8 buf[2];

	nunchuk_init_msg(client, msg, buf, 0xf0, 0x55);
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
		goto failure;
	else if (ret != 1)
		goto io_failure;

	mdelay(1); /* 1ms delay between messages */

	nunchuk_init_msg(client, msg, buf, 0xfb, 0x00);
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
		goto failure;
	else if (ret != 1)
		goto io_failure;

	return 0;

 io_failure:
	ret = -EIO;

 failure:
	return ret;
}

static int wii_nunchuk_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;

	ret = wii_nunchuk_setup(client);
	if (ret < 0) {
		dev_err(&client->dev, "Device setup failed.\n");
		goto failure;
	}

	dev_info(&client->dev, "Wii Nunchuck reporting for duty!\n");

	ret = wii_nunchuk_polled_input_setup(client);
	if (ret < 0) {
		dev_err(&client->dev, "Filed to setup polled input.\n");
		goto failure;
	}

	return 0;

 failure:
	dev_err(&client->dev, "Wii Nunchuck dying in a lot of pain!\n");

	return ret;
}

static int wii_nunchuk_remove(struct i2c_client *client)
{
	struct wii_nunchuk_dev *nunchuk = i2c_get_clientdata(client);

	input_unregister_polled_device(nunchuk->polled_dev);
	input_free_polled_device(nunchuk->polled_dev);
	kfree(nunchuk);
	i2c_set_clientdata(client, NULL);

	dev_info(&client->dev, "Wii Nunchuk dying in honor!\n");

	return 0;
}

static struct i2c_driver wii_nunchuk_driver = {
	.probe = wii_nunchuk_probe,
	.remove = __devexit_p(wii_nunchuk_remove),
	.id_table = wii_nunchuk_id,
	.driver = {
		.name = "wii_nunchuk",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(wii_nunchuk_dt_ids),
	},
};

module_i2c_driver(wii_nunchuk_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Peter Trsko <peter.trsko@unify.com>");
MODULE_DESCRIPTION("Nintendo Wii Nunchuk driver.");
MODULE_VERSION("0.1");

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>


#define nunchuk_init_msg(m, c, b, v1, v2) \
	do { \
		b[0] = v1; \
		b[1] = v2; \
		m.addr = c->addr; \
		m.flags = c->flags; \
		m.len = 2; \
		m.buf = b; \
	} while (0)

#define MASK_BUTTON_Z   0x01
#define MASK_BUTTON_C   0x02
#define MASK_ACCEL_X    0x0C
#define MASK_ACCEL_Y    0x40
#define MASK_ACCEL_Z    0xC0

#define BUTTON_Z(a)     (a & MASK_BUTTON_Z)
#define BUTTON_C(a)     ((a & MASK_BUTTON_C) >> 1)

#define ACCEL_X(a, b)   ((a << 2) | ((b & MASK_ACCEL_X) >> 2))
#define ACCEL_Y(a, b)   ((a << 2) | ((b & MASK_ACCEL_X) >> 4))
#define ACCEL_Z(a, b)   ((a << 2) | ((b & MASK_ACCEL_X) >> 6))

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

static int wii_nunchuk_setup(struct i2c_client *client)
{
	struct i2c_msg msg;
	int ret = 0;
	u8 buf[2];

	nunchuk_init_msg(msg, client, buf, 0xf0, 0x55);
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
	{
		goto transfer_failure;
	}
	else if (ret != 1)
	{
		goto io_failure;
	}

	mdelay(1); /* 1ms delay between messages */

	nunchuk_init_msg(msg, client, buf, 0xfb, 0x00);
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
	{
		goto transfer_failure;
	}
	else if (ret != 1)
	{
		goto io_failure;
	}

	return 0;

 io_failure:
 	ret = -EIO;

 transfer_failure:
	dev_err(&client->dev, "Error in I2C transfer.");

	return ret;
}

static int wii_nunchuk_read_regs(struct i2c_client *client)
{
	int accelX, accelY, accelZ;
	int buttonZ, buttonC;
	int ret = 0;

	u8 outbuf[1];
	u8 inbuf[6];

	mdelay(10);

	outbuf[0] = 0x00;

	ret = i2c_master_send(client, outbuf, 1);
	if (ret < 0)
	{
		goto transfer_failure;
	}
	else if (ret != 1)
	{
		goto io_failure;
	}

	mdelay(10);

	ret = i2c_master_recv(client, inbuf, 6);
	if (ret < 0)
	{
		goto transfer_failure;
	}
	else if (ret != 6)
	{
		goto io_failure;
	}

	dev_info(&client->dev, "Joystick: %d/%d", inbuf[0], inbuf[1]);

	accelX = ACCEL_X(inbuf[2], inbuf[5]);
	accelY = ACCEL_Y(inbuf[3], inbuf[5]);
	accelZ = ACCEL_Z(inbuf[4], inbuf[5]);
	dev_info(&client->dev, "Accels: %d/%d/%d", accelX, accelY, accelZ);

	buttonZ = BUTTON_Z(inbuf[5]);
	buttonC = BUTTON_C(inbuf[5]);
	dev_info(&client->dev, "Buttons: %s/%s", (buttonZ ? "z" : "Z"),
		(buttonC ? "c" : "C"));

	return 0;

 io_failure:
 	ret = -EIO;

 transfer_failure:
	dev_err(&client->dev, "Error in I2C transfer.");

 	return ret;
}

static int __devinit wii_nunchuk_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;

	ret = wii_nunchuk_setup(client);
	if (ret < 0)
	{
		goto failure;
	}

	dev_info(&client->dev, "Wii Nunchuck reporting for duty!\n");

	ret = wii_nunchuk_read_regs(client);
	if (ret < 0)
	{
		goto failure;
	}

	return 0;

 failure:
	dev_err(&client->dev, "Wii Nunchuck dying in a lot of pain!\n");

	return ret; 
}

static int __devexit wii_nunchuk_remove(struct i2c_client *client)
{
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

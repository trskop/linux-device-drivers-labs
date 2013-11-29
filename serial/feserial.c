/*
 *  Experimenting with UART on Raspberry Pi.
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
#include <linux/module.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/serial_reg.h>
#include <linux/fs.h>
#include <linux/slab.h>


/* {{{ Driver Specific Definitions ***************************************** */

#define IOCTL_SERIAL_RESET_COUNTER 0
#define IOCTL_SERIAL_GET_COUNTER 1

struct feserial_dev {
	void __iomem *regs;
	struct platform_device *pdev;

	/* Later we use container_of() macro in misc driver's file operations
	 * to get pointer to struct feserial_dev */
	struct miscdevice miscdev;

	/* Number of characters written to UART. */
	unsigned long counter;
};

#define feserial_get_drvdata(pdev)	\
	((struct feserial_dev *)platform_get_drvdata(pdev))

/* TODO: Locking. */
#define inc_counter(d)	(d->counter++)
#define rst_counter(d)	(d->counter = 0)
#define get_counter(d)	(d->counter)

/* }}} Driver Specific Definitions ***************************************** */

/* {{{ Low-level UART Code ************************************************* */

/* void *iobase(struct feserial_dev *dev) */
#define iobase(d)	(d->regs)

/* void *ioaddr(struct feserial_dev *dev, int offset) */
#define ioaddr(d, o)	((void *)(iobase(d) + o * 4))

/* Currently unused.
static unsigned int reg_read(struct feserial_dev *dev, int offset)
{
	dev_info(&dev->pdev->dev,
		"Reading value from offset %d (iobase: %p, ioaddr: %p)\n",
		offset, iobase(dev), ioaddr(dev, offset));
	return readl(ioaddr(dev, offset));
}
*/


static void reg_write(struct feserial_dev *dev, int val, int offset)
{
	dev_info(&dev->pdev->dev,
		"Writing %d to offset %d (iobase: %p, ioaddr: %p)\n",
		val, offset, (void *)iobase(dev), (void *)ioaddr(dev, offset));
	writel(val, ioaddr(dev, offset));
}


static void uart_write_char(struct feserial_dev *dev, char ch)
{
	reg_write(dev, (int)ch, UART_TX);
	inc_counter(dev);
}


static void uart_write(struct feserial_dev *dev, const char *buf,
	unsigned int count)
{
	int i;

	for (i = 0; i < count; i++)
		uart_write_char(dev, buf[i]);
}


static void uart_setup(struct feserial_dev *dev)
{
	unsigned int uartclk;
	unsigned int baud_divisor;

	/* Configure the baud rate to 115200. */

	uartclk = 48000000;
	baud_divisor = uartclk / 16 / 115200;

	reg_write(dev, 0x07, UART_OMAP_MDR1);
	reg_write(dev, 0x00, UART_LCR);
	reg_write(dev, UART_LCR_DLAB, UART_LCR);
	reg_write(dev, baud_divisor & 0xff, UART_DLL);
	reg_write(dev, (baud_divisor >> 8) & 0xff, UART_DLM);
	reg_write(dev, UART_LCR_WLEN8, UART_LCR);

	/* Soft reset. */

	reg_write(dev, UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT, UART_FCR);
	reg_write(dev, 0x00, UART_OMAP_MDR1);
}

/* }}} Low-level UART Code ************************************************* */

/* {{{ Misc Device ********************************************************* */

#define RW_CHUNK_SIZE	32


static int feserial_open(struct inode *inode, struct file *file)
{
	return 0;
}


static ssize_t feserial_write(struct file *file, const char __user *ubuf,
	size_t count, loff_t *pos)
{
	char kbuf[RW_CHUNK_SIZE];
	unsigned int chunk_size = RW_CHUNK_SIZE;

	size_t left_to_copy = count;
	ssize_t result = 0;

	struct feserial_dev *fdev;

	fdev = container_of(file->private_data, struct feserial_dev, miscdev);

	while (left_to_copy > 0) {
		if (count < RW_CHUNK_SIZE)
			chunk_size = (unsigned int)count;

		if (copy_from_user(kbuf, ubuf, chunk_size) > 0) {
			dev_err(&fdev->pdev->dev, "copy_from_user() failed.\n");
			result = -EFAULT;
			goto failure;
		}

		left_to_copy -= chunk_size;
		uart_write(fdev, kbuf, chunk_size);
		result += chunk_size;
	}

 failure:
	return result;
}


static ssize_t feserial_read(struct file *file, char __user *buf,
	size_t count, loff_t *pos)
{
	return -EINVAL;
}


static long feserial_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	struct feserial_dev *fdev;
	unsigned long result;
	long int ret;

	fdev = container_of(file->private_data, struct feserial_dev, miscdev);

	switch (cmd) {
	case IOCTL_SERIAL_RESET_COUNTER:
		rst_counter(fdev);
		ret = 0;
		break;
	case IOCTL_SERIAL_GET_COUNTER:
		result = get_counter(fdev);
		ret = copy_to_user((unsigned long *)arg, &result,
			sizeof(unsigned long));
		if (ret)
			return -EACCES;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}


static const struct file_operations feserial_fops = {
	.owner		= THIS_MODULE,
	.open		= feserial_open,
	.read		= feserial_read,
	.write		= feserial_write,
	.unlocked_ioctl	= feserial_ioctl,
};

/* }}} Misc Device ********************************************************* */

/* {{{ Driver ************************************************************** */

static int feserial_setup(struct platform_device *pdev)
{
	struct resource *res;
	struct feserial_dev *dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Unable to get IORESOURCE_MEM.\n");
		return -1;
	}

	pr_info("IO memory resource start address: %d\n", res->start);

	dev = devm_kzalloc(&pdev->dev, sizeof(struct feserial_dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&pdev->dev,
			"Unable to allocate feserial_dev structure.\n");
		return -ENOMEM;
	}

	dev->pdev = pdev;
	rst_counter(dev);

	dev->regs = devm_request_and_ioremap(&pdev->dev, res);
	if (!dev->regs) {
		dev_err(&pdev->dev, "Cannot remap registers\n");
		return -ENOMEM;
	}

	dev->miscdev.minor = MISC_DYNAMIC_MINOR,
	dev->miscdev.name = "feserial_misc",
	dev->miscdev.fops = &feserial_fops,
	dev->miscdev.nodename =
		kasprintf(GFP_KERNEL, "feserial-%x", res->start);

	platform_set_drvdata(pdev, dev);

	return 0;
}


static void feserial_cleanup(struct feserial_dev *fdev)
{
	kfree(fdev->miscdev.nodename);
}

/* {{{ Platform Driver ***************************************************** */

static int feserial_probe(struct platform_device *pdev)
{
	struct feserial_dev *fdev;
	char evil[] = "evil\r\n";
	int ret = -1;

	pr_info("Called feserial_probe\n");

	/* Power management. */

	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	/* Initialize feserial_dev data structure and allocate resources. */

	ret = feserial_setup(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "feserial setup failed.\n");
		goto failure;
	}

	fdev = feserial_get_drvdata(pdev);
	if (!fdev) {
		dev_err(&pdev->dev, "Can't access feserial_dev structure.\n");
		goto failure;
	}

	/* Initialize UART */

	uart_setup(fdev);
	uart_write(fdev, evil, ARRAY_SIZE(evil));

	/* Misc device setup. */

	if (misc_register(&fdev->miscdev)) {
		pr_err("feserial_misc: Unable to register misc device\n");
		ret = -EIO;
		goto failure_do_cleanup;
	}

	return 0;

 failure_do_cleanup:
	feserial_cleanup(fdev);

 failure:
	return ret;
}


static int feserial_remove(struct platform_device *pdev)
{
	struct feserial_dev *fdev;

	pr_info("Called feserial_remove\n");

	fdev = feserial_get_drvdata(pdev);

	misc_deregister(&fdev->miscdev);
	/* Calling feserial_cleanup() has to be AFTER misc_deregister(), since
	 * it dealocates parts of struct miscdevice.
	 */
	feserial_cleanup(fdev);

	pm_runtime_disable(&pdev->dev);

	return 0;
}


static struct platform_driver feserial_driver = {
	.driver = {
		.name = "feserial",
		.owner = THIS_MODULE,
	},
	.probe = feserial_probe,
	.remove = feserial_remove,
};

/* }}} Platform Driver ***************************************************** */

module_platform_driver(feserial_driver)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Peter Trsko <peter.trsko@unify.com>");
MODULE_DESCRIPTION("Raspberry Pi UART driver.");
MODULE_VERSION("0.1");

/* }}} Driver ************************************************************** */

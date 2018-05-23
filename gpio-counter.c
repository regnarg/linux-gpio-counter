#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/errno.h>
 
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/jiffies.h>


#include <asm/atomic.h>
 
#define DRIVER_AUTHOR "Filip Stedronsky <r.lkml@regnarg.cz>"
#define DRIVER_DESC   "GPIO pulse counter"
#define MODNAME "gpio-counter"
#define LOGPREFIX MODNAME ": "

// Inspired by GPIO interrupt example by Igor <hardware.coder@gmail.com>
// from <http://morethanuser.blogspot.cz/2013/04/raspbery-pi-gpio-interrupts-in-kernel.html>
 

static int gpio_num = -1;
module_param_named(gpio_num, gpio_num, int, 0);
MODULE_PARM_DESC(gpio_num, "GPIO number");

static int debounce = 0;
module_param_named(debounce, debounce, int, 0);
MODULE_PARM_DESC(debounce, "debounce time (in microseconds)");

static void *devid = "gpio-counter-Gex3lieshu";// pointer used as unique cookie for register_irq
 
static short int irq_num    = 0;
static atomic64_t counter = ATOMIC64_INIT(0);
static struct device *dev;
static u64 last_pulse = 0;
static int last_value = 0;
static u64 debounce_jiffies = 0;


static ssize_t count_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	//struct watchdog_device *wdd = dev_get_drvdata(dev);

	return sprintf(buf, "%lld\n", (long long)atomic64_read(&counter));
}
static ssize_t count_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	unsigned int val;
	int ret;

	ret = kstrtouint(buf, 10, &val);
	if (ret)
		return ret;
        atomic64_set(&counter, val);
        return count;
}
static DEVICE_ATTR_RW(count);
static struct attribute *ctr_attrs[] = {
	&dev_attr_count.attr,
	NULL,
};

static const struct attribute_group ctr_group = {
	.attrs = ctr_attrs,
};
__ATTRIBUTE_GROUPS(ctr);


static struct class ctr_class = {
	.name = "gpio_counter",
	.owner = THIS_MODULE,
	.dev_groups = ctr_groups,
};

static DEFINE_SPINLOCK(ctr_irq_lock);
 
 
static irqreturn_t irq_handler(int irq, void *dev_id, struct pt_regs *regs) {
    unsigned long flags;
    spin_lock_irqsave(&ctr_irq_lock, flags);

    u64 now = get_jiffies_64();
    if (now - last_pulse > debounce_jiffies && last_value == 1) { // debounced falling edge
        atomic64_add(1, &counter);
    }

    last_pulse = now;
    last_value = gpio_get_value(gpio_num);
  
    spin_unlock_irqrestore(&ctr_irq_lock, flags);
    return IRQ_HANDLED;
}
 
 
int gpio_counter_init(void) {
    int err;
    if (gpio_num == -1) {
        printk(KERN_ERR LOGPREFIX "The parameter 'gpio_num' is required.");
        return -EINVAL;
    }
    
    if (debounce != -1)  {
        debounce_jiffies = usecs_to_jiffies(debounce);
    }
 
    printk(KERN_NOTICE LOGPREFIX "initializing\n");


    if ((err = class_register(&ctr_class)) != 0) goto err_class;

    if ((err = gpio_request(gpio_num, MODNAME))) {
        printk(KERN_ERR LOGPREFIX "GPIO request faiure: %d\n", gpio_num);
        goto err_gpio;
    }
    last_value = gpio_get_value(gpio_num);
    
    if ( (irq_num = err = gpio_to_irq(gpio_num)) < 0 ) {
        printk(KERN_ERR LOGPREFIX "GPIO to IRQ mapping faiure %d\n", gpio_num);
        goto err_irq;
    }
    
    printk(KERN_NOTICE "Mapped interrupt %d\n", irq_num);

    if ((err = request_irq(irq_num,
                    (irq_handler_t ) irq_handler,
                    IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
                    MODNAME " pulse interrupt",
                    devid))) {
        printk(KERN_ERR LOGPREFIX "Irq Request failure\n");
        goto err_irq;
    }

    dev = device_create(&ctr_class, NULL /*parent*/,
                 MKDEV(0,0), NULL, "gpio_counter_%d", gpio_num);
 
    return 0;

   
    free_irq(irq_num, devid);
err_irq:
    gpio_free(gpio_num);
err_gpio:
    class_unregister(&ctr_class);
err_class:
    return err;
}
 
void gpio_counter_cleanup(void) {
    printk(KERN_NOTICE LOGPREFIX "unloading\n");
    device_unregister(dev);
    free_irq(irq_num, devid);
    gpio_free(gpio_num);
    class_unregister(&ctr_class);
}
 
 
module_init(gpio_counter_init);
module_exit(gpio_counter_cleanup);
 
 
MODULE_LICENSE("GPL");

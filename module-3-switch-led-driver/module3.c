#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>

/*
 * AXI GPIO channel 1 register offsets.
 */
#define AXI_GPIO_DATA_OFFSET    0x00
#define AXI_GPIO_TRI_OFFSET     0x04

/*
 * SW0 and LD0 are both connected to bit 0 of their
 * respective AXI GPIO peripherals.
 */
#define SW0_MASK                0x00000001
#define LD0_MASK                0x00000001

/*
 * Private device data for the combined switch and LED device.
 */
struct zyboswled_device {
    /*
     * Mapped register area for the switch AXI GPIO:
     * physical address 0x41210000.
     */
    void __iomem *switch_base;

    /*
     * Mapped register area for the LED AXI GPIO:
     * physical address 0x41220000.
     */
    void __iomem *led_base;
};

/*
 * Linux calls probe() after matching:
 *
 * compatible = "intern,zybo-sw-led";
 */
static int zyboswled_probe(struct platform_device *pdev)
{
    struct zyboswled_device *device;
    u32 switch_direction;
    u32 led_direction;
    u32 switch_value;
    u32 led_value;

    /*
     * Allocate persistent private data for this device.
     */
    device = devm_kzalloc(&pdev->dev, sizeof(*device), GFP_KERNEL);

    if (!device)
        return -ENOMEM; 

    /*
     * Map the resource whose reg-name is "switches".
     *
     * This corresponds to physical address 0x41210000.
     */
    device->switch_base =devm_platform_ioremap_resource_byname(pdev,"switches");

    if (IS_ERR(device->switch_base))
        return PTR_ERR(device->switch_base);

    /*
     * Map the resource whose reg-name is "leds".
     *
     * This corresponds to physical address 0x41220000.
     */
    device->led_base =devm_platform_ioremap_resource_byname(pdev,"leds");

    if (IS_ERR(device->led_base))
        return PTR_ERR(device->led_base);

    /*
     * Store the private structure so remove() can retrieve it.
     */
    platform_set_drvdata(pdev, device);

    /*
     * Configure SW0 as an input.
     *
     * AXI GPIO TRI:
     *
     * 1 = input
     * 0 = output
     *
     * Setting bit 0 as 1 makes SW0 an input.
     */
    switch_direction = readl(device->switch_base +AXI_GPIO_TRI_OFFSET);

    switch_direction |= SW0_MASK;

    writel(switch_direction,device->switch_base +AXI_GPIO_TRI_OFFSET);

    /*
     * Configure LD0 as an output.
     *
     * Clearing bit 0 configures LD0 as an output.
     */
    led_direction =readl(device->led_base + AXI_GPIO_TRI_OFFSET);

    led_direction &= ~LD0_MASK;

    writel(led_direction,  device->led_base +AXI_GPIO_TRI_OFFSET);

    /*
     * Read the switch AXI GPIO DATA register.
     */
    switch_value = readl(device->switch_base +  AXI_GPIO_DATA_OFFSET);

    /*
     * Read the current LED output register so unrelated
     * LED bits remain unchanged.
     */
    led_value =readl(device->led_base + AXI_GPIO_DATA_OFFSET);

    /*
     * Set LD0 according to SW0.
     */
    if (switch_value & SW0_MASK) {
        /*
         * SW0 is on, so set LD0's bit.
         */
        led_value |= LD0_MASK;

        dev_info(&pdev->dev,  "SW0 is ON; LD0 turned ON\n");
    } 
    else {
        /*
         * SW0 is off, so clear LD0's bit.
         */
        led_value &= ~LD0_MASK;

        dev_info(&pdev->dev, "SW0 is OFF; LD0 turned OFF\n");
    }

    /*
     * Send the new LD0 state to the LED hardware.
     */
    writel(led_value,device->led_base + AXI_GPIO_DATA_OFFSET);

    return 0;
}

/*
 * Turn LD0 off before the module is removed.
 */
static int zyboswled_remove(struct platform_device * pdev)
{
    struct zyboswled_device *device;
    u32 led_value;

    device = platform_get_drvdata(pdev);


    // Turn off LD0 before removing the driver
    led_value = readl(device->led_base +AXI_GPIO_DATA_OFFSET);

    led_value = led_value & ~LD0_MASK;

    writel(led_value,device->led_base + AXI_GPIO_DATA_OFFSET);



    dev_info(&pdev->dev,"LD0 turned off and driver removed\n");

    return 0;
}

/*
 * Device Tree matching table.
 */
static const struct of_device_id zyboswled_of_match[] = {
    {
        .compatible = "intern,zybo-sw-led",
    },
    {
        /* End-of-table marker. */
    }
};

MODULE_DEVICE_TABLE(of, zyboswled_of_match);

/*
 * Platform-driver definition.
 */
static struct platform_driver zyboswled_driver = {
    .probe = zyboswled_probe,
    .remove = zyboswled_remove,

    .driver = {
        .name = "zyboswled",
        .of_match_table = zyboswled_of_match,
    },
};

module_platform_driver(zyboswled_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Arda");
MODULE_DESCRIPTION("Kernel-only Zybo Z7 SW0 to LD0 driver");
MODULE_VERSION("1.1");
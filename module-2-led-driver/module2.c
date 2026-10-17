#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/slab.h> // Kernel memory allocation helpers
/*
 * AXI GPIO channel 1 register offsets.
 *
 * The AXI GPIO physical base address is obtained from
 * the Device Tree:
 *
 * reg = <0x41220000 0x10000>;
 *
 * DATA physical address = 0x41220000 + 0x00
 * TRI  physical address = 0x41220000 + 0x04
 */
#define AXI_GPIO_DATA_OFFSET    0x00 // Output data: bit 0 controls LD0
#define AXI_GPIO_TRI_OFFSET     0x04 // GPIO direction: 0 = output, 1 = input

/*
 * LD0 is connected to bit 0 of the AXI GPIO peripheral.
 */
#define LD0_MASK                0x1

/*
* Stores the mapped I/O base address for the LED device.
*/
struct zyboled_device { 
    void __iomem *base;
};

/*
 * Linux calls this function after the driver is registered,
 * an enabled platform device is found, and the Device Tree
 * compatible string matches this driver.
 */
static int zyboled_probe(struct platform_device *pdev){
    void __iomem *base; // Mapped I/O base address
    u32 direction_value;
    u32 data_value;
    // AXI GPIO registers are 32 bits wide, so use u32 for register values.
    /*
     * Obtain the first memory resource from the platform device
     * and map it into the kernel's virtual address space.
     *
     * Resource 0 comes from:
     *
     * reg = <0x41220000 0x10000>;
     */
    base = devm_platform_ioremap_resource(pdev, 0); // Returns the mapped I/O address or an encoded error pointer.
    /*
     * Kernel functions can return encoded error pointers.
     * Check whether the mapping failed.
     */
    if (IS_ERR(base))   return PTR_ERR(base);
    /*
     * Read the AXI GPIO channel 1 TRI register.
     *
     * AXI GPIO direction:
     *
     * 0 = output
     * 1 = input
     */
    direction_value = readl(base + AXI_GPIO_TRI_OFFSET);
    /*
     * Clear bit 0 so LD0 is configured as an output.
     * Only bit 0 is modified. The direction of the other
     * GPIO pins remains unchanged.
     */
    direction_value = direction_value & ~LD0_MASK; // Clear only bit 0 to configure LD0 as output
    /*
     * Write the updated direction value back to the hardware register.
     */
    writel(direction_value, base + AXI_GPIO_TRI_OFFSET);
    /*
     * Read the current AXI GPIO DATA register.
     * Read the current value first so the state of the other GPIO pins is preserved.
     */
    data_value = readl(base + AXI_GPIO_DATA_OFFSET);
    /*
     * Set bit 0 to turn LD0 on.
     *
     * The other LED bits remain unchanged.
     */
    data_value = data_value | LD0_MASK;
    /*
     * Write the modified value back to the DATA register.
     */
    writel(data_value, base + AXI_GPIO_DATA_OFFSET);
    /*
     * Read the registers again so that the values printed
     * below represent the values currently in the hardware.
     */
    direction_value = readl(base + AXI_GPIO_TRI_OFFSET);
    data_value = readl(base + AXI_GPIO_DATA_OFFSET);
    dev_info(&pdev->dev, "LD0 turned on: DATA=0x%08x TRI=0x%08x\n", data_value, direction_value);
    return 0;
}


/*
 * Linux calls this when the driver is detached from the device,
 * for example when running:
 *
 * rmmod zyboled
 *
 * At the current stage this function does not turn LD0 off.
 */
static int zyboled_remove(struct platform_device *pdev)
{
    dev_info(&pdev->dev,"Custom Zybo LED driver removed\n");

    return 0;
}


/*
 * Device Tree matching table.
 *
 * This compatible string must exactly match the compatible
 * string in system-user.dtsi:
 *
 * compatible = "intern,zybo-led";
 */
static const struct of_device_id zyboled_of_match[] = {
    {
        .compatible = "intern,zybo-led",
    },
    {}
};

MODULE_DEVICE_TABLE(of, zyboled_of_match);


/*
 * Platform-driver description.
 */
static struct platform_driver zyboled_driver = {
    .probe = zyboled_probe,
    .remove = zyboled_remove,

    .driver = {
        .name = "zyboled",
        .of_match_table = zyboled_of_match,
    },
};


/*
 * Generate the module initialization and cleanup functions.
 *
 * Loading the module registers zyboled_driver.
 * Removing the module unregisters zyboled_driver.
 */
module_platform_driver(zyboled_driver);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Arda");
MODULE_DESCRIPTION("Custom Zybo Z7 AXI GPIO LED platform driver");
MODULE_VERSION("1.0");
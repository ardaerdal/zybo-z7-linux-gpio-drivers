#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>

#define AXI_GPIO_DATA_OFFSET    0x0
#define AXI_GPIO_TRI_OFFSET     0x4

#define SW0_MASK                0x1
#define LD0_MASK                0x1

#define POLL_INTERVAL_MS        50 // Poll every 50 ms 

struct zyboswled_device {
    void __iomem *switch_base;
    void __iomem *led_base;

    /*
     * Kernel delayed work used for periodic polling.
     * INIT_DELAYED_WORK() uses struct delayed_work.
     * It contains an internal work_struct used by the polling callback.
     */
    struct delayed_work poll_work;

    /*
     * Needed for dev_info() messages from the work function.
     */
    struct device *dev;

    /*
     * Previous SW0 state.
     *
     * This lets us update and print only when SW0 changes.
     */
    bool previous_sw0_state; // Last observed SW0 state 

};

// Helper function for updating LD0

/*
 * Set LD0 without changing the other LED bits.
 */
static void zyboswled_set_ld0(struct zyboswled_device *device, bool turn_on)
{
    u32 led_value;

    led_value = readl(device->led_base + 0x00); // Read the current LED DATA register

    if (turn_on)
        led_value |= LD0_MASK; // Set the LD0 bit
    else // Clear the LD0 bit
        led_value &= ~LD0_MASK;

    writel(led_value, device->led_base + 0x00); // Write the updated DATA value
}

/*
 * This function runs every 50 milliseconds.
 */
// Periodic polling function
static void zyboswled_poll_function(struct work_struct * work)
{
    struct delayed_work *delayed_work;// Convert the callback's work_struct back to delayed_work
    struct zyboswled_device *device;
    u32 switch_value;
    bool current_sw0_state;

    /*
     * Convert work_struct back to delayed_work.
     */
    delayed_work = to_delayed_work(work);

    /*
     * Find the zyboswled_device structure that contains
     * this delayed_work object by using its poll_work member.
     */
    device = container_of(delayed_work, struct zyboswled_device, poll_work);

    /*
     * Read the AXI GPIO switch DATA register.
     */
    switch_value = readl(device->switch_base + 0X00);

    current_sw0_state = (bool)(switch_value & SW0_MASK);

    /*
     * Only update the LED when the switch state changes.
     */
    if (current_sw0_state != device->previous_sw0_state) {
        zyboswled_set_ld0(device, current_sw0_state);

        device->previous_sw0_state = current_sw0_state;

        if (current_sw0_state)  dev_info(device->dev, "SW0 changed to ON; LD0 turned ON\n");
        else dev_info(device->dev, "SW0 changed to OFF; LD0 turned OFF\n");
    }

    /*
     * Schedule this function to run again after 50 ms.
     * For continuous polling, schedule the work again at the end of the function.
     */
    schedule_delayed_work(&device->poll_work, msecs_to_jiffies(POLL_INTERVAL_MS));
}


/*
 * Linux calls this function when the driver matches the Device Tree entry.
 */
static int zyboswled_probe(struct platform_device * pdev)
{
    struct zyboswled_device *device; // Private device data used to access the mapped I/O registers
    
    // Temporary values used while configuring and reading the GPIO registers
    u32 switch_direction; 
    u32 led_direction;
    u32 switch_value;
    bool initial_sw0_state;     // Initial SW0 state 

    device = devm_kzalloc(&pdev->dev, sizeof(*device) , GFP_KERNEL); // Allocate zero-initialized private device data

    if (!device)    return -ENOMEM;  // Check whether the allocation succeeded

    device->dev = &pdev->dev;

    /*
     * Map the switch AXI GPIO resource.
     */
    device->switch_base = devm_platform_ioremap_resource_byname(pdev, "switches"); // Map the resource defined in the Device Tree

    if (IS_ERR(device->switch_base))    return PTR_ERR(device->switch_base);

    /*
     * Map the LED AXI GPIO resource.
     */
    device->led_base = devm_platform_ioremap_resource_byname(pdev, "leds"); // Map the resource defined in the Device Tree

    // Debug: print the mapped I/O addresses
    dev_info(&pdev->dev, "device->led_base: %px\n", device->led_base);
    dev_info(&pdev->dev, "device->switch_base: %px\n", device->switch_base);

    if (IS_ERR(device->led_base))   return PTR_ERR(device->led_base);

    platform_set_drvdata(pdev, device); // Associate the private device data with this platform device

    /*
     * Configure switch0 as input.
     *
     * AXI GPIO TRI: 0x04
     *     1 = input
     *     0 = output
     */
    switch_direction = readl(device->switch_base + 0x04);

    switch_direction |= SW0_MASK; // Set bit 0 to configure SW0 as input

    writel(switch_direction, device->switch_base + 0x04);// Write the updated direction value

    /*
     * Configure led0 as output.
     */
    led_direction = readl(device->led_base + AXI_GPIO_TRI_OFFSET);

    led_direction &= ~LD0_MASK; // Clear bit 0 while preserving the other direction bits 

    writel(led_direction, device->led_base + AXI_GPIO_TRI_OFFSET);


    // Initialize LD0 from the current SW0 state
     
    /*
     * Read and apply the initial SW0 state.
     */
    switch_value =readl(device->switch_base + 0x00);

    // Use bit 0 to determine whether SW0 is on or off
    initial_sw0_state = (bool)(switch_value & SW0_MASK);  // Convert the masked SW0 bit to a boolean state

    zyboswled_set_ld0(device, initial_sw0_state);

    device->previous_sw0_state = initial_sw0_state;

    if (initial_sw0_state)
        dev_info(&pdev->dev, "SW0 is initially ON; LD0 turned ON\n");
    else
        dev_info(&pdev->dev, "SW0 is initially OFF; LD0 turned OFF\n");

    /*
     * Initialize and start periodic polling.
     */
    INIT_DELAYED_WORK(&device->poll_work, zyboswled_poll_function); // Initialize the delayed work item with the polling function

    schedule_delayed_work(&device->poll_work, msecs_to_jiffies(50)); // Schedule the first poll after 50 ms

    dev_info(&pdev->dev, "SW0 polling started with %d ms interval\n", POLL_INTERVAL_MS);

    return 0;
}

static int zyboswled_remove(struct platform_device *pdev)
{
    struct zyboswled_device *device;

    device = platform_get_drvdata(pdev);

    /*
     * Stop polling before removing the driver.
     */
    cancel_delayed_work_sync(&device->poll_work);

    zyboswled_set_ld0(device, false);

    dev_info(&pdev->dev, "Polling stopped, LD0 turned off, driver removed\n");

    return 0;
}

static const struct of_device_id zyboswled_of_match[] = {
    {
        .compatible = "intern,zybo-sw-led",
    },
    {
        /* End-of-table marker. */
    }
};

MODULE_DEVICE_TABLE(of, zyboswled_of_match);

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
MODULE_DESCRIPTION("Zybo Z7 SW0 to LD0 polling driver");
MODULE_VERSION("1.3");
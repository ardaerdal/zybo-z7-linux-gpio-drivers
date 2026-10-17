#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/bitops.h>

#define AXI_GPIO_DATA_OFFSET     0x0
#define AXI_GPIO_TRI_OFFSET      0x4
#define AXI_GPIO_DATA2_OFFSET    0x8 // Channel 2 DATA register used for buttons
#define AXI_GPIO_TRI2_OFFSET     0xC

#define SW0_MASK                 0x1
#define SW1_MASK                 0x2
#define SW2_MASK                 0x4
#define SW3_MASK                 0x8

#define BTN0_MASK                0x1
#define BTN1_MASK                0x2
#define BTN2_MASK                0x4
#define BTN3_MASK                0x8

#define LD0_MASK                 0x1
#define LD1_MASK                 0x2
#define LD2_MASK                 0x4
#define LD3_MASK                 0x8

// Masks used to extract the current switch, LED, and button states
#define SW_MASKS                 (SW0_MASK | SW1_MASK | SW2_MASK | SW3_MASK)
#define LD_MASKS                 (LD0_MASK | LD1_MASK | LD2_MASK | LD3_MASK)
#define BTN_MASKS                (BTN0_MASK | BTN1_MASK | BTN2_MASK | BTN3_MASK)

#define POLL_INTERVAL_MS         50
#define NUM_SWITCHES             4 // Number of switches
#define NUM_BUTTONS              4

// A global device pointer is not needed because container_of() retrieves it from poll_work. 
// This keeps the device state associated with the delayed work item instead.
struct zyboswled_device {
    void __iomem * switch_base; // AXI GPIO base used for switches and buttons
    void __iomem * led_base;

    /*
     * Kernel delayed work used for periodic polling.
     * INIT_DELAYED_WORK() uses struct delayed_work for periodic polling.
     */
    struct delayed_work poll_work;

    /*
     * Needed for dev_info() messages from the work function.
     */
    struct device *dev;

    /*
     * Previous switch state.
     *
     * This lets us update and print only when a switch changes.
     */
    u32 previous_sw_state;
    u32 previous_btn_state;

    // Previous button state is tracked separately

};

/*
 * Set the LEDs without changing unrelated output bits.
 * led_mask contains the desired state of the LED bits.
 */
static void zyboswled_set_ld(struct zyboswled_device *device, u32 led_mask) 
{
    u32 led_value;

    led_value = readl( device->led_base + AXI_GPIO_DATA_OFFSET); // Read the current LED DATA register

    led_value &= ~LD_MASKS; // Clear the LED bits before applying the new state
    led_value |= (led_mask & LD_MASKS); // Set the requested LED bits

    writel(led_value, device->led_base + AXI_GPIO_DATA_OFFSET); // Write the updated DATA value
}


/*
 * Runs every 50 milliseconds.
 */
static void zyboswled_poll_function(struct work_struct * work)
{
    struct delayed_work * delayed_work;// Convert the callback's work_struct back to delayed_work
    struct zyboswled_device * device;

    u32 switch_value, current_sw_state, changed_sw_state;
    int i;

    u32 switch_btn_value, current_btn_state ,changed_btn_state;

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
    switch_value = readl(device->switch_base + AXI_GPIO_DATA_OFFSET); // Read the current switch value

    current_sw_state = (switch_value & SW_MASKS); // Keep only the switch bits
    changed_sw_state = current_sw_state ^ device->previous_sw_state; // XOR identifies which switch bits changed

    device->previous_sw_state = current_sw_state;

    switch_btn_value = readl(device->switch_base + AXI_GPIO_DATA2_OFFSET); // Read the current button value
    current_btn_state = (switch_btn_value & BTN_MASKS);
    changed_btn_state = current_btn_state ^ device->previous_btn_state; 

    if((changed_btn_state != 0) || (changed_sw_state != 0)){

        zyboswled_set_ld(device, (current_sw_state | current_btn_state));  
     
    }
    device->previous_btn_state = current_btn_state;
    /*
     * Schedule this function to run again after 50 ms.
     */
    schedule_delayed_work(&device->poll_work, msecs_to_jiffies(POLL_INTERVAL_MS));
}


/*
 * Linux calls this function when the driver matches the Device Tree entry.
 */
static int zyboswled_probe(struct platform_device * pdev)
{
    struct zyboswled_device *device; // Private device data used to access the mapped I/O registers
    
    u32 led_direction;

    u32 switch_direction;
    u32 switch_value, current_sw_state;

    u32 btn_direction;
    u32 btn_value, current_btn_state;

    device = devm_kzalloc(&pdev->dev, sizeof(*device) , GFP_KERNEL); // Allocate zero-initialized private device data
    // devm_kzalloc() is used so the allocation is managed automatically.

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

    if (IS_ERR(device->led_base))   return PTR_ERR(device->led_base);

    platform_set_drvdata(pdev, device); // Associate the private device data with this platform device

    /*
     * Configure switches and buttons as input.
     *
     * AXI GPIO TRI: 0x04
     *     1 = input
     *     0 = output
     */
    switch_direction = readl(device->switch_base + AXI_GPIO_TRI_OFFSET);

    switch_direction |= SW_MASKS;

    writel(switch_direction, device->switch_base + AXI_GPIO_TRI_OFFSET);// Write the updated direction value


    btn_direction = readl(device->switch_base + AXI_GPIO_TRI2_OFFSET);

    btn_direction |= BTN_MASKS;

    writel(btn_direction, device->switch_base + AXI_GPIO_TRI2_OFFSET);

    /*
     * Configure LEDs as output.
     */
    led_direction = readl(device->led_base + AXI_GPIO_TRI_OFFSET);

    led_direction &= ~LD_MASKS; // Clear the LED direction bits while preserving unrelated bits 

    writel(led_direction, device->led_base + AXI_GPIO_TRI_OFFSET);

    /*
     * Read and apply the initial switch and button state.
     */
    switch_value = readl(device->switch_base + AXI_GPIO_DATA_OFFSET); // Read the current switch value
    current_sw_state = (switch_value & SW_MASKS);

    btn_value = readl(device->switch_base + AXI_GPIO_DATA2_OFFSET); // Read the current button value
    current_btn_state = (btn_value & BTN_MASKS);
    
    device->previous_sw_state = current_sw_state;
    device->previous_btn_state = current_btn_state;

    zyboswled_set_ld(device, (current_sw_state | current_btn_state) );

    // For switches
    if (current_sw_state & SW0_MASK)
        dev_info(&pdev->dev, "SW0 is initially ON; LD0 turned ON\n");
    else
        dev_info(&pdev->dev, "SW0 is initially OFF; LD0 turned OFF\n");
        
    if (current_sw_state & SW1_MASK)
        dev_info(&pdev->dev, "SW1 is initially ON; LD1 turned ON\n");
    else
        dev_info(&pdev->dev, "SW1 is initially OFF; LD1 turned OFF\n");

    if (current_sw_state & SW2_MASK)
        dev_info(&pdev->dev, "SW2 is initially ON; LD2 turned ON\n");
    else
        dev_info(&pdev->dev, "SW2 is initially OFF; LD2 turned OFF\n");

    if (current_sw_state & SW3_MASK)
        dev_info(&pdev->dev, "SW3 is initially ON; LD3 turned ON\n");
    else
        dev_info(&pdev->dev, "SW3 is initially OFF; LD3 turned OFF\n");

    // For buttons
    if (current_btn_state & BTN0_MASK)
        dev_info(&pdev->dev, "BTN0 is initially ON; LD0 turned ON\n");
    else
        dev_info(&pdev->dev, "BTN0 is initially OFF; LD0 turned OFF\n");
        
    if (current_btn_state & BTN1_MASK)
        dev_info(&pdev->dev, "BTN1 is initially ON; LD1 turned ON\n");
    else
        dev_info(&pdev->dev, "BTN1 is initially OFF; LD1 turned OFF\n");

    if (current_btn_state & BTN2_MASK)
        dev_info(&pdev->dev, "BTN2 is initially ON; LD2 turned ON\n");
    else
        dev_info(&pdev->dev, "BTN2 is initially OFF; LD2 turned OFF\n");

    if (current_btn_state & BTN3_MASK)
        dev_info(&pdev->dev, "BTN3 is initially ON; LD3 turned ON\n");
    else
        dev_info(&pdev->dev, "BTN3 is initially OFF; LD3 turned OFF\n");
    
    /*
     * Initialize and start periodic polling.
     */
    INIT_DELAYED_WORK(&device->poll_work, zyboswled_poll_function); // Initialize the delayed work item with the polling function

    schedule_delayed_work(&device->poll_work, msecs_to_jiffies(POLL_INTERVAL_MS)); // Schedule the first poll after 50 ms

    dev_info(&pdev->dev, "Switch polling started with %d ms interval\n", POLL_INTERVAL_MS);

    // No manual kfree() is needed because devm_kzalloc() is device-managed.

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

    zyboswled_set_ld(device, 0); // Turn off all LEDs

    dev_info(&pdev->dev, "Polling stopped, LEDs turned off, driver removed\n");

    return 0;
}

static const struct of_device_id zyboswled_of_match[] = {
    {
        .compatible = "intern,zybo-sw-led",
    },
    {
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
MODULE_VERSION("1.4");
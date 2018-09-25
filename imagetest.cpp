
#include <vulkan/vulkan.h>

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>

// global
static VkInstance instance = 0;
static VkPhysicalDevice physicalDevice = 0;
static VkDevice device = 0;
static uint32_t queueFamilyIndex = -1;// compute queue

static uint32_t memoryTypeIndex_devicelocal = -1;// device local
static uint32_t memoryTypeIndex_hostvisible = -1;// host visible

std::string read_file(const char* path)
{
    FILE* fp = fopen(path, "rb");
    if (!fp)
    {
        fprintf(stderr, "fopen %s failed\n", path);
        return std::string();
    }

    fseek(fp, 0, SEEK_END);
    int len = ftell(fp);
    rewind(fp);

    std::string data;
    data.resize(len);

    fread((void*)data.data(), 1, len, fp);

    fclose(fp);

    return data;
}

VkDevice get_gpu_device()
{
    return device;
}

uint32_t get_gpu_queueFamilyIndex()
{
    return queueFamilyIndex;
}

uint32_t get_gpu_device_local_memoryTypeIndex()
{
    return memoryTypeIndex_devicelocal;
}

uint32_t get_gpu_host_visible_memoryTypeIndex()
{
    return memoryTypeIndex_hostvisible;
}

static uint32_t find_device_local_memory(VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties)
{
    // first try, device local only
    for (uint32_t j=0; j<physicalDeviceMemoryProperties.memoryTypeCount; j++)
    {
        const VkMemoryType& memoryType = physicalDeviceMemoryProperties.memoryTypes[j];

        if (memoryType.propertyFlags == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        {
            return j;
        }
    }

    // second try, with device local bit
    for (uint32_t j=0; j<physicalDeviceMemoryProperties.memoryTypeCount; j++)
    {
        const VkMemoryType& memoryType = physicalDeviceMemoryProperties.memoryTypes[j];

        if (memoryType.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        {
            return j;
        }
    }

    fprintf(stderr, "no device local memory\n");
    return -1;
}

static uint32_t find_host_visible_memory(VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties)
{
    // first try, host visible + host coherent + device local
    for (uint32_t j=0; j<physicalDeviceMemoryProperties.memoryTypeCount; j++)
    {
        const VkMemoryType& memoryType = physicalDeviceMemoryProperties.memoryTypes[j];

        if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            && (memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
            && (memoryType.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            return j;
        }
    }

    // second try, host visible + host coherent
    for (uint32_t j=0; j<physicalDeviceMemoryProperties.memoryTypeCount; j++)
    {
        const VkMemoryType& memoryType = physicalDeviceMemoryProperties.memoryTypes[j];

        if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            && (memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            return j;
        }
    }

    // third try, with host visible bit
    for (uint32_t j=0; j<physicalDeviceMemoryProperties.memoryTypeCount; j++)
    {
        const VkMemoryType& memoryType = physicalDeviceMemoryProperties.memoryTypes[j];

        if (memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        {
            return j;
        }
    }

    fprintf(stderr, "no host visible memory\n");
    return -1;
}

int init_gpu_device()
{
    VkResult ret;

    VkApplicationInfo applicationInfo;
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pNext = 0;
    applicationInfo.pApplicationName = "vkconv";
    applicationInfo.applicationVersion = 0;
    applicationInfo.pEngineName = "ncnn";
    applicationInfo.engineVersion = 20180710;
    applicationInfo.apiVersion = VK_MAKE_VERSION(1, 0, 0);

    VkInstanceCreateInfo instanceCreateInfo;
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pNext = 0;
    instanceCreateInfo.flags = 0;
    instanceCreateInfo.pApplicationInfo = &applicationInfo;
    instanceCreateInfo.enabledLayerCount = 0;
    instanceCreateInfo.ppEnabledLayerNames = 0;
    instanceCreateInfo.enabledExtensionCount = 0;
    instanceCreateInfo.ppEnabledExtensionNames = 0;

//     VkInstance instance;
    ret = vkCreateInstance(&instanceCreateInfo, 0, &instance);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkCreateInstance failed %d\n", ret);
    }

    uint32_t physicalDeviceCount = 0;
    ret = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, 0);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkEnumeratePhysicalDevices failed %d\n", ret);
    }

    fprintf(stderr, "physicalDeviceCount = %u\n", physicalDeviceCount);

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);

    ret = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkEnumeratePhysicalDevices failed %d\n", ret);
    }

    // TODO device param
    // TODO queue param
    // find proper device and queue
    uint32_t physicalDeviceIndex = -1;
//     uint32_t queueFamilyIndex = -1;

    for (uint32_t i=0; i<physicalDeviceCount; i++)
    {
        const VkPhysicalDevice& physicalDevice = physicalDevices[i];

        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

        fprintf(stderr, "[%u] apiVersion = %u.%u.%u\n", i, VK_VERSION_MAJOR(physicalDeviceProperties.apiVersion), VK_VERSION_MINOR(physicalDeviceProperties.apiVersion), VK_VERSION_PATCH(physicalDeviceProperties.apiVersion));
        fprintf(stderr, "[%u] driverVersion = %u.%u.%u\n", i, VK_VERSION_MAJOR(physicalDeviceProperties.driverVersion), VK_VERSION_MINOR(physicalDeviceProperties.driverVersion), VK_VERSION_PATCH(physicalDeviceProperties.driverVersion));
        fprintf(stderr, "[%u] vendorID = %x\n", i, physicalDeviceProperties.vendorID);
        fprintf(stderr, "[%u] deviceID = %x\n", i, physicalDeviceProperties.deviceID);
//         fprintf(stderr, "deviceType = %u\n", physicalDeviceProperties.deviceType);
        fprintf(stderr, "[%u] deviceName = %s\n", i, physicalDeviceProperties.deviceName);
//         fprintf(stderr, "pipelineCacheUUID = %u\n", physicalDeviceProperties.pipelineCacheUUID);

        if (physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
        {
            fprintf(stderr, "[%u] deviceType = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU\n", i);
        }
        if (physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            fprintf(stderr, "[%u] deviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU\n", i);
        }
        if (physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
        {
            fprintf(stderr, "[%u] deviceType = VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU\n", i);
        }
        if (physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
        {
            fprintf(stderr, "[%u] deviceType = VK_PHYSICAL_DEVICE_TYPE_CPU\n", i);
        }

        if (physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
        {
            continue;
        }

        physicalDeviceIndex = i;

        // TODO check limits
        fprintf(stderr, "maxImageDimension1D = %u\n", physicalDeviceProperties.limits.maxImageDimension1D);
        fprintf(stderr, "maxImageDimension2D = %u\n", physicalDeviceProperties.limits.maxImageDimension2D);
        fprintf(stderr, "maxImageDimension3D = %u\n", physicalDeviceProperties.limits.maxImageDimension3D);

        fprintf(stderr, "maxComputeSharedMemorySize = %u\n", physicalDeviceProperties.limits.maxComputeSharedMemorySize);
        fprintf(stderr, "maxComputeWorkGroupCount = %u %u %u\n", physicalDeviceProperties.limits.maxComputeWorkGroupCount[0], physicalDeviceProperties.limits.maxComputeWorkGroupCount[1], physicalDeviceProperties.limits.maxComputeWorkGroupCount[2]);
        fprintf(stderr, "maxComputeWorkGroupInvocations = %u\n", physicalDeviceProperties.limits.maxComputeWorkGroupInvocations);
        fprintf(stderr, "maxComputeWorkGroupSize = %u %u %u\n", physicalDeviceProperties.limits.maxComputeWorkGroupSize[0], physicalDeviceProperties.limits.maxComputeWorkGroupSize[1], physicalDeviceProperties.limits.maxComputeWorkGroupSize[2]);


        // TODO check features
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(physicalDevice, &features);

        // TODO check formatProperties
        VkFormat format = VK_FORMAT_R32_SFLOAT;
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);

        uint32_t queueFamilyPropertiesCount;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, 0);

        fprintf(stderr, "queueFamilyPropertiesCount = %u\n", queueFamilyPropertiesCount);

        std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertiesCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, queueFamilyProperties.data());

        for (uint32_t j=0; j<queueFamilyPropertiesCount; j++)
        {
            const VkQueueFamilyProperties& queueFamilyProperty = queueFamilyProperties[j];

            if (queueFamilyProperty.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                fprintf(stderr, "[%u] VK_QUEUE_GRAPHICS_BIT\n", j);
            }
            if (queueFamilyProperty.queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                fprintf(stderr, "[%u] VK_QUEUE_COMPUTE_BIT\n", j);
            }
            if (queueFamilyProperty.queueFlags & VK_QUEUE_TRANSFER_BIT)
            {
                fprintf(stderr, "[%u] VK_QUEUE_TRANSFER_BIT\n", j);
            }
            if (queueFamilyProperty.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
            {
                fprintf(stderr, "[%u] VK_QUEUE_SPARSE_BINDING_BIT\n", j);
            }
        }

        // first try, compute only queue
        for (uint32_t j=0; j<queueFamilyPropertiesCount; j++)
        {
            const VkQueueFamilyProperties& queueFamilyProperty = queueFamilyProperties[j];

            if ((queueFamilyProperty.queueFlags & VK_QUEUE_COMPUTE_BIT) && !(queueFamilyProperty.queueFlags & VK_QUEUE_GRAPHICS_BIT))
            {
                queueFamilyIndex = j;
                break;
            }
        }

        if (queueFamilyIndex == (uint32_t)-1)
        {
            // second try, any queue with compute
            for (uint32_t j=0; j<queueFamilyPropertiesCount; j++)
            {
                const VkQueueFamilyProperties& queueFamilyProperty = queueFamilyProperties[j];

                if (queueFamilyProperty.queueFlags & VK_QUEUE_COMPUTE_BIT)
                {
                    queueFamilyIndex = j;
                    break;
                }
            }
        }

        if (queueFamilyIndex == (uint32_t)-1)
        {
            fprintf(stderr, "no compute queue on device %u\n", i);
            continue;
        }

        // TODO check memory info
        VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);

        fprintf(stderr, "memoryTypeCount = %u\n", physicalDeviceMemoryProperties.memoryTypeCount);
        for (uint32_t j=0; j<physicalDeviceMemoryProperties.memoryTypeCount; j++)
        {
            const VkMemoryType& memoryType = physicalDeviceMemoryProperties.memoryTypes[j];

            fprintf(stderr, "[%u] %u\n", j, memoryType.heapIndex);
            if (memoryType.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            {
                fprintf(stderr, "    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT\n");
            }
            if (memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            {
                fprintf(stderr, "    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT\n");
            }
            if (memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
            {
                fprintf(stderr, "    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT\n");
            }
            if (memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
            {
                fprintf(stderr, "    VK_MEMORY_PROPERTY_HOST_CACHED_BIT\n");
            }
            if (memoryType.propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
            {
                fprintf(stderr, "    VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT\n");
            }
        }

        fprintf(stderr, "memoryHeapCount = %u\n", physicalDeviceMemoryProperties.memoryHeapCount);
        for (uint32_t j=0; j<physicalDeviceMemoryProperties.memoryHeapCount; j++)
        {
            const VkMemoryHeap& memoryHeap = physicalDeviceMemoryProperties.memoryHeaps[j];

            fprintf(stderr, "[%u] %lu\n", j, memoryHeap.size);
            if (memoryHeap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            {
                fprintf(stderr, "    VK_MEMORY_HEAP_DEVICE_LOCAL_BIT\n");
            }
        }

        // find memory type index
        memoryTypeIndex_devicelocal = find_device_local_memory(physicalDeviceMemoryProperties);
        memoryTypeIndex_hostvisible = find_host_visible_memory(physicalDeviceMemoryProperties);
        if (memoryTypeIndex_devicelocal == (uint32_t)-1 || memoryTypeIndex_hostvisible == (uint32_t)-1)
        {
            fprintf(stderr, "no valid memoryTypeIndex_devicelocal or memoryTypeIndex_hostvisible\n");
            continue;
        }

        break;
    }

    fprintf(stderr, "----- select physicalDevice %u queueFamilyProperty %u \n", physicalDeviceIndex, queueFamilyIndex);
    fprintf(stderr, "----- select memoryTypeIndex_devicelocal %u memoryTypeIndex_hostvisible %u\n", memoryTypeIndex_devicelocal, memoryTypeIndex_hostvisible);

    physicalDevice = physicalDevices[physicalDeviceIndex];


    // get device extension
    uint32_t deviceExtensionPropertyCount = 0;
    ret = vkEnumerateDeviceExtensionProperties(physicalDevice, NULL, &deviceExtensionPropertyCount, NULL);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkEnumerateDeviceExtensionProperties failed %d\n", ret);
    }

    fprintf(stderr, "deviceExtensionPropertyCount = %d\n", deviceExtensionPropertyCount);

    std::vector<VkExtensionProperties> deviceExtensionProperties(deviceExtensionPropertyCount);
    ret = vkEnumerateDeviceExtensionProperties(physicalDevice, NULL, &deviceExtensionPropertyCount, deviceExtensionProperties.data());
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkEnumerateDeviceExtensionProperties failed %d\n", ret);
    }

    for (uint32_t i=0; i<deviceExtensionPropertyCount; i++)
    {
        const VkExtensionProperties exp = deviceExtensionProperties[i];
        fprintf(stderr, "%s = %u\n", exp.extensionName, exp.specVersion);
    }

    const float queuePriorities[1] = { 1.f };// 0.f ~ 1.f

    VkDeviceQueueCreateInfo deviceQueueCreateInfo;
    deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    deviceQueueCreateInfo.pNext = 0;
    deviceQueueCreateInfo.flags = 0;
    deviceQueueCreateInfo.queueFamilyIndex = queueFamilyIndex;
    deviceQueueCreateInfo.queueCount = 1;
    deviceQueueCreateInfo.pQueuePriorities = queuePriorities;

    VkDeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = 0;
    deviceCreateInfo.flags = 0;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = 0;
    deviceCreateInfo.enabledExtensionCount = 0;
    deviceCreateInfo.ppEnabledExtensionNames = 0;
    deviceCreateInfo.pEnabledFeatures = 0;// VkPhysicalDeviceFeatures pointer

//     VkDevice device;
    ret = vkCreateDevice(physicalDevice, &deviceCreateInfo, 0, &device);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkCreateDevice failed %d\n", ret);
    }

    return 0;
}

void destroy_gpu_device()
{
    vkDestroyDevice(device, 0);

    vkDestroyInstance(instance, 0);
}

VkImage create_image(VkImageType imageType, int w, int h, int c)
{
    uint32_t queueFamilyIndex = get_gpu_queueFamilyIndex();

    // create image
    VkImageCreateInfo imageCreateInfo;
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext = 0;
    imageCreateInfo.flags = 0;
    imageCreateInfo.imageType = imageType;
    imageCreateInfo.format = VK_FORMAT_R32_SFLOAT;
    imageCreateInfo.extent.width = w;
    imageCreateInfo.extent.height = h;
    imageCreateInfo.extent.depth = c;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
//     imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
    imageCreateInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT;
//     imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.queueFamilyIndexCount = 1;
    imageCreateInfo.pQueueFamilyIndices = &queueFamilyIndex;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage image = 0;
    VkResult ret = vkCreateImage(get_gpu_device(), &imageCreateInfo, 0, &image);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkCreateImage failed %d\n", ret);
    }

    return image;
}

VkImageView create_imageview(VkImageViewType viewType, VkImage image)
{
    // create imageview
    VkComponentMapping componentMapping;
    componentMapping.r = VK_COMPONENT_SWIZZLE_R;
    componentMapping.g = VK_COMPONENT_SWIZZLE_G;
    componentMapping.b = VK_COMPONENT_SWIZZLE_B;
    componentMapping.a = VK_COMPONENT_SWIZZLE_A;

    VkImageSubresourceRange subresourceRange;
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    VkImageViewCreateInfo imageViewCreateInfo;
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.pNext = 0;
    imageViewCreateInfo.flags = 0;
    imageViewCreateInfo.image = image;
    imageViewCreateInfo.viewType = viewType;
    imageViewCreateInfo.format = VK_FORMAT_R32_SFLOAT;
    imageViewCreateInfo.components = componentMapping;
    imageViewCreateInfo.subresourceRange = subresourceRange;

    VkImageView imageView = 0;
    VkResult ret = vkCreateImageView(get_gpu_device(), &imageViewCreateInfo, 0, &imageView);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkCreateImageView failed %d\n", ret);
    }

    return imageView;
}

VkDeviceMemory fastMalloc(size_t size, uint32_t memoryTypeIndex)
{
    fprintf(stderr, "fastMalloc %lu on %u\n", size, memoryTypeIndex);
    // new
    VkMemoryAllocateInfo memoryAllocateInfo;
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext = 0;
    memoryAllocateInfo.allocationSize = size;
    memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;

    VkDeviceMemory ptr = 0;
    VkResult ret = vkAllocateMemory(get_gpu_device(), &memoryAllocateInfo, 0, &ptr);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkAllocateMemory failed %d\n", ret);
    }

    return ptr;
}

int main()
{
    init_gpu_device();

    int w = 8;
    int h = 8;

    VkResult ret;

    VkDevice device = get_gpu_device();
    uint32_t queueFamilyIndex = get_gpu_queueFamilyIndex();
//     uint32_t memoryTypeIndex = get_gpu_memoryTypeIndex();

    int group_x;
    int group_y;
    int group_z;

    VkShaderModule shaderModule;

    VkDescriptorPool descriptorPool;

    // layer-specific
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;

    VkPipeline pipeline;
    VkDescriptorSet descriptorSet;

//     ncnn::VkMat top_blob(8, 8, 4u, g_vulkan_devicelocal_allocator);

    VkImage image = create_image(VK_IMAGE_TYPE_2D, w, h, 1);

    VkImageView imageview = create_imageview(VK_IMAGE_VIEW_TYPE_2D, image);

    // get image memory layout
    VkSubresourceLayout subresourceLayout;
    VkImageSubresource subresource;
    subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource.mipLevel = 0;
    subresource.arrayLayer = 0;
    vkGetImageSubresourceLayout(get_gpu_device(), image, &subresource, &subresourceLayout);

    fprintf(stderr, "offset = %lu\n", subresourceLayout.offset);
    fprintf(stderr, "size = %lu\n", subresourceLayout.size);
    fprintf(stderr, "rowPitch = %lu\n", subresourceLayout.rowPitch);
    fprintf(stderr, "arrayPitch = %lu\n", subresourceLayout.arrayPitch);
    fprintf(stderr, "depthPitch = %lu\n", subresourceLayout.depthPitch);

    // alloc
    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(get_gpu_device(), image, &memoryRequirements);

    // memoryRequirements.size
    // memoryRequirements.alignment
    // memoryRequirements.memoryTypeBits

    VkDeviceMemory memory = fastMalloc(memoryRequirements.size, memoryTypeIndex_hostvisible);

    ret = vkBindImageMemory(get_gpu_device(), image, memory, 0);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkBindImageMemory failed %d\n", ret);
    }


    std::string imagetest_comp_spv = read_file("imagetest.comp.spv");

    // shader module
    VkShaderModuleCreateInfo shaderModuleCreateInfo;
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.pNext = 0;
    shaderModuleCreateInfo.flags = 0;
    shaderModuleCreateInfo.codeSize = imagetest_comp_spv.size();
    shaderModuleCreateInfo.pCode = (const uint32_t*)imagetest_comp_spv.data();

    ret = vkCreateShaderModule(device, &shaderModuleCreateInfo, 0, &shaderModule);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkCreateShaderModule failed %d\n", ret);
    }

    // descriptorset layout
    VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[1] =
    {
        {
            0,//binding
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,//descriptorType
            1,//descriptorCount
            VK_SHADER_STAGE_COMPUTE_BIT,//stageFlags
            0//pImmutableSamplers
        }
    };

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.pNext = 0;
    descriptorSetLayoutCreateInfo.flags = 0;
    descriptorSetLayoutCreateInfo.bindingCount = 1;
    descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings;

    ret = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, 0, &descriptorSetLayout);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkCreateDescriptorSetLayout failed %d\n", ret);
    }

    // pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = 0;
    pipelineLayoutCreateInfo.flags = 0;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = 0;

    ret = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, 0, &pipelineLayout);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkCreatePipelineLayout failed %d\n", ret);
    }

    // pipeline
    VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo;
    pipelineShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineShaderStageCreateInfo.pNext = 0;
    pipelineShaderStageCreateInfo.flags = 0;
    pipelineShaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineShaderStageCreateInfo.module = shaderModule;
    pipelineShaderStageCreateInfo.pName = "main";
    pipelineShaderStageCreateInfo.pSpecializationInfo = 0;

    VkComputePipelineCreateInfo computePipelineCreateInfo;
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = 0;
    computePipelineCreateInfo.flags = 0;
    computePipelineCreateInfo.stage = pipelineShaderStageCreateInfo;
    computePipelineCreateInfo.layout = pipelineLayout;
    computePipelineCreateInfo.basePipelineHandle = 0;
    computePipelineCreateInfo.basePipelineIndex = 0;

    ret = vkCreateComputePipelines(device, 0, 1, &computePipelineCreateInfo, 0, &pipeline);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkCreateComputePipelines failed %d\n", ret);
    }

    // descriptor pool
    VkDescriptorPoolSize poolSizes[1] =
    {
        {
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            1 // descriptorCount
        }
    };

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.pNext = 0;
    descriptorPoolCreateInfo.flags = 0;
    descriptorPoolCreateInfo.maxSets = 1;
    descriptorPoolCreateInfo.poolSizeCount = 1;
    descriptorPoolCreateInfo.pPoolSizes = poolSizes;

    ret = vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, 0, &descriptorPool);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkCreateDescriptorPool failed %d\n", ret);
    }

    // descriptorset
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.pNext = 0;
    descriptorSetAllocateInfo.descriptorPool = descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;

    ret = vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkAllocateDescriptorSets failed %d\n", ret);
    }

    // update descriptor set FIXME TODO
    VkDescriptorImageInfo descriptorImageInfos[1];

    descriptorImageInfos[0].sampler = 0;
    descriptorImageInfos[0].imageView = imageview;
    descriptorImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet writeDescriptorSets[1];

    writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSets[0].pNext = 0;
    writeDescriptorSets[0].dstSet = descriptorSet;
    writeDescriptorSets[0].dstBinding = 0;
    writeDescriptorSets[0].dstArrayElement = 0;
    writeDescriptorSets[0].descriptorCount = 1;
    writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writeDescriptorSets[0].pImageInfo = &descriptorImageInfos[0];
    writeDescriptorSets[0].pBufferInfo = 0;
    writeDescriptorSets[0].pTexelBufferView = 0;

    vkUpdateDescriptorSets(device, 1, writeDescriptorSets, 0, 0);

    group_x = w;
    group_y = h;
    group_z = 1;

    // commandpool and commandbuffer
    VkCommandPoolCreateInfo commandPoolCreateInfo;
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.pNext = 0;
    commandPoolCreateInfo.flags = 0;
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;

    VkCommandPool commandPool;
    ret = vkCreateCommandPool(device, &commandPoolCreateInfo, 0, &commandPool);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkCreateCommandPool failed %d\n", ret);
    }

    VkCommandBufferAllocateInfo commandBufferAllocateInfo;
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.pNext = 0;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    ret = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkAllocateCommandBuffers failed %d\n", ret);
    }

    // command buffer record
    VkCommandBufferBeginInfo commandBufferBeginInfo;
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.pNext = 0;
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    commandBufferBeginInfo.pInheritanceInfo = 0;

    ret = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkBeginCommandBuffer failed %d\n", ret);
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

    {
        // image layout
        VkImageMemoryBarrier imageBarrier;
        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageBarrier.pNext = 0;
        imageBarrier.srcAccessMask = 0;
        imageBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageBarrier.srcQueueFamilyIndex = queueFamilyIndex;
        imageBarrier.dstQueueFamilyIndex = queueFamilyIndex;
        imageBarrier.image = image;
        imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBarrier.subresourceRange.baseMipLevel = 0;
        imageBarrier.subresourceRange.levelCount = 1;
        imageBarrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // srcStageMask
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // dstStageMask
            0,
            0, 0, 0, 0,
            1, &imageBarrier);
    }

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, 0);

    vkCmdDispatch(commandBuffer, group_x, group_y, group_z);

    {
        // image layout
        VkImageMemoryBarrier imageBarrier;
        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageBarrier.pNext = 0;
        imageBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageBarrier.srcQueueFamilyIndex = queueFamilyIndex;
        imageBarrier.dstQueueFamilyIndex = queueFamilyIndex;
        imageBarrier.image = image;
        imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBarrier.subresourceRange.baseMipLevel = 0;
        imageBarrier.subresourceRange.levelCount = 1;
        imageBarrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // srcStageMask
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // dstStageMask
            0,
            0, 0, 0, 0,
            1, &imageBarrier);
    }

    ret = vkEndCommandBuffer(commandBuffer);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkEndCommandBuffer failed %d\n", ret);
    }

    fprintf(stderr, "vulkan record done\n");

    // queue
    VkQueue queue;
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

    // queue submit
    VkSubmitInfo submitInfo;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = 0;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = 0;
    submitInfo.pWaitDstStageMask = 0;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = 0;

    ret = vkQueueSubmit(queue, 1, &submitInfo, 0);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkQueueSubmit failed %d\n", ret);
    }

    // queue wait
    ret = vkQueueWaitIdle(queue);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkQueueWaitIdle failed %d\n", ret);
    }


    // get result
    {
    void* mapped_ptr = 0;
    VkResult ret = vkMapMemory(get_gpu_device(), memory, 0, VK_WHOLE_SIZE, 0, &mapped_ptr);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkMapMemory failed %d\n", ret);
    }

//     float* ptr = (float*)mapped_ptr;
//     unsigned char* ptr = (unsigned char*)mapped_ptr;
    for (int i=0; i<h; i++)
    {
        float* ptr = (float*)((unsigned char*)mapped_ptr + subresourceLayout.rowPitch * i);
        for (int j=0; j<w; j++)
        {
            fprintf(stderr, "%f\n", ptr[j]);
//             fprintf(stderr, "%d\n", ptr[j]);
        }
    }

    // TODO hold mapped ptr
    vkUnmapMemory(get_gpu_device(), memory);
    }


    vkDestroyDescriptorPool(device, descriptorPool, 0);

    vkDestroyPipeline(device, pipeline, 0);

    vkDestroyPipelineLayout(device, pipelineLayout, 0);

    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, 0);

    vkDestroyShaderModule(device, shaderModule, 0);


    destroy_gpu_device();

    return 0;
}

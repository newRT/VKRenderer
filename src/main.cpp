// common lib
#include <assert.h>
#include <stdio.h>

// GLFW for window create
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

// vulkan header file (c header is more fast than c++ header file)
#include <vulkan/vulkan.h>

//  macrocs
#define VK_CHECK(call) \
	do {\
		VkResult result_ = call;\
		assert(result_ == VK_SUCCESS);\
	}while(0)

// Array count
#define arrayCount(_array) (sizeof(_array)/sizeof(_array[0]))

// create vulkan instance
VkInstance createVKInstance()
{
	// application info
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.apiVersion = VK_VERSION_1_1;

	// vkInstance create info
	VkInstanceCreateInfo vkInstanceCreateInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	vkInstanceCreateInfo.pApplicationInfo = &appInfo;
#if _DEBUG
	// Layers
	const char* debugLayers[] =
	{
		"VK_LAYER_LUNARG_standard_validation"
	};
	vkInstanceCreateInfo.enabledLayerCount = arrayCount(debugLayers);
	vkInstanceCreateInfo.ppEnabledLayerNames = debugLayers;
#endif

	// extensions
	const  char* extensions[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef VK_USE_PLATFORM_WIN32_KHR
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif
	};
	vkInstanceCreateInfo.enabledExtensionCount = arrayCount(extensions);
	vkInstanceCreateInfo.ppEnabledExtensionNames = extensions;

	//  create vk instance
	VkInstance instance = 0;
	VK_CHECK(vkCreateInstance(&vkInstanceCreateInfo, nullptr, &instance));

	return instance;
}

// selected the discrete GPU otherwise choose the first one
VkPhysicalDevice selectPhysicalDevice(VkPhysicalDevice * physicalDevices, uint32_t physicalDeviceCount)
{
	for (uint32_t i = 0; i < physicalDeviceCount; i++)
	{
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(physicalDevices[i], &props);
		if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			printf("Selected physical device: %s\n", props.deviceName);
			return physicalDevices[i];
		}
	}

	if (physicalDeviceCount > 0)
	{
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(physicalDevices[0], &props);

		printf("Selected Integrated GPU: %s\n", props.deviceName);
		return physicalDevices[0];
	}

	printf("No physical devices available!\n");
	return VK_NULL_HANDLE;
}

// create device
VkDevice createDevice(VkInstance instance, VkPhysicalDevice physicalDevice)
{
	// device queue
	float queuePriorities[] = { 1.0f };

	VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	queueInfo.queueCount = 1;
	queueInfo.queueFamilyIndex = 0;
	queueInfo.pQueuePriorities = queuePriorities;

	VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	deviceCreateInfo.pQueueCreateInfos = &queueInfo;
	deviceCreateInfo.queueCreateInfoCount = 1;

	// device
	VkDevice device = 0;
	VK_CHECK(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device));

	return device;
}

// Create surface 
VkSurfaceKHR createSurface(VkInstance instance, GLFWwindow* window)
{
#ifdef VK_USE_PLATFORM_WIN32_KHR
	VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
	surfaceCreateInfo.hinstance = GetModuleHandle(0);
	surfaceCreateInfo.hwnd = glfwGetWin32Window(window);

	VkSurfaceKHR surface = 0;
	VK_CHECK(vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surface));
	return surface;
#else
#error Unsupported platform
#endif

}
int main()
{
	int ret = glfwInit();
	assert(ret);

	VkInstance instance = createVKInstance();
	assert(instance);

	// vulkan device
	VkPhysicalDevice physicalDevices[8];
	uint32_t physicalDeviceCount = arrayCount(physicalDevices);
	VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices));

	// physicall device
	VkPhysicalDevice physicalDevice = selectPhysicalDevice(physicalDevices, physicalDeviceCount);
	assert(physicalDevice);

	VkDevice device = createDevice(instance, physicalDevice);
	assert(device);

	GLFWwindow* window = glfwCreateWindow(1024, 768, "My Renderer", 0, 0);
	assert(window);

	VkSurfaceKHR surface = createSurface(instance, window);
	assert(surface);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	glfwDestroyWindow(window);

	vkDestroyInstance(instance, nullptr);

	return 0;
}
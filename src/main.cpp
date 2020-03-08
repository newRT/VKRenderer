#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <functional>
#include <cstdlib>
#include <map>
#include <set>
#include <cstdint>
#include <algorithm>
#include <fstream>
#include <array>
#include <assert.h>
#include <optional>
#include <vector>

// global const
const int		WIDTH			= 800;
const int		HEIGHT			= 600;
const int		MAX_FRAMES		= 2;

// for validation layer
const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };

// for device layer
const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#ifdef _DEBUG
const bool enableValidationLayer = true;
#else
const bool enableValidationLayer = false;
#endif // #ifdef NDEBUG


VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
	VkDebugUtilsMessengerEXT debugMessenger,
	const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

static std::vector<char> readFile(const std::string& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open file!");
	}

	// file size
	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}

class VKRenderer
{
	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		bool isComplete()
		{
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};

	struct SwapchainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};
public:
	void Run()
	{
		_initWindow();
		_initVulkan();
		_mainLoop();
		_cleanup();
	}

private:
	GLFWwindow* _window;
	int			windowWidth;
	int			windowHeight;

	// vulkan
	VkInstance							_instance;

	// debug messenger
	VkDebugUtilsMessengerEXT			debugMessenger;

	// physical device-->gpu graphics card
	VkPhysicalDevice					_physicalDevice = VK_NULL_HANDLE;

	// logic device
	VkDevice							_device;

	// queue handle
	VkQueue								_graphicsQueue;
	VkQueue								_presentQueue;

	// surface
	VkSurfaceKHR						_surface;

	// swapchain
	VkSwapchainKHR						_swapChain;
	std::vector<VkImage>				_swapChainImages;
	VkFormat							_swapChainImageFormat;
	VkExtent2D							_swapChainExtent;

	// image view
	std::vector<VkImageView>			_swapChainImageViews;

	// pipeline layout
	VkPipelineLayout					_pipelineLayout;

	// render pass
	VkRenderPass						_renderPass;

	// graphics pipeline
	VkPipeline							_graphicsPipeline;

	// frame buffers
	std::vector<VkFramebuffer>			_swapChainFrameBuffers;

	// command pool
	VkCommandPool						_commandPool;

	// command buffers
	std::vector<VkCommandBuffer>		_commandBuffers;

	// semaphores
	std::vector<VkSemaphore>			_imageAvailableSemaphores;
	std::vector<VkSemaphore>			_renderFinishedSemaphores;
	std::vector<VkFence>				_inFlightFences;
	std::vector<VkFence>                _imagesInFlight;

	// current frame
	size_t								_currentFrame = 0;

	// resized
	bool								_framebufferResized = false;

	// shaders
	VkShaderModule						_shaderModuleVS;
	VkShaderModule						_shaderModulePS;


private:

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserdata)
	{
		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
	}
	void _initWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);	// no openGL api
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);		// no resize

		{
			windowWidth = WIDTH;
			windowHeight = HEIGHT;
		}
		_window = glfwCreateWindow(windowWidth, windowHeight, "Vulkan Renderer", nullptr, nullptr);
		glfwSetWindowUserPointer(_window, this);
		glfwSetFramebufferSizeCallback(_window, framebufferResizeCallback);
	}

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
	{
		auto app = reinterpret_cast<VKRenderer*>(glfwGetWindowUserPointer(window));
		app->_framebufferResized = true;
	}

	bool _checkValidationLayerSupport()
	{
		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availibleLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availibleLayers.data());

		bool layerFound = false;
		for (const char* layerName : validationLayers)
		{
			for (const auto& layerProperties : availibleLayers)
			{
				if (strcmp(layerName, layerProperties.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}

		}
		if (!layerFound)
			return false;

		return true;
	}

	std::vector<const char*> getRequiredExtensions()
	{
		uint32_t gfwExtensionCount = 0;
		const char** glfwExtensions;

		glfwExtensions = glfwGetRequiredInstanceExtensions(&gfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + gfwExtensionCount);
		if (enableValidationLayer)
		{
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}

	void _populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
	{
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
	}

	void _createInstance()
	{
		if (enableValidationLayer && !_checkValidationLayerSupport())
		{
			throw std::runtime_error("validation layers requested, but not available");
		}
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		auto extensions = getRequiredExtensions();

		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
		if (enableValidationLayer)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			_populateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}


		if (vkCreateInstance(&createInfo, nullptr, &_instance) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create instance!");
		}
	}

	void _createSurface()
	{
		if (glfwCreateWindowSurface(_instance, _window, nullptr, &_surface) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create window surface.");
		}
	}

	void _setupMessenger()
	{
		if (!enableValidationLayer) return;

		VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
		_populateDebugMessengerCreateInfo(createInfo);

		if (CreateDebugUtilsMessengerEXT(_instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to set up debug messenger");
		}
	}

	bool _checkDeviceExtensionSupport(VkPhysicalDevice device)
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

		for (const auto& extension : availableExtensions)
		{
			requiredExtensions.erase(extension.extensionName); // do we have swapchain support?
		}

		return requiredExtensions.empty();
	}

	bool _isDeviceSuitable(VkPhysicalDevice device)
	{
		QueueFamilyIndices indices = _findQueueFamily(device);

		// check device for swapchain support
		bool extensionsSupported = _checkDeviceExtensionSupport(device);

		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			SwapchainSupportDetails swapChainSupport = _querySwapchainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		return indices.isComplete() && extensionsSupported && swapChainAdequate;
	}

	int _rateDeviceSuitability(VkPhysicalDevice device)
	{
		VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
		int score = 0;

		// discrete GPUs have a significant performance advantage
		if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			score += 1000;
		}

		// maximum possible size of textures affects graphics quality
		score += deviceProperties.limits.maxImageDimension2D;

		// application can't funcion without geometry shaders
		if (!deviceFeatures.geometryShader)
		{
			return 0;
		}

		return score;
	}

	void _pickPhysicalDevice() // graphics card choose(GPU)
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr); // get the number of GPUs first

		// do we have any GPUs with Vulkan support?
		if (deviceCount == 0)
		{
			throw std::runtime_error("Failed to find GPUs with Vulkan support!");
		}

		// fill with all the GPUs
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data());

		for (const auto& device : devices)
		{
			if (_isDeviceSuitable(device))
			{
				_physicalDevice = device;
				break;
			}
		}

		// if no suitable GPU
		if (_physicalDevice == VK_NULL_HANDLE)
		{
			throw std::runtime_error("Failed to find a suitable GPU!");
		}

		// use an ordered map to automatically sort candidates by increasing score
		std::multimap<int, VkPhysicalDevice> candidates;

		for (const auto& device : devices)
		{
			int score = _rateDeviceSuitability(device);
			candidates.insert(std::make_pair(score, device));
		}

		// check if the best candidate is suitable alt all
		if (candidates.rbegin()->first > 0)
		{
			_physicalDevice = candidates.rbegin()->second;
		}
		else
		{
			throw std::runtime_error("Failed to find a suitable GPU");
		}
	}

	QueueFamilyIndices _findQueueFamily(VkPhysicalDevice device)
	{
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int i = 0;
		VkBool32 presentSupport = false;
		for (const auto& queueFamily : queueFamilies)
		{
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, _surface, &presentSupport);
			if (presentSupport)
			{
				indices.presentFamily = i;
			}
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.graphicsFamily = i;
			}
			if (indices.isComplete())
				break;
			i++;
		}
		return indices;
	}

	void _createLogicDevice()
	{
		QueueFamilyIndices indices = _findQueueFamily(_physicalDevice);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamiles = { indices.graphicsFamily.value(), indices.presentFamily.value() };
		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamiles)
		{
			VkDeviceQueueCreateInfo queueCreateInfo = {};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;

			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures = {};

		VkDeviceCreateInfo deviceCreateInfo = {};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();

		deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

		// enable swapchain
		deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

		if (enableValidationLayer)
		{
			deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			deviceCreateInfo.enabledLayerCount = 0;
		}

		if (vkCreateDevice(_physicalDevice, &deviceCreateInfo, nullptr, &_device) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create logical device");
		}

		// retrieving queue handle
		vkGetDeviceQueue(_device, indices.graphicsFamily.value(), 0, &_graphicsQueue);
		vkGetDeviceQueue(_device, indices.presentFamily.value(), 0, &_presentQueue);
	}

	SwapchainSupportDetails _querySwapchainSupport(VkPhysicalDevice device)
	{
		SwapchainSupportDetails details;

		// surface
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, _surface, &details.capabilities);

		// support format
		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &formatCount, nullptr);
		if (formatCount != 0)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &formatCount, details.formats.data());
		}

		// query present
		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &presentModeCount, nullptr);
		if (presentModeCount != 0)
		{
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	VkSurfaceFormatKHR _chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		for (const VkSurfaceFormatKHR& availableFormat : availableFormats)
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return availableFormat;
		}

		return availableFormats[0];
	}

	VkPresentModeKHR _chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
	{
		for (const auto& presentMode : availablePresentModes)
		{
			if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return presentMode;
			}
		}
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D _chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
	{
		if (capabilities.currentExtent.width != UINT32_MAX)
		{
			return capabilities.currentExtent;
		}
		else
		{
			int width, height;
			glfwGetFramebufferSize(_window, &width, &height);

			VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

			actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
			actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

			return actualExtent;
		}
	}

	void _createSwapchain()
	{
		SwapchainSupportDetails swapChainSupport = _querySwapchainSupport(_physicalDevice);

		VkSurfaceFormatKHR surfaceFormat = _chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = _chooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = _chooseSwapExtent(swapChainSupport.capabilities);

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = _surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		QueueFamilyIndices indices = _findQueueFamily(_physicalDevice);
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		if (indices.graphicsFamily != indices.presentFamily)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
		}

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;

		createInfo.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(_device, &createInfo, nullptr, &_swapChain) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create swap chain");
		}

		vkGetSwapchainImagesKHR(_device, _swapChain, &imageCount, nullptr);
		_swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(_device, _swapChain, &imageCount, _swapChainImages.data());
		_swapChainImageFormat = surfaceFormat.format;
		_swapChainExtent = extent;
	}

	void _createImageViews()
	{
		_swapChainImageViews.resize(_swapChainImages.size());

		for (size_t i = 0; i < _swapChainImages.size(); i++)
		{
			VkImageViewCreateInfo imgViewCreateInfo = {};
			imgViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imgViewCreateInfo.image = _swapChainImages[i];
			imgViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imgViewCreateInfo.format = _swapChainImageFormat;

			imgViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			imgViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			imgViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			imgViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			imgViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imgViewCreateInfo.subresourceRange.baseMipLevel = 0;
			imgViewCreateInfo.subresourceRange.levelCount = 1;
			imgViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			imgViewCreateInfo.subresourceRange.layerCount = 1;

			if (vkCreateImageView(_device, &imgViewCreateInfo, nullptr, &_swapChainImageViews[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create image views");
			}
		}
	}

	VkShaderModule  _createShaderModule(const char* path)
	{
		// load file
		FILE* file = fopen(path, "rb");
		assert(file);

		fseek(file, 0, SEEK_END);
		long length = ftell(file);
		assert(length >= 0);
		fseek(file, 0, SEEK_SET);

		char* buffer = new char[length];
		assert(buffer);

		size_t rc = fread(buffer, 1, length, file);
		assert(rc == size_t(length));
		fclose(file);

		VkShaderModuleCreateInfo createInfo = {};

		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = length;
		createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer);

		VkShaderModule shaderModule = 0;
		if (vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create shader module");
		}

		return shaderModule;
	}
	
	void _createRenderPass()
	{
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = _swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// SUBPASS
		// attachment references
		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;

		// subpass dependencies
		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;

		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;

		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(_device, &renderPassInfo, nullptr, &_renderPass) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create render pass!");
		}
	}

	void _createPipelineLayout()
	{
		VkPipelineLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

		vkCreatePipelineLayout(_device, &createInfo, nullptr, &_pipelineLayout);
	}

	void _createGraphicsPipeline()
	{
		VkGraphicsPipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

		VkPipelineShaderStageCreateInfo shaderStages[2] = {  };
		shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		shaderStages[0].module = _shaderModuleVS;
		shaderStages[0].pName = "main";

		shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shaderStages[1].module = _shaderModulePS;
		shaderStages[1].pName = "main";

		createInfo.stageCount = sizeof(shaderStages) / sizeof(shaderStages[0]);
		createInfo.pStages = shaderStages;

		VkPipelineVertexInputStateCreateInfo vertexInput = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
		createInfo.pVertexInputState = &vertexInput;

		VkPipelineInputAssemblyStateCreateInfo assemblyState = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
		assemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		createInfo.pInputAssemblyState = &assemblyState;

		VkPipelineTessellationStateCreateInfo tessellationState = { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };
		createInfo.pTessellationState = &tessellationState;

		VkPipelineViewportStateCreateInfo viewport = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
		viewport.viewportCount = 1;
		viewport.scissorCount = 1;
		createInfo.pViewportState = &viewport;

		VkPipelineRasterizationStateCreateInfo rasterizationState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
		rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationState.lineWidth = 1.0f;
		createInfo.pRasterizationState = &rasterizationState;

		VkPipelineMultisampleStateCreateInfo multiSampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
		multiSampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		createInfo.pMultisampleState = &multiSampleState;

		VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
		createInfo.pDepthStencilState = &depthStencilState;

		VkPipelineColorBlendAttachmentState colorAttachmentState = {};
		colorAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
		colorBlendState.attachmentCount = 1;
		colorBlendState.pAttachments = &colorAttachmentState;
		createInfo.pColorBlendState = &colorBlendState;

		VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

		VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
		dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
		dynamicState.pDynamicStates = dynamicStates;
		createInfo.pDynamicState = &dynamicState;

		createInfo.layout = _pipelineLayout;
		createInfo.renderPass = _renderPass;

		vkCreateGraphicsPipelines(_device, nullptr, 1, &createInfo, nullptr, &_graphicsPipeline);
		assert(_graphicsPipeline);
	}

	void _createFrameBuffers()
	{
		_swapChainFrameBuffers.resize(_swapChainImageViews.size());

		for (size_t i = 0; i < _swapChainImageViews.size(); ++i)
		{
			VkImageView attachments[] = { _swapChainImageViews[i] };

			VkFramebufferCreateInfo frameBufferInfo = {};
			frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			frameBufferInfo.renderPass = _renderPass;
			frameBufferInfo.attachmentCount = 1;
			frameBufferInfo.pAttachments = attachments;
			frameBufferInfo.width = _swapChainExtent.width;
			frameBufferInfo.height = _swapChainExtent.height;
			frameBufferInfo.layers = 1;

			if (vkCreateFramebuffer(_device, &frameBufferInfo, nullptr, &_swapChainFrameBuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create framebuffer");
			}
		}
	}

	void _createCommandPool()
	{
		QueueFamilyIndices queueFamilyIndice = _findQueueFamily(_physicalDevice);

		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueFamilyIndice.graphicsFamily.value();
		poolInfo.flags = 0;

		if (vkCreateCommandPool(_device, &poolInfo, nullptr, &_commandPool) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create Command pool");
		}

		vkResetCommandPool(_device, _commandPool, 0);
	}

	void _createCommandBuffers()
	{
		_commandBuffers.resize(_swapChainFrameBuffers.size());

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = _commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t)_commandBuffers.size();

		if (vkAllocateCommandBuffers(_device, &allocInfo, _commandBuffers.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create command buffers!");
		}
	}

	void _createSyncObjects()
	{
		_imageAvailableSemaphores.resize(MAX_FRAMES);
		_renderFinishedSemaphores.resize(MAX_FRAMES);
		_inFlightFences.resize(MAX_FRAMES);
		_imagesInFlight.resize(_swapChainImages.size(), VK_NULL_HANDLE);

		VkSemaphoreCreateInfo semaphoreCreateInfo = {};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < MAX_FRAMES; ++i)
		{
			if (vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_imageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_renderFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(_device, &fenceInfo, nullptr, &_inFlightFences[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create synchronization objects for a frame!");
			}
		}
	}

	void _recreateSwapChain()
	{
		int width = 0, height = 0;
		glfwGetFramebufferSize(_window, &width, &height);
		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(_window, &windowWidth, &windowWidth);
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(_device);

		_cleanupSwapChain();

		_createSwapchain();
		_createImageViews();
		_createRenderPass();
		_createPipelineLayout();
		_createGraphicsPipeline();
		_createFrameBuffers();
		_createCommandBuffers();
	}
	
	uint32_t _findeMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
			if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}

		throw std::runtime_error("Failed to find suitable memory type!");
	}

	void _createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
	{
		VkBufferCreateInfo vertexBufferInfo = {};
		vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferInfo.size = size;

		vertexBufferInfo.usage = usage;
		vertexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(_device, &vertexBufferInfo, nullptr, &buffer) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create vertex buffer!");
		}

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(_device, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = _findeMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(_device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate vertex buffer memory!");
		}

		vkBindBufferMemory(_device, buffer, bufferMemory, 0);
	}

	void _copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
	{
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = _commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(_device, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		VkBufferCopy copyRegion = {};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		vkEndCommandBuffer(commandBuffer);

		// submit
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(_graphicsQueue);

		// clean up
		vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);
	}

	VkImageMemoryBarrier _imageBarrier(VkImage image, VkAccessFlags srcAccessMask, VkImageLayout oldLayout, VkAccessFlags dscAcessMask, VkImageLayout newLayout)
	{
		VkImageMemoryBarrier imageBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		imageBarrier.srcAccessMask				 = srcAccessMask;
		imageBarrier.dstAccessMask				 = dscAcessMask;
		imageBarrier.oldLayout                   = oldLayout;
		imageBarrier.newLayout                   = newLayout;
		imageBarrier.srcQueueFamilyIndex		 = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier.dstQueueFamilyIndex		 = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier.image						 = image;
		imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		imageBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;

		return imageBarrier;
	}

	void pipelineImageBarrier(VkCommandBuffer commandbuffer, VkImage image, 
		VkPipelineStageFlags srcStageMask, VkAccessFlags srcAccessMask, VkImageLayout oldLayout, 
		VkPipelineStageFlags dstStageMask, VkAccessFlags dscAcessMask, VkImageLayout newLayout)
	{

	}
	// init vulkan
	void _initVulkan()
	{
		_createInstance();
		_createSurface(); // The window surface needs to be created right after the instance creation
		_setupMessenger();
		_pickPhysicalDevice();
		_createLogicDevice();
		_createSwapchain();

		_shaderModuleVS = _createShaderModule("shaders/triangle.vert.spv");
		_shaderModulePS = _createShaderModule("shaders/triangle.frag.spv");

		_createImageViews();
		_createRenderPass();
		_createPipelineLayout();
		_createGraphicsPipeline();
		_createFrameBuffers();
		_createCommandPool();
		_createCommandBuffers();
		_createSyncObjects();
	}

	void _drawFrame()
	{
		vkWaitForFences(_device, 1, &_inFlightFences[_currentFrame], VK_TRUE, UINT64_MAX);

		// acquiring an image
		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(_device, _swapChain, UINT64_MAX, _imageAvailableSemaphores[_currentFrame], VK_NULL_HANDLE, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			_recreateSwapChain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("Failed to acquire swap chain image!");
		}

		if (_imagesInFlight[imageIndex] != VK_NULL_HANDLE)
		{
			vkWaitForFences(_device, 1, &_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
		}

		_imagesInFlight[imageIndex] = _inFlightFences[_currentFrame];

		VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		vkBeginCommandBuffer(_commandBuffers[imageIndex], &beginInfo);

		VkImageMemoryBarrier renderBeginBarrier = _imageBarrier(_swapChainImages[imageIndex], 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		vkCmdPipelineBarrier(_commandBuffers[imageIndex], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderBeginBarrier);

		// use renderpass to clear
		VkClearColorValue cleanColor = { 48.0 / 255.f, 10.0 / 255.0f, 36.0 / 255.0f, 1.0f };
		VkClearValue clearValue = { cleanColor };

		VkRenderPassBeginInfo renderPassBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		renderPassBeginInfo.renderPass = _renderPass;
		renderPassBeginInfo.framebuffer = _swapChainFrameBuffers[imageIndex];
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = &clearValue;
		renderPassBeginInfo.renderArea.extent.width = windowWidth;
		renderPassBeginInfo.renderArea.extent.height = windowHeight;

		vkCmdBeginRenderPass(_commandBuffers[imageIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// draw calls go here
		VkViewport viewport = { 0, float(windowHeight), float(windowWidth), -float(windowHeight), 0, 1 };
		VkRect2D scissor = { {0, 0}, {windowWidth, windowHeight} };

		vkCmdSetViewport(_commandBuffers[imageIndex], 0, 1, &viewport);
		vkCmdSetScissor(_commandBuffers[imageIndex], 0, 1, &scissor);

		vkCmdBindPipeline(_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);
		vkCmdDraw(_commandBuffers[imageIndex], 3, 1, 0, 0);

		vkCmdEndRenderPass(_commandBuffers[imageIndex]);


		VkImageMemoryBarrier renderEndBarrier = _imageBarrier(_swapChainImages[imageIndex], VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		vkCmdPipelineBarrier(_commandBuffers[imageIndex], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderEndBarrier);

		vkEndCommandBuffer(_commandBuffers[imageIndex]);
		// submitting the command buffer
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore watsSemaphores[] = { _imageAvailableSemaphores[_currentFrame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = watsSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &_commandBuffers[imageIndex];

		VkSemaphore signalSemaphores[] = { _renderFinishedSemaphores[_currentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		vkResetFences(_device, 1, &_inFlightFences[_currentFrame]);
		if (vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _inFlightFences[_currentFrame]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to submit draw command buffer");
		}

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { _swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;

		presentInfo.pResults = nullptr;

		result = vkQueuePresentKHR(_presentQueue, &presentInfo);
		if (result == VK_ERROR_OUT_OF_POOL_MEMORY_KHR || result == VK_SUBOPTIMAL_KHR || _framebufferResized)
		{
			glfwGetWindowSize(_window, &windowWidth, &windowHeight);
			_framebufferResized = false;
			_recreateSwapChain();
		}
		else if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to present swap chain image!");
		}

		_currentFrame = (_currentFrame + 1) % MAX_FRAMES;
	}

	void _mainLoop()
	{
		while (!glfwWindowShouldClose(_window))
		{
			glfwPollEvents();
			_drawFrame();
		}

		vkDeviceWaitIdle(_device);
	}

	void _cleanupSwapChain()
	{
		for (auto framebuffer : _swapChainFrameBuffers)
		{
			vkDestroyFramebuffer(_device, framebuffer, nullptr);
		}

		vkFreeCommandBuffers(_device, _commandPool, static_cast<uint32_t>(_commandBuffers.size()), _commandBuffers.data());

		vkDestroyPipeline(_device, _graphicsPipeline, nullptr);

		vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);

		vkDestroyRenderPass(_device, _renderPass, nullptr);

		for (auto imageView : _swapChainImageViews)
		{
			vkDestroyImageView(_device, imageView, nullptr);
		}

		vkDestroySwapchainKHR(_device, _swapChain, nullptr);
	}

	void _cleanup()
	{
		vkDestroyShaderModule(_device, _shaderModuleVS, nullptr);
		vkDestroyShaderModule(_device, _shaderModulePS, nullptr);

		_cleanupSwapChain();

		for (size_t i = 0; i < MAX_FRAMES; i++)
		{
			vkDestroySemaphore(_device, _imageAvailableSemaphores[i], nullptr);
			vkDestroySemaphore(_device, _renderFinishedSemaphores[i], nullptr);
			vkDestroyFence(_device, _inFlightFences[i], nullptr);
		}

		vkDestroyCommandPool(_device, _commandPool, nullptr);

		vkDestroyDevice(_device, nullptr);

		if (enableValidationLayer)
		{
			DestroyDebugUtilsMessengerEXT(_instance, debugMessenger, nullptr);
		}

		vkDestroySurfaceKHR(_instance, _surface, nullptr);

		vkDestroyInstance(_instance, nullptr);

		glfwDestroyWindow(_window);

		glfwTerminate();
	}
};

int main()
{
	VKRenderer app;

	try
	{
		app.Run();
	}
	catch (const std::exception & e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

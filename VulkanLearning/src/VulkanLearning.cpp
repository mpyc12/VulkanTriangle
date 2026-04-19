#if defined (__INTELLISENSE__) || !defined (USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <vector>
#include <assert.h>
#include <limits>
#include <fstream>
#include <filesystem>

constexpr uint32_t WIDTH = 1000;
constexpr uint32_t HEIGHT = 800;

const std::vector<char const*> validationLayers = {
		"VK_LAYER_KHRONOS_validation" };

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

class HelloTriangleApplication {
public:
	void run() {
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}
private:
	GLFWwindow *window;

	vk::raii::Context context;
	vk::raii::Instance instance = nullptr;

	vk::raii::PhysicalDevice physicalDevice = nullptr;

	std::vector<const char*> requiredDeviceExtension = {
			vk::KHRSwapchainExtensionName };

	vk::raii::Device device = nullptr;
	vk::raii::Queue graphicsQueue = nullptr;

	vk::raii::SurfaceKHR surface = nullptr;

	vk::raii::SwapchainKHR swapChain = nullptr;
	std::vector<vk::Image> swapChainImages;
	vk::SurfaceFormatKHR swapChainSurfaceFormat;
	vk::Extent2D swapChainExtent;
	std::vector<vk::raii::ImageView> swapChainImageViews;

	vk::raii::PipelineLayout pipelineLayout = nullptr;
	vk::raii::Pipeline graphicsPipeline = nullptr;

	uint32_t queueIndex = ~0;

	vk::raii::CommandPool commandPool = nullptr;
	vk::raii::CommandBuffer commandBuffer = nullptr;

	vk::raii::Semaphore presentCompleteSemaphore = nullptr;
	vk::raii::Semaphore renderFinishedSemaphore = nullptr;
	vk::raii::Fence drawFence = nullptr;

	void initWindow() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Hello Window", nullptr,
				nullptr);
	}

	void initVulkan() {

		createInstance();
		std::cout << "1. Instance created" << std::endl;
		setupDebugMessenger();
		std::cout << "2. Debug messenger ready" << std::endl;
		createSurface();
		std::cout << "3. Surface created" << std::endl;
		pickPhysicalDevice();
		std::cout << "4. Physical device picked" << std::endl;
		createLogicalDevice();
		std::cout << "5. Logical device created" << std::endl;
		createSwapChain();
		std::cout << "6. Swapchain created" << std::endl;
		createImageViews();
		std::cout << "7. Image views created" << std::endl;
		createGraphicsPipeline();
		std::cout << "8. Pipeline created" << std::endl;
		createCommandPool();
		std::cout << "9. Command pool created" << std::endl;
		createCommandBuffer();
		std::cout << "10. Command buffer created" << std::endl;
		createSyncObjects();
		std::cout << "--- VULKAN INIT SUCCESSFUL ---" << std::endl;
	}

	void createImageViews() {
		assert(swapChainImageViews.empty());

		vk::ImageViewCreateInfo imageViewCreateInfo { };
		imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
		imageViewCreateInfo.format = swapChainSurfaceFormat.format;
		imageViewCreateInfo.subresourceRange = {
				vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

		for (auto &image : swapChainImages) {
			imageViewCreateInfo.image = image;
			swapChainImageViews.emplace_back(device, imageViewCreateInfo);
		}
	}

	void createSwapChain() {
		vk::SurfaceCapabilitiesKHR surfaceCapabilities =
				physicalDevice.getSurfaceCapabilitiesKHR(*surface);
		swapChainExtent = chooseSwapExtent(surfaceCapabilities);
		uint32_t minImageCount = chooseSwapMinImageCount(surfaceCapabilities);

		std::vector<vk::SurfaceFormatKHR> availableFormats =
				physicalDevice.getSurfaceFormatsKHR(*surface);
		swapChainSurfaceFormat = chooseSwapSurfaceFormat(availableFormats);

		std::vector<vk::PresentModeKHR> availablePresentModes =
				physicalDevice.getSurfacePresentModesKHR(*surface);
		vk::PresentModeKHR presentMode = chooseSwapPresentMode(
				availablePresentModes);

		vk::SwapchainCreateInfoKHR swapChainCreateInfo { };
		swapChainCreateInfo.surface = *surface;
		swapChainCreateInfo.minImageCount = minImageCount;
		swapChainCreateInfo.imageFormat = swapChainSurfaceFormat.format;
		swapChainCreateInfo.imageColorSpace = swapChainSurfaceFormat.colorSpace;
		swapChainCreateInfo.imageExtent = swapChainExtent;
		swapChainCreateInfo.imageArrayLayers = 1;
		swapChainCreateInfo.imageUsage =
				vk::ImageUsageFlagBits::eColorAttachment;
		swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
		swapChainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
		swapChainCreateInfo.compositeAlpha =
				vk::CompositeAlphaFlagBitsKHR::eOpaque;
		swapChainCreateInfo.presentMode = presentMode;
		swapChainCreateInfo.clipped = true;

		swapChain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
		swapChainImages = swapChain.getImages();
	}

	static uint32_t chooseSwapMinImageCount(
			vk::SurfaceCapabilitiesKHR const &surfaceCapabilities) {
		auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);

		if ((0 < surfaceCapabilities.maxImageCount)
				&& (surfaceCapabilities.maxImageCount < minImageCount)) {
			minImageCount = surfaceCapabilities.maxImageCount;
		}

		return minImageCount;
	}

	static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(
			std::vector<vk::SurfaceFormatKHR> const &availableFormats) {
		assert(!availableFormats.empty());

		const auto formatIt = std::ranges::find_if(availableFormats,
				[](const auto &format) {
					return format.format == vk::Format::eB8G8R8A8Srgb
							&& format.colorSpace
									== vk::ColorSpaceKHR::eSrgbNonlinear;
				});
		return formatIt != availableFormats.end() ?
				*formatIt : availableFormats[0];
	}

	static vk::PresentModeKHR chooseSwapPresentMode(
			std::vector<vk::PresentModeKHR> const &availablePresentModes) {
		assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) {
			return presentMode == vk::PresentModeKHR::eFifo;
		})
		);

		return std::ranges::any_of(availablePresentModes,
				[](const vk::PresentModeKHR value) {
					return vk::PresentModeKHR::eMailbox == value;
				}) ?
		vk::PresentModeKHR::eMailbox :
					vk::PresentModeKHR::eFifo;
	}

	vk::Extent2D chooseSwapExtent(
			vk::SurfaceCapabilitiesKHR const &capabilities) {
		if (capabilities.currentExtent.width
				!= std::numeric_limits<uint32_t>::max()) {
			return capabilities.currentExtent;
		}

		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		return {
			std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
			std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
		};
	}

	void createSurface() {
		VkSurfaceKHR _surface;

		if (*instance == VK_NULL_HANDLE) {
			throw std::runtime_error(
					"Instance handle is NULL! Check createInstance log.");
		}

		if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface)
				!= VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface!");
		}

		surface = vk::raii::SurfaceKHR(instance, _surface);

		if (surface == nullptr)
			throw std::runtime_error("Surface is null");
	}

	void createLogicalDevice() {
		std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
				physicalDevice.getQueueFamilyProperties();

		vk::PhysicalDeviceVulkan11Features features11;
		features11.shaderDrawParameters = VK_TRUE;

		queueIndex = std::numeric_limits<uint32_t>::max();

		for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size();
				qfpIndex++) {
			if ((queueFamilyProperties[qfpIndex].queueFlags
					& vk::QueueFlagBits::eGraphics)
					&& physicalDevice.getSurfaceSupportKHR(qfpIndex,
							*surface)) {
				queueIndex = qfpIndex;
				break;
			}
		}

		if (queueIndex == std::numeric_limits<uint32_t>::max()) {
			throw std::runtime_error("Could not find a suitable queue family!");
		}

		auto graphicsQueueFamilyProperty = std::ranges::find_if(
				queueFamilyProperties,
				[](auto const &qfp) {
					return (qfp.queueFlags & vk::QueueFlagBits::eGraphics)
							!= static_cast<vk::QueueFlags>(0);
				});
		assert(
				graphicsQueueFamilyProperty != queueFamilyProperties.end()
						&& "No graphics queue family found!");

		auto graphicsIndex = static_cast<uint32_t>(std::distance(
				queueFamilyProperties.begin(), graphicsQueueFamilyProperty));

		vk::StructureChain<vk::PhysicalDeviceFeatures2,
				vk::PhysicalDeviceVulkan13Features,
				vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain {
				vk::PhysicalDeviceFeatures2 { },
				vk::PhysicalDeviceVulkan13Features { }.setDynamicRendering(true).setSynchronization2(
						true),
				vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT { }.setExtendedDynamicState(
						true) };

		float queuePriority = 0.5f;

		vk::DeviceQueueCreateInfo deviceQueueCreateInfo { };
		deviceQueueCreateInfo.queueFamilyIndex = graphicsIndex;
		deviceQueueCreateInfo.queueCount = 1;
		deviceQueueCreateInfo.pQueuePriorities = &queuePriority;

		vk::DeviceCreateInfo deviceCreateInfo { };
		deviceCreateInfo.pNext = &features11;
		deviceCreateInfo.queueCreateInfoCount = 1;
		deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
		deviceCreateInfo.enabledExtensionCount =
				static_cast<uint32_t>(requiredDeviceExtension.size());
		deviceCreateInfo.ppEnabledExtensionNames =
				requiredDeviceExtension.data();

		device = vk::raii::Device(physicalDevice, deviceCreateInfo);

		graphicsQueue = vk::raii::Queue(device, graphicsIndex, 0);

	}

	bool isDeviceSuitable(vk::raii::PhysicalDevice const &physicalDevice) {
		bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion
				>= vk::ApiVersion13;

		auto queueFamilies = physicalDevice.getQueueFamilyProperties();
		bool supportsGraphics = std::ranges::any_of(queueFamilies,
				[](auto const &qfp) {
					return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
				});

		auto availableDeviceExtensions =
				physicalDevice.enumerateDeviceExtensionProperties();
		bool supportsAllRequiredExtensions = std::ranges::all_of(
				requiredDeviceExtension,
				[&availableDeviceExtensions](
						auto const &requiredDeviceExtension) {
					return std::ranges::any_of(availableDeviceExtensions,
							[requiredDeviceExtension](
									auto const &availableDeviceExtension) {
								return strcmp(
										availableDeviceExtension.extensionName,
										requiredDeviceExtension) == 0;
							});
				});

		auto features = physicalDevice.template getFeatures2<
				vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features,
				vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

		bool supportsRequiredFeatures = features.template get<
				vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
		features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

		return supportsVulkan1_3 && supportsGraphics
				&& supportsAllRequiredExtensions && supportsRequiredFeatures;
	}

	void pickPhysicalDevice() {
		std::vector<vk::raii::PhysicalDevice> physicalDevices =
				instance.enumeratePhysicalDevices();

		auto const devIter = std::ranges::find_if(physicalDevices,
				[&](auto const &pd) {
					return isDeviceSuitable(pd);
				});

		if (devIter == physicalDevices.end()) {
			throw std::runtime_error("failed to find a suitable GPU!");
		}

		physicalDevice = *devIter;
	}

	void setupDebugMessenger() {
		if (!enableValidationLayers)
			return;

		vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
						| vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
		vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
				vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
						| vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
						| vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

		vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT { };
		debugUtilsMessengerCreateInfoEXT.messageSeverity = severityFlags;
		debugUtilsMessengerCreateInfoEXT.messageType = messageTypeFlags;
		debugUtilsMessengerCreateInfoEXT.pfnUserCallback = &debugCallback;
	}

	void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
			drawFrame();
		}
		device.waitIdle();
	}

	void cleanup() {
		glfwDestroyWindow(window);

		glfwTerminate();
	}

	void createInstance() {
		vk::ApplicationInfo appInfo { };
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = vk::ApiVersion13;

		std::vector<char const*> requiredLayers;
		if (enableValidationLayers) {
			requiredLayers.assign(validationLayers.begin(),
					validationLayers.end());
		}

		auto layerProperties = context.enumerateInstanceLayerProperties();
		auto unsupportedLayerIt = std::ranges::find_if(requiredLayers,
				[&layerProperties](auto const &requiredLayer) {
					return std::ranges::none_of(layerProperties,
							[requiredLayer](auto const &layerProperty) {
								return strcmp(layerProperty.layerName,
										requiredLayer) == 0;
							});
				});

		if (unsupportedLayerIt != requiredLayers.end()) {
			throw std::runtime_error(
					"Required layer not supported: "
							+ std::string(*unsupportedLayerIt));
		}

		auto requiredExtensions = getRequiredInstanceExtensions();

		auto extensionProperties =
				context.enumerateInstanceExtensionProperties();
		auto unsupportedPropertyIt = std::ranges::find_if(requiredExtensions,
				[&extensionProperties](auto const &requiredExtension) {
					return std::ranges::none_of(extensionProperties,
							[requiredExtension](auto const &extensionProperty) {
								return strcmp(extensionProperty.extensionName,
										requiredExtension) == 0;
							});
				});
		if (unsupportedPropertyIt != requiredExtensions.end()) {
			throw std::runtime_error(
					"Required extension not supported: "
							+ std::string(*unsupportedPropertyIt));
		}

		vk::InstanceCreateInfo createInfo { };
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledLayerCount =
				static_cast<uint32_t>(requiredLayers.size());
		createInfo.ppEnabledLayerNames = requiredLayers.data();
		createInfo.enabledExtensionCount =
				static_cast<uint32_t>(requiredExtensions.size());
		createInfo.ppEnabledExtensionNames = requiredExtensions.data();

		instance = vk::raii::Instance(context, createInfo);
	}

	std::vector<const char*> getRequiredInstanceExtensions() {
		uint32_t glfwExtensionCount = 0;
		auto glfwExtensions = glfwGetRequiredInstanceExtensions(
				&glfwExtensionCount);

		std::vector extensions(glfwExtensions,
				glfwExtensions + glfwExtensionCount);

		if (enableValidationLayers) {
			extensions.push_back(vk::EXTDebugUtilsExtensionName);
		}

		return extensions;
	}

	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
			vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
			vk::DebugUtilsMessageTypeFlagsEXT type,
			const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
			void *pUserData) {
		std::cerr << "validation layers: type " << to_string(type) << " msg: "
				<< pCallbackData->pMessage << std::endl;

		return vk::False;
	}

	static std::vector<char> readFile(const std::string &filename) {
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open()) {
			throw std::runtime_error("failed to open file!");
		}
		std::vector<char> buffer(file.tellg());
		file.seekg(0, std::ios::beg);
		file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
		file.close();
		return buffer;
	}

	vk::raii::ShaderModule createShaderModule(const std::vector<char> &code,
			const vk::raii::Device &device) {
		vk::ShaderModuleCreateInfo createInfo { };
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		return vk::raii::ShaderModule(device, createInfo);
	}

	void createGraphicsPipeline() {

		auto shaderCode = readFile("shaders/shader.spv");

		vk::raii::ShaderModule shaderModule = createShaderModule(shaderCode,
				device);

		vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
		vertShaderStageInfo.setStage(vk::ShaderStageFlagBits::eVertex);
		vertShaderStageInfo.setModule(*shaderModule);
		vertShaderStageInfo.setPName("vertMain");

		vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
		fragShaderStageInfo.setStage(vk::ShaderStageFlagBits::eFragment);
		fragShaderStageInfo.setModule(*shaderModule);
		fragShaderStageInfo.setPName("fragMain");

		vk::PipelineShaderStageCreateInfo shaderStages[] = {
				vertShaderStageInfo, fragShaderStageInfo };

		vk::PipelineVertexInputStateCreateInfo vertexInputInfo;

		vk::PipelineInputAssemblyStateCreateInfo inputAssembly { };
		inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

		vk::PipelineViewportStateCreateInfo viewportState { };
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;

		vk::PipelineRasterizationStateCreateInfo rasterizer { };
		rasterizer.depthClampEnable = vk::False;
		rasterizer.rasterizerDiscardEnable = vk::False;
		rasterizer.polygonMode = vk::PolygonMode::eFill;
		rasterizer.cullMode = vk::CullModeFlagBits::eNone;
		rasterizer.frontFace = vk::FrontFace::eClockwise;
		rasterizer.depthBiasEnable = vk::False;
		rasterizer.lineWidth = 1.0f;

		vk::PipelineMultisampleStateCreateInfo multisampling { };
		multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
		multisampling.sampleShadingEnable = vk::False;

		vk::PipelineColorBlendAttachmentState colorBlendAttachment = { };
		colorBlendAttachment.blendEnable = vk::False;
		colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR
				| vk::ColorComponentFlagBits::eG
				| vk::ColorComponentFlagBits::eB;

		vk::PipelineColorBlendStateCreateInfo colorBlending = { };
		colorBlending.logicOpEnable = vk::False;
		colorBlending.logicOp = vk::LogicOp::eCopy;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;

		std::vector<vk::DynamicState> dynamicStates = {
				vk::DynamicState::eViewport, vk::DynamicState::eScissor };

		vk::PipelineDynamicStateCreateInfo dynamicState = { };
		dynamicState.dynamicStateCount =
				static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();

		vk::PipelineLayoutCreateInfo pipelineLayoutInfo = { };
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pushConstantRangeCount = 0;

		pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

		vk::GraphicsPipelineCreateInfo graphicsPipelineInfo = { };
		vk::PipelineRenderingCreateInfo renderingPipelineInfo = { };

		graphicsPipelineInfo.stageCount = 2;
		graphicsPipelineInfo.pStages = shaderStages;
		graphicsPipelineInfo.pVertexInputState = &vertexInputInfo;
		graphicsPipelineInfo.pInputAssemblyState = &inputAssembly;
		graphicsPipelineInfo.pViewportState = &viewportState;
		graphicsPipelineInfo.pRasterizationState = &rasterizer;
		graphicsPipelineInfo.pMultisampleState = &multisampling;
		graphicsPipelineInfo.pColorBlendState = &colorBlending;
		graphicsPipelineInfo.pDynamicState = &dynamicState;
		graphicsPipelineInfo.layout = pipelineLayout;
		graphicsPipelineInfo.renderPass = nullptr;

		renderingPipelineInfo.colorAttachmentCount = 1;

		auto format = vk::Format::eB8G8R8A8Srgb;
		renderingPipelineInfo.pColorAttachmentFormats = &format;

		graphicsPipelineInfo.pNext = &renderingPipelineInfo;

		graphicsPipeline = vk::raii::Pipeline(device, nullptr,
				graphicsPipelineInfo);
	}

	void createCommandPool() {
		vk::CommandPoolCreateInfo poolInfo;
		poolInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
		poolInfo.setQueueFamilyIndex(queueIndex);

		commandPool = vk::raii::CommandPool(device, poolInfo);
	}

	void createCommandBuffer() {
		vk::CommandBufferAllocateInfo allocInfo;
		allocInfo.setCommandPool(*commandPool);
		allocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
		allocInfo.setCommandBufferCount(1);

		vk::raii::CommandBuffers cbs(device, allocInfo);
		commandBuffer = std::move(cbs.front());
	}

	void recordCommandBuffer(uint32_t imageIndex) {
		commandBuffer.begin( { });

		std::cout << "Drawing to: " << swapChainExtent.width << "x"
				<< swapChainExtent.height << std::endl;

		transition_image_layout(imageIndex, vk::ImageLayout::eUndefined,
				vk::ImageLayout::eColorAttachmentOptimal, { },
				vk::AccessFlagBits2::eColorAttachmentWrite,
				vk::PipelineStageFlagBits2::eColorAttachmentOutput,
				vk::PipelineStageFlagBits2::eColorAttachmentOutput);

		vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);

		vk::RenderingAttachmentInfo attachmentInfo = { };
		attachmentInfo.imageView = *swapChainImageViews[imageIndex];
		attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
		attachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
		attachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
		attachmentInfo.clearValue = clearColor;

		vk::RenderingInfo renderingInfo = { };
		renderingInfo.renderArea.offset = vk::Offset2D { 0, 0 };
		renderingInfo.renderArea.extent = swapChainExtent;
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &attachmentInfo;

		commandBuffer.beginRendering(renderingInfo);
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
				*graphicsPipeline);
		vk::Viewport viewport { 0.0f, 0.0f, (float) swapChainExtent.width,
				(float) swapChainExtent.height, 0.0f, 1.0f };
		commandBuffer.setViewport(0, viewport);
		vk::Rect2D scissor { { 0, 0 }, swapChainExtent };
		commandBuffer.setScissor(0, scissor);
		commandBuffer.draw(3, 1, 0, 0);
		commandBuffer.endRendering();

		transition_image_layout(imageIndex,
				vk::ImageLayout::eColorAttachmentOptimal,
				vk::ImageLayout::ePresentSrcKHR,
				vk::AccessFlagBits2::eColorAttachmentWrite, { },
				vk::PipelineStageFlagBits2::eColorAttachmentOutput,
				vk::PipelineStageFlagBits2::eBottomOfPipe);

		commandBuffer.end();
	}

	void transition_image_layout(uint32_t imageIndex,
			vk::ImageLayout old_layout, vk::ImageLayout new_layout,
			vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask,
			vk::PipelineStageFlags2 src_stage_mask,
			vk::PipelineStageFlags2 dst_stage_mask) {
		vk::ImageMemoryBarrier2 barrier = { };
		barrier.srcStageMask = src_stage_mask;
		barrier.srcAccessMask = src_access_mask;
		barrier.dstStageMask = dst_stage_mask;
		barrier.dstAccessMask = dst_access_mask;
		barrier.oldLayout = old_layout;
		barrier.newLayout = new_layout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = swapChainImages[imageIndex];

		barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		vk::DependencyInfo dependency_info { };
		dependency_info.dependencyFlags = { };
		dependency_info.imageMemoryBarrierCount = 1;
		dependency_info.pImageMemoryBarriers = &barrier;

		commandBuffer.pipelineBarrier2(dependency_info);
	}

	void createSyncObjects() {
		presentCompleteSemaphore = vk::raii::Semaphore(device,
				vk::SemaphoreCreateInfo());
		renderFinishedSemaphore = vk::raii::Semaphore(device,
				vk::SemaphoreCreateInfo());

		vk::FenceCreateInfo fenceInfo(vk::FenceCreateFlagBits::eSignaled);

		drawFence = vk::raii::Fence(device, fenceInfo);
	}

	void drawFrame() {

		static bool firstFrame = true;
		if (firstFrame) {
			std::cout << "First frame is drawing!" << std::endl;
			firstFrame = false;
		}

		auto fenceResult = device.waitForFences(*drawFence, vk::True,
		UINT64_MAX);
		if (fenceResult != vk::Result::eSuccess) {
			throw std::runtime_error("failed to wait for fence!");
		}
		device.resetFences(*drawFence);

		auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX,
				*presentCompleteSemaphore, nullptr);

		recordCommandBuffer(imageIndex);

		vk::PipelineStageFlags waitDestinationStageMask(
				vk::PipelineStageFlagBits::eColorAttachmentOutput);

		vk::PipelineStageFlags waitStage =
				vk::PipelineStageFlagBits::eColorAttachmentOutput;

		vk::SubmitInfo submitInfo = { };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &*presentCompleteSemaphore;
		submitInfo.pWaitDstStageMask = &waitDestinationStageMask;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &*commandBuffer;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &*renderFinishedSemaphore;

		graphicsQueue.submit(submitInfo, *drawFence);
		graphicsQueue.waitIdle();

		vk::PresentInfoKHR presentInfoKHR = { };
		presentInfoKHR.waitSemaphoreCount = 1;
		presentInfoKHR.pWaitSemaphores = &*renderFinishedSemaphore;
		presentInfoKHR.swapchainCount = 1;
		presentInfoKHR.pSwapchains = &*swapChain;
		presentInfoKHR.pImageIndices = &imageIndex;

		result = graphicsQueue.presentKHR(presentInfoKHR);

		switch (result) {
		case vk::Result::eSuccess:
			break;
		case vk::Result::eSuboptimalKHR:
			std::cout
					<< "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR !\n";
			break;
		default:
			break;
		}
	}
};

int main() {
	try {
		std::cout << "Current path is: " << std::filesystem::current_path()
				<< std::endl;

		HelloTriangleApplication app;

		app.run();
	} catch (std::exception &e) {
		std::cerr << e.what() << std::endl;

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

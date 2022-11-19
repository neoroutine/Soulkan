/*Include guards to avoid including this .hpp file multiple times*/
#ifndef SOULKAN_HPP
#define SOULKAN_HPP

#define SOULKAN_NAMESPACE sk
#define SOULKAN_TEST_NAMESPACE skt

/*Vulkan/GLFW includes*/
#define VMA_IMPLEMENTATION
#include<vk_mem_alloc.h>

#include <vulkan/vulkan.hpp>
#include <shaderc/shaderc.hpp>
#include <GLFW/glfw3.h>
#define GLFW_INCLUDE_VULKAN


/*Std includes*/
#include <fstream>
#include <iostream>
#include <format>
#include <deque>
#include <functional>
#include <thread>
#include <atomic>

/*GLM includes*/
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

/*Informations about comments and their "captions" (INFO, TODO, ...):
 *coordef INFO = A purely informational and context relevant comment
 *coordef TODO = Something must be tackled and taken care of here, in the near future
 *coordef MAYB = Something worth researching, not TODO worthy as it may not be worth implementing
 */

/*MAYB: Implement a VS extension summing up the different INFO / TODO / MAYB / ... comments in some kind of UI
 *Either one simple interface listing every "caption comments" or many distinct interfaces listing each type of "caption comment"
 */


/*Helpful macros*/
#define INDEX(x) (static_cast<size_t>(x))

#define VK_API_VERSION_FULL(packedVersion) (std::format("{}.{}.{}", VK_API_VERSION_MAJOR(packedVersion),  \
		                                                            VK_API_VERSION_MINOR(packedVersion),  \
		                                                            VK_API_VERSION_PATCH(packedVersion)))\

#define KILL(x) { std::cout << x << std::endl; std::exit(EXIT_FAILURE); }

#define VK_CHECK(x)                                                                                                             \
{                                                                                                                               \
	vk::Result error = x;                                                                                                       \
	if (error != vk::Result::eSuccess)                                                                                          \
	{                                                                                                                           \
		std::cout << std::format("Vulkan error at line {} in ({}) : {}", __LINE__, __FILE__, vk::to_string(error)) << std::endl;\
		KILL("Killing process");                                                                                                               \
	}                                                                                                                           \
}                                                                                                                               \

#define GLFW_CHECK()                                                                                                                                             \
{                                                                                                                                                       \
        const char* description = NULL;                                                                                                                           \
        int error = glfwGetError(&description);                                                                                                                   \
        if (error != GLFW_NO_ERROR)                                                                                                                               \
		{                                                                                                                                                         \
			std::cout << std::format("GFLW error at line {} in ({}) : {}", __LINE__, __FILE__, description != NULL ? description : "no description") << std::endl;\
            KILL("Killing process");                                                                                                                                             \
		}                                                                                                                                                         \
                                                                                                                                                                  \
   }                                                                                                                                                              \

#define DEBUGOUT(x) std::cout << "Line " << __LINE__ << " in (" << __FILE__ << ") : " << #x << " = " << x << std::endl;

//Implement appropriate Copy/Move constructors : important
//Keep in mind smart ptrs : important
//Implement modules to further debug or give more infos (like pipeline debug infos from vulkan extensions) : not important
//Move semantics and RAII work together with vulkan destruction : "It is valid to pass these values (VK_NULL_HANDLE, NULL) to vkDestroy* or vkFree* commands, which will silently ignore these values."
namespace SOULKAN_NAMESPACE
{
	//MAYB:Opt-in for manual destruction by setting manual to true, every class with destroyables should derive from Destroyable
	//INFO:vkCmdFillBuffer to clear buffer data
	class Destroyable
	{
	public:
		Destroyable()
		{

		}

		void setManual() { manual_ = true; }

	protected:
		bool manual_ = false;
		bool destroyed_ = false;
	};

	/*---------------------UTILS---------------------*/
	class DeletionQueue
	{
	public:
		DeletionQueue() {}

		void push(std::function<void()>&& fun)
		{
			deletors.push_back(fun);
		}
		
		void flush()
		{
			for (auto it = deletors.rbegin(); it != deletors.rend(); it++)
			{
				(*it)();
			}

			deletors.clear();
		}

	private:
		std::deque<std::function<void()>> deletors = {};
	};

	template<class T>
	using vec_ref = std::vector<std::reference_wrapper<T>>;

	template<class T>
	using ref = std::reference_wrapper<T>;

	/*---------------------GLFW---------------------*/
	class Window : Destroyable
	{
	public:
		Window(uint32_t width, uint32_t height, std::string title) :
			width_(width), height_(height), title_(title)
		{
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			window_ = glfwCreateWindow(width, height, title.c_str(), 0, NULL);
			GLFW_CHECK(/*Checking for correct window creation*/);
		}

		Window() : Window(800, 600, "Window") {}

		//Move constructor/assigment
		Window(Window&& other) noexcept :
			width_(other.width_), height_(other.height_), title_(other.title_), window_(other.window_)
		{
			other.width_ = 0;
			other.height_ = 0;
			other.window_ = nullptr;
			other.rename("");
		}

		//Destroys current window since it is being replaced with new one
		Window& operator=(Window&& other) noexcept
		{
			destroy();

			//Pointer to window is needed to rename moved into Window
			window_ = other.window_;
			other.window_ = nullptr;

			rename(other.title_);
			other.title_ = "";

			width_ = other.width_;
			other.width_ = 0;

			height_ = other.height_;
			other.height_ = 0;

			return *this;
		}

		//No copy constructor/assignment
		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		void destroy() 
		{ 
			if (destroyed_) { return; }
			if (window_ != nullptr) 
			{ 
				glfwDestroyWindow(window_); 
				window_ = nullptr; /*TODO:Check if setting to nullptr is redundant*/ 
			} 
			destroyed_ = true;
		}

		~Window() 
		{ 
			if (manual_) { return; }
			destroy();
		}

		void rename(std::string newTitle) 
		{ 
			if (window_ == nullptr) { return; }
			glfwSetWindowTitle(window_, newTitle.c_str()); 
			title_ = newTitle; 
		}

		uint32_t width() const     { return width_; }
		uint32_t height() const    { return height_; }
		std::string title() const  { return title_; }
		GLFWwindow* window() const { return window_; }


	private:
		uint32_t width_ = 0;
		uint32_t height_ = 0;
		std::string title_ = "";

		//INFO/TODO:No unique_ptr because GLFWwindow is an incomplete type (https://stackoverflow.com/a/6089065)
		GLFWwindow* window_ = nullptr;
	};
	
	/*---------------------VULKAN---------------------*/

	//INFO:Graphics and compute focused queue families can perform transfer operations
	enum class QueueFamilyCapability
	{
		GENERAL,
		GRAPHICS,
		//PRESENT,
		COMPUTE,
		TRANSFER,
		COUNT
	};

	class Instance : Destroyable
	{
	public:
		Instance(bool validation)
		{
			uint32_t glfwExtensionCount = 0;
			const char** glfwExtensions = nullptr;
			glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
			GLFW_CHECK(/*Checking if the last call did not trigger any error*/);

			//vector with (first, last) constructor
			auto extensions = std::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);

			appInfo_ = vk::ApplicationInfo("Soulkan", VK_MAKE_API_VERSION(0, 1, 0, 0), "Soulstream", VK_MAKE_API_VERSION(0, 1, 0, 0), VK_API_VERSION_1_3);

			std::vector<const char*> validationLayers = {};
			vk::DebugUtilsMessengerCreateInfoEXT debugCI = {};

			if (validation) 
			{ 
				extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
				validationLayers.push_back("VK_LAYER_KHRONOS_validation");

				debugCI.setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
					vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning);

				debugCI.setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
					vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
					vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance);

				debugCI.setPfnUserCallback(debugCallback);
				debugCI.setPUserData(nullptr);
			}

			extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);//INFO:Required by dynamic rendering and buffer device address

			vk::InstanceCreateInfo createInfo = {};
			createInfo.flags = vk::InstanceCreateFlags();
			createInfo.pApplicationInfo = &appInfo_;

			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
			createInfo.ppEnabledExtensionNames = extensions.data();

			instance_ = vk::createInstance(createInfo);

			if (validation)
			{
				auto dynamicLoader = vk::DispatchLoaderDynamic(instance_, vkGetInstanceProcAddr);
				VK_CHECK(instance_.createDebugUtilsMessengerEXT(&debugCI, nullptr, &debugMessenger_, dynamicLoader));
			}
		}

		Instance(Instance&& other) noexcept : 
			instance_(other.instance_), debugMessenger_(other.debugMessenger_), appInfo_(other.appInfo_)
		{
			other.instance_ = nullptr;
			other.debugMessenger_ = nullptr;
			other.appInfo_ = vk::ApplicationInfo{};
		}

		Instance& operator=(Instance&& other) noexcept
		{
			destroy();

			instance_ = other.instance_; 
			other.instance_ = nullptr;

			debugMessenger_ = other.debugMessenger_;
			other.debugMessenger_ = nullptr;

			appInfo_ = other.appInfo_;
			other.appInfo_ = vk::ApplicationInfo{};

			return *this;
		}

		//No copy constructor/assignment
		Instance(const Instance&) = delete;
		Instance& operator=(const Instance&) = delete;

		void destroy()
		{
			if (destroyed_) { return; }
			instance_.destroySurfaceKHR(surface_);
			auto dynamicLoader = vk::DispatchLoaderDynamic(instance_, vkGetInstanceProcAddr);
			instance_.destroyDebugUtilsMessengerEXT(debugMessenger_, nullptr, dynamicLoader);
			instance_.destroy();
			destroyed_ = true;
		}

		~Instance()
		{
			if (manual_) { return; }
			destroy();
		}

		vk::SurfaceKHR surface(Window& window)
		{
			if (surface_ != vk::SurfaceKHR(nullptr))
			{
				return surface_;
			}

			VkSurfaceKHR tmp;
			VK_CHECK(vk::Result(glfwCreateWindowSurface(instance_, window.window(), nullptr, &tmp)));
			GLFW_CHECK(/*Checking if window creation was succesfull, TODO:Not sure if necessary after VK_CHECK*/);

			if (tmp == VK_NULL_HANDLE) { KILL("An error occured in surface creation, killing process"); } //TODO:Not sure if necessary after VK_CHECK

			surface_ = vk::SurfaceKHR(tmp);
			return surface_;
		}

		std::vector<std::string> supportedExtensions()
		{
			if (supportedExtensions_.size() != 0)
			{
				return supportedExtensions_;
			}

			uint32_t availableExtensionCount = 0;
			VK_CHECK(vk::enumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr));

			if (availableExtensionCount == 0) { KILL("No supported instance extensions, killing process"); }

			std::vector<vk::ExtensionProperties> availableExtensions(availableExtensionCount);
			VK_CHECK(vk::enumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, availableExtensions.data()));

			//INFO:Reserves space for *availableExtensionCount* number of strings, leaving the vector empty.
			//Calling the size constructor on a vector actually creates *size* number of empty strings

			supportedExtensions_.reserve(availableExtensionCount);

			for (const auto& ext : availableExtensions)
			{
				supportedExtensions_.emplace_back(ext.extensionName.data());
			}

			return supportedExtensions_;
		}

		std::vector<vk::PhysicalDevice> availables()
		{
			if (availables_.size() != 0) { return availables_; }

			auto availables_ = instance_.enumeratePhysicalDevices();
			if (availables_.size() == 0) { KILL("No available physical devices, killing process"); }

			return availables_;
		}

		std::vector<vk::PhysicalDevice> suitables()
		{
			if (suitables_.size() != 0) { return suitables_; }

			for (const auto& available : availables())
			{
				auto features = available.getFeatures();

				//Requiring geometry shader for now as a baseline, more or less later on
				if (features.geometryShader)
				{
					suitables_.push_back(available);
				}
			}

			if (suitables_.size() == 0) { KILL("No suitables devices, killing process"); }

			return suitables_;
		}

		//TODO:Do more precise selection, handle case where no discrete gpus are found
		vk::PhysicalDevice best()
		{
			if (best_ != vk::PhysicalDevice(nullptr)) { return best_; }

			if (suitables().size() == 0) { KILL("No suitables devices, cannot find best one, killing process"); }
			if (suitables().size() == 1) { return suitables_[0]; }

			//Looking for discrete GPUs
			std::vector<vk::PhysicalDevice> discretes = {};
			for (const auto& s : suitables())
			{
				if (s.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
				{
					discretes.push_back(s);
				}
			}

			if (discretes.size() == 0) { KILL("No discrete gpus found, killing process"); }

			//Looking for GPUs with apiVersion atleast 1.3
			std::vector<vk::PhysicalDevice> apiConforming = {};
			for (const auto& d : discretes)
			{
				if (VK_API_VERSION_MINOR(d.getProperties().apiVersion) >= 3)
				{
					apiConforming.push_back(d);
				}
			}

			if (apiConforming.size() == 0) { KILL("No 1.3 api gpus found, killing process"); }

			best_ = apiConforming[0];
			return best_;
		}

		vk::Instance vk() const                      { return instance_; }
		vk::DebugUtilsMessengerEXT debugMessenger() const  { return debugMessenger_; }
		vk::ApplicationInfo appInfo() const                { return appInfo_; }

	private:
		vk::Instance instance_ = nullptr;
		vk::DebugUtilsMessengerEXT debugMessenger_ = nullptr;

		vk::ApplicationInfo appInfo_ = {};

		vk::SurfaceKHR surface_{};

		std::vector<std::string> supportedExtensions_ = {};

		std::vector<vk::PhysicalDevice> availables_ = {};
		std::vector<vk::PhysicalDevice> suitables_ = {};
		vk::PhysicalDevice best_ = nullptr;

		static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT messageType,
			const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* pUserData)
		{
			//TODO:Be able to specify a specific filename/filepath 
			//TODO:Specify severity at start of line
			std::ofstream out;
			out.open("debugMessenger.txt", std::ios::out | std::ios::app);
			out << pCallbackData->pMessage << "\n" << std::endl;
			out.close();

			return VK_FALSE;
		}
	};

	//Declare before use for Queue
	class Device;
	class CommandBuffer;
	class Swapchain;
	class Fence; //For Queue::submit
	class Semaphore; //For Queue::submit

	//QUEUE
	//Implement a busy queue index system: if end user got a queue from device.getQueue, mark queue as busy. when user calls getQueue return appropriate queue family with available index
	class Queue
	{
	public:
		Queue(Device& device, vk::Queue queue, QueueFamilyCapability family, uint32_t index) :
			device_(device), queue_(queue), family_(family), index_(index) {}

		//TODO:Implement move constructors
		Queue(Queue&& other) noexcept: device_(other.device_), queue_(other.queue_), family_(other.family_), index_(other.index_)
		{
			other.queue_ = vk::Queue(nullptr);

			other.family_ = QueueFamilyCapability::COUNT; //Debug value to indicate that queue is no longer viable

			other.index_ = 0;
		}

		Queue& operator=(Queue&& other) noexcept
		{
			device_ = other.device_;
			
			queue_ = other.queue_;
			other.queue_ = vk::Queue(nullptr);

			family_ = other.family_;
			other.family_ = QueueFamilyCapability::COUNT;

			index_ = other.index_;
			other.index_ = 0;
		}

		//No copy constructors
		Queue(Queue& other) = delete;
		Queue& operator=(Queue& other) = delete;

		void submit(CommandBuffer& commandBuffer, Semaphore &waitSemaphore, Semaphore &signalSemaphore, Fence &fence);
		void submit(CommandBuffer& commandBuffer, Fence& signalFence);

		void present(Swapchain& swapchain, Semaphore &waitSemaphore, uint32_t imageIndex);//Defined after swapchain definition (alongside submit)

		uint32_t index() const { return index_; }
	private:
		ref<Device> device_;
		vk::Queue queue_;
		QueueFamilyCapability family_;
		uint32_t index_;
	};

	//DEVICE
	class Device : Destroyable
	{
	public:
		Device(vk::PhysicalDevice physicalDevice, ref<Window> window, vk::SurfaceKHR surface) :
			physicalDevice_(physicalDevice), window_(window), surface_(surface)
		{
			//Queue families
			std::vector<vk::DeviceQueueCreateInfo> deviceQueueCreateInfos = {};
			float queuePriority = 1.0f; //MAYB:Look into queue priority
			for (const auto& q : queueConcentrate())
			{
				deviceQueueCreateInfos.push_back(vk::DeviceQueueCreateInfo(vk::DeviceQueueCreateFlags(), q, 1, &queuePriority));
			}

			auto deviceCreateInfo = vk::DeviceCreateInfo(vk::DeviceCreateFlags(),
				                                         static_cast<uint32_t>(deviceQueueCreateInfos.size()),
														 deviceQueueCreateInfos.data());

			//Extensions
			enabledExtensions_.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);//INFO:For presenting to the screen
			enabledExtensions_.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);//INFO:Avoid setting up Renderpasses and framebuffers
			enabledExtensions_.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);//INFO:New pipeline stages and synchronization structures/commands
			enabledExtensions_.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);//INFO:Query 64 bit buffer address to use it in shaders


			std::vector<const char*> enabledExtensionsC;
			enabledExtensionsC.reserve(enabledExtensions_.size());
			for (const auto& e : enabledExtensions_) { enabledExtensionsC.emplace_back(e.c_str()); }

			deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensionsC.size());
			deviceCreateInfo.ppEnabledExtensionNames = enabledExtensionsC.data();

			vk::PhysicalDeviceFeatures2 features = {};

			vk::PhysicalDeviceVulkan11Features features11 = {};
			features11.shaderDrawParameters = true;

			vk::PhysicalDeviceVulkan12Features features12 = {};
			features12.bufferDeviceAddress = true;
			features12.bufferDeviceAddressCaptureReplay = true;

			vk::PhysicalDeviceVulkan13Features features13 = {};
			features13.dynamicRendering = true;
			features13.synchronization2 = true;


			vk::PhysicalDeviceFeatures baseFeatures = {};
			baseFeatures.fillModeNonSolid = true; //INFO:Allows wireframe
			//baseFeatures.wideLines = true; //INFO:Allows lineWidth > 1.f
			baseFeatures.shaderInt64 = true; // Int64 in shaders

			deviceCreateInfo.pNext = &features;
			features.features = baseFeatures;//INFO:if a pNext chain is used (like here), do not use deviceCreateInfo.enabledFeatures = &enabledFeatures, use this instead
			features.pNext = &features11;
			features11.pNext = &features12;
			features12.pNext = &features13;

			//Building
			VK_CHECK(physicalDevice_.createDevice(&deviceCreateInfo, nullptr, &device_));
		}

		//TODO:Implement move constructors
		Device(Device&& other) noexcept : device_(other.device_), window_(other.window_), surface_(other.surface_),
			queueFamilies_(other.queueFamilies_), physicalDevice_(physicalDevice_)
		{
			destroyed_ = other.destroyed_;
			other.destroyed_ = true;

			manual_ = other.manual_;
			other.manual_ = false;

			other.device_ = vk::Device(nullptr);
			other.surface_ = vk::SurfaceKHR(nullptr);
			other.queueFamilies_ = {};
			other.enabledExtensions_ = {};
			other.physicalDevice_ = vk::PhysicalDevice(nullptr);
			other.supportedExtensions_ = {};
		}
		Device& operator=(Device&& other) noexcept
		{
			destroy();

			destroyed_ = other.destroyed_;
			other.destroyed_ = true;

			manual_ = other.manual_;
			other.manual_ = false;

			device_ = other.device_;
			other.device_ = vk::Device(nullptr);

			surface_ = other.surface_;
			other.surface_ = vk::SurfaceKHR(nullptr);

			queueFamilies_ = other.queueFamilies_;
			other.queueFamilies_ = {};

			enabledExtensions_ = other.enabledExtensions_;
			other.enabledExtensions_ = {};

			physicalDevice_ = other.physicalDevice_;
			other.physicalDevice_ = vk::PhysicalDevice(nullptr);

			supportedExtensions_ = other.supportedExtensions_;
			other.supportedExtensions_ = {};

			return *this;
		}

		//No copy constructors
		Device(Device& other) = delete;
		Device& operator=(Device& other) = delete;

		void destroy()
		{
			if (destroyed_) { return; }
			device_.destroy();
			destroyed_ = true;
		}

		~Device()
		{
			if (manual_) { return; }
			destroy();
		}
		//TODO:Implement generic destroy, calling getProcAddr to get correct destroyFunction according to type of parameter
		//Fence

		void waitFence(Fence &fence);

		void resetFence(Fence &fence);


		vk::Extent2D extent()
		{
			auto surfaceCapabilities = physicalDevice_.getSurfaceCapabilitiesKHR(surface_);

			int width, height = 0;
			glfwGetFramebufferSize(window_.get().window(), &width, &height);

			vk::Extent2D extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

			extent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width, extent.width));
			extent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, extent.height));

			return extent;
		}

		vk::SurfaceFormatKHR surfaceFormat()
		{
			std::vector<vk::SurfaceFormatKHR> surfaceFormats = physicalDevice_.getSurfaceFormatsKHR(surface_);

			if (surfaceFormats.size() == 0)
			{
				KILL("No surface formats found");
			}

			if (surfaceFormats.size() == 1 && surfaceFormats[0].format == vk::Format::eUndefined)
			{
				KILL("Only format found is undefined");
			}

			if (surfaceFormats.size() == 1)
			{
				return surfaceFormats[0];
			}

			for (const auto& surfaceFormat : surfaceFormats)
			{
				if (surfaceFormat.format == vk::Format::eB8G8R8A8Unorm && surfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
				{
					return surfaceFormat;
				}
			}

			return vk::Format::eUndefined; //TODO:Check if it should be returned
		}

		vk::PresentModeKHR presentMode()
		{
			return vk::PresentModeKHR::eFifo; //FIFO is the only present mode required to be supported and is the chosen one for now


			bool fifoRFound   = false;
			bool fifoFound    = false; 

			auto presentModes = physicalDevice_.getSurfacePresentModesKHR(surface_);

			//Looking for mailbox > FIFO Relaxed > FIFO
			for (const auto& m : presentModes)
			{
				if (m == vk::PresentModeKHR::eMailbox) { return m; }
				if (m == vk::PresentModeKHR::eFifoRelaxed) { fifoRFound = true; }
				if (m == vk::PresentModeKHR::eFifo) { fifoFound = true; }
			}

			if (fifoRFound) { return vk::PresentModeKHR::eFifoRelaxed; }
			if (fifoFound) { return vk::PresentModeKHR::eFifo; }

			KILL("No present modes found");
		}

		std::vector<std::string> supportedExtensions()
		{
			if (supportedExtensions_.size() != 0) { return supportedExtensions_; }

			uint32_t extensionCount = 0;
			VK_CHECK(physicalDevice_.enumerateDeviceExtensionProperties(nullptr, &extensionCount, nullptr));

			if (extensionCount == 0) { KILL("Found no physical device extensions, killing process"); }

			std::vector<vk::ExtensionProperties> extensions(extensionCount);
			VK_CHECK(physicalDevice_.enumerateDeviceExtensionProperties(nullptr, &extensionCount, extensions.data()));

			std::vector<std::string> extensionNames;
			supportedExtensions_.reserve(extensionCount);

			for (const auto& e : extensions)
			{
				supportedExtensions_.emplace_back(e.extensionName.data());
			}

			return supportedExtensions_;
		}

		bool isSupported(std::string extension)
		{
			//Intentionally discarding return value of supportedExtensions() since the return value is already written to supportedExtensions_
			static_cast<void>(supportedExtensions());

			return std::find(supportedExtensions_.begin(), supportedExtensions_.end(), extension) != supportedExtensions_.end();
		}

		//                                        GENERAL, GRAPHICS, COMPUTE, TRANSFER
		//std::array<uint32_t, 4> queueFamilies = {x,      y,        z,       w}
		std::array<uint32_t, 4> queueFamilies()
		{
			if (queueFamilies_[INDEX(QueueFamilyCapability::GENERAL)] != std::numeric_limits<uint32_t>::max() ||
				queueFamilies_[INDEX(QueueFamilyCapability::GRAPHICS)] != std::numeric_limits<uint32_t>::max() ||
				queueFamilies_[INDEX(QueueFamilyCapability::COMPUTE)] != std::numeric_limits<uint32_t>::max() ||
				queueFamilies_[INDEX(QueueFamilyCapability::TRANSFER)] != std::numeric_limits<uint32_t>::max())
			{
				return queueFamilies_;
			}

			uint32_t qfCount = 0;

			//INFO:Can not use VK_CHECK macro, no overload with vk::Result return type
			std::vector<vk::QueueFamilyProperties> qf = physicalDevice_.getQueueFamilyProperties();

			if (qf.size() == 0) { KILL("Foud no queue families, killing process"); }

			//Looking for general queue, capable of every operations
			//Must support presenting
			for (uint32_t i = 0; i < qf.size(); ++i)
			{
				if ((qf[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
					(qf[i].queueFlags & vk::QueueFlagBits::eCompute) &&
					(qf[i].queueFlags & vk::QueueFlagBits::eTransfer) &&/*Optional line, written for clarity's sake*/
					(physicalDevice_.getSurfaceSupportKHR(i, surface_)))
				{
					queueFamilies_.fill(i);
				}
			}

			//INFO:Reminder that graphics and compute queues are capable of transfer operations
			for (uint32_t i = 0; i < qf.size(); ++i)
			{
				//Purely graphics queue if available
				//Must support presenting
				if ((qf[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
					!(qf[i].queueFlags & vk::QueueFlagBits::eCompute) &&
					(physicalDevice_.getSurfaceSupportKHR(i, surface_)))
				{
					queueFamilies_[INDEX(QueueFamilyCapability::GRAPHICS)] = i;
				}

				//Purely compute queue if available
				if ((qf[i].queueFlags & vk::QueueFlagBits::eCompute) &&
					!(qf[i].queueFlags & vk::QueueFlagBits::eGraphics))
				{
					queueFamilies_[INDEX(QueueFamilyCapability::COMPUTE)] = i;
				}

				//Purely transfer queue if available
				if ((qf[i].queueFlags & vk::QueueFlagBits::eTransfer) &&
					!(qf[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
					!(qf[i].queueFlags & vk::QueueFlagBits::eCompute))
				{
					queueFamilies_[INDEX(QueueFamilyCapability::TRANSFER)] = i;
				}
			}

			if (!queueAvailable(QueueFamilyCapability::GENERAL) && !queueAvailable(QueueFamilyCapability::GRAPHICS))
			{
				KILL("No general or graphics queue found, killing process");
			}

			return queueFamilies_;
		}

		std::vector<uint32_t> queueConcentrate()
		{
			std::vector<uint32_t> concentrate = {};

			//Filter out not found queues
			for (const auto& q : queueFamilies())
			{
				bool alreadyThere = false;
				for (const auto& cq : concentrate)
				{
					if (q == cq) { alreadyThere = true; }
				}

				if ((q != std::numeric_limits<uint32_t>::max()) && !alreadyThere)
				{
					concentrate.push_back(q);
				}
			}

			return concentrate;
		}

		uint32_t queueIndex(QueueFamilyCapability queueFamilyCapability)
		{
			return queueFamilies()[INDEX(queueFamilyCapability)];
		}

		Queue queue(QueueFamilyCapability family, uint32_t index)
		{
			auto queue = device_.getQueue(queueFamilies()[INDEX(family)], index);

			return Queue(*this, queue, family, index);
		}

		vk::Device vk() const { return device_; }
		vk::PhysicalDevice physicalDevice() const { return physicalDevice_; }
		vk::SurfaceKHR surface() const { return surface_; }

	private:
		vk::Device device_ = nullptr;
		ref<Window> window_; //TODO:Unsure about this
		vk::SurfaceKHR surface_ = nullptr;

		std::array<uint32_t, INDEX(QueueFamilyCapability::COUNT)> queueFamilies_
		{ [] { std::array <uint32_t, INDEX(QueueFamilyCapability::COUNT)> temp; temp.fill(std::numeric_limits<uint32_t>::max()); return temp; }() }; //Debug value

		std::vector<std::string> enabledExtensions_ = {};

		vk::PhysicalDevice physicalDevice_ = nullptr;
		std::vector<std::string> supportedExtensions_ = {};

		//If queue at index is defined (other than uint32_t max) then it is available to use
		bool queueAvailable(QueueFamilyCapability capability)
		{
			return (queueFamilies_[INDEX(capability)] != std::numeric_limits<uint32_t>::max());
		}
	};

	//FENCE
	class Fence : Destroyable
	{
	public:
		Fence(ref<Device> device) : device_(device)
		{
			vk::FenceCreateInfo createInfo = {};
			createInfo.flags = vk::FenceCreateFlagBits::eSignaled; //INFO:Fence is created as signaled so that the first call to waitForFences() returns instantly

			vk::Fence fence;
			VK_CHECK(device_.get().vk().createFence(&createInfo, nullptr, &fence));
			
			fence_ = fence;
		}

		//TODO:Implement move constructors
		Fence(Fence&& other) noexcept : device_(other.device_), fence_(other.fence_)
		{
			destroyed_ = other.destroyed_;
			other.destroyed_ = true;

			manual_ = other.manual_;
			other.manual_ = false;

			other.fence_ = vk::Fence(nullptr);
		}

		Fence& operator=(Fence&& other) noexcept
		{
			destroy();

			destroyed_ = other.destroyed_;
			other.destroyed_ = true;

			manual_ = other.manual_;
			other.manual_ = false;

			device_ = other.device_;

			fence_ = other.fence_;
			other.fence_ = vk::Fence(nullptr);

			return *this;
		}

		//No copy constructors
		Fence(Fence& other) = delete;
		Fence& operator=(Fence& other) = delete;

		void destroy()
		{
			if (destroyed_) { return; }
			device_.get().waitFence(*this); //Must not destroy fence in use
			device_.get().vk().destroyFence(fence_);
			destroyed_ = true;
		}

		~Fence()
		{
			if (manual_) { return; }
			destroy();
		}

		vk::Fence vk() const
		{
			return fence_;
		}
	private:
		ref<Device> device_;
		vk::Fence fence_{};
	};

	//SEMAPHORE
	class Semaphore : Destroyable
	{
	public:
		Semaphore(ref<Device> device) : device_(device)
		{
			vk::SemaphoreCreateInfo createInfo = {};

			VK_CHECK(device_.get().vk().createSemaphore(&createInfo, nullptr, &semaphore_));
		}

		//TODO:Implement move constructors
		Semaphore(Semaphore&& other) noexcept : device_(other.device_), semaphore_(other.semaphore_)
		{
			destroyed_ = other.destroyed_;
			other.destroyed_ = true;

			manual_ = other.manual_;
			other.manual_ = false;

			other.semaphore_ = vk::Semaphore(nullptr);
		}

		Semaphore& operator=(Semaphore&& other) noexcept
		{
			destroyed_ = other.destroyed_;
			other.destroyed_ = true;

			manual_ = other.manual_;
			other.manual_ = false;

			device_ = other.device_;

			semaphore_ = other.semaphore_;
			other.semaphore_ = vk::Semaphore(nullptr);

			return *this;
		}

		//No copy constructors
		Semaphore(Semaphore& other) = delete;
		Semaphore& operator=(Semaphore& other) = delete;

		void destroy()
		{
			if (destroyed_) { return; }
			device_.get().vk().destroySemaphore(semaphore_);
			destroyed_ = true;
		}

		~Semaphore()
		{
			if (manual_) { return; }
			destroy();
		}

		vk::Semaphore vk() const
		{
			return semaphore_;
		}
	private:
		ref<Device> device_;
		vk::Semaphore semaphore_;
	};

	//DEVICE
	void Device::waitFence(Fence &fence)
	{
		vk::Fence vkFence = fence.vk();
		VK_CHECK(device_.waitForFences(1, &vkFence, true, 999'999'999));//TODO:Temporary hacky fix to avoid timeout when loading big .obj
	}

	void Device::resetFence(Fence &fence)
	{
		vk::Fence vkFence = fence.vk();
		VK_CHECK(device_.resetFences(1, &vkFence));
	}

	//SWAPCHAIN
	class Swapchain : Destroyable
	{
	public:

		Swapchain(ref<Device> device) : device_(device)
		{
			auto surface = device_.get().surface();
			auto surfaceCapabilities = device_.get().physicalDevice().getSurfaceCapabilitiesKHR(surface);

			//Image Count
			uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
			if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount)
			{
				imageCount = surfaceCapabilities.maxImageCount;
			}

			//Extent
			extent_ = device_.get().extent();

			//Concurrency
			auto sharingMode = vk::SharingMode::eConcurrent; //TODO:Defaulting to concurrent, read up on it later on

			//SurfaceFormat
			surfaceFormat_ = device_.get().surfaceFormat();

			//PresentMode
			presentMode_ = device_.get().presentMode();

			//Queues
			auto queues = device_.get().queueConcentrate();

			auto createInfo = vk::SwapchainCreateInfoKHR(vk::SwapchainCreateFlagsKHR());

			createInfo.surface = surface;
			createInfo.minImageCount = imageCount;

			createInfo.imageFormat = surfaceFormat_.format;
			createInfo.imageColorSpace = surfaceFormat_.colorSpace;

			createInfo.imageExtent = extent_;

			createInfo.imageArrayLayers = 1; //1 for non-stereoscopic 3D apps
			createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment; //Image can be used to create a VkImageView

			createInfo.queueFamilyIndexCount = static_cast<uint32_t>(queues.size());
			createInfo.pQueueFamilyIndices = queues.data();

			createInfo.imageSharingMode = sharingMode;

			createInfo.preTransform = surfaceCapabilities.currentTransform; //TODO:Read up on both lines
			createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;

			createInfo.presentMode = presentMode_;

			createInfo.clipped = VK_TRUE; //Allows vulkan implementation to discard rendering operations on regions of the surface not visible
			createInfo.oldSwapchain = vk::SwapchainKHR(nullptr); 

			VK_CHECK(device_.get().vk().createSwapchainKHR(&createInfo, nullptr, &swapchain_));

		}

		//TODO:Handle move constructors/assignements
		Swapchain(Swapchain&& other) noexcept :
			device_(other.device_), swapchain_(other.swapchain_), surfaceFormat_(other.surfaceFormat_),
			presentMode_(other.presentMode_), extent_(other.extent_), images_(other.images_),
			imageViews_(other.imageViews_)
		{
			destroyed_ = other.destroyed_;
			other.destroyed_ = true;

			manual_ = other.manual_;
			other.manual_ = false;

			other.swapchain_ = vk::SwapchainKHR(nullptr);
			other.surfaceFormat_ = vk::SurfaceFormatKHR();
			other.presentMode_ = vk::PresentModeKHR();
			other.extent_ = vk::Extent2D{0,0};
			other.images_ = {};
			other.imageViews_ = {};
		}

		Swapchain& operator=(Swapchain& other) noexcept
		{
			destroy();

			destroyed_ = other.destroyed_;
			other.destroyed_ = true;

			manual_ = other.manual_;
			other.manual_ = false;

			swapchain_ = other.swapchain_;
			other.swapchain_ = vk::SwapchainKHR(nullptr);

			surfaceFormat_ = other.surfaceFormat_;
			other.surfaceFormat_ = vk::SurfaceFormatKHR();

			presentMode_ = other.presentMode_;
			other.presentMode_ = vk::PresentModeKHR();

			extent_ = other.extent_;
			other.extent_ = vk::Extent2D{ 0,0 };

			images_ = other.images_;
			other.images_ = {};

			imageViews_ = other.imageViews_;
			other.imageViews_ = {};

			return *this;
		}

		Swapchain(Device&& device) = delete; //INFO:Prevents rvalue binding, otherwise the class would accept temporaries
		Swapchain& operator=(Device& other) = delete;

		//INFO:"Application-created vk::Image need to be destroyed, unlike images retrieved from vkGetSwapchainImagesKHR" - quote from vulkan spec
		std::vector<vk::Image> images()
		{
			if (images_.size() != 0)
			{
				return images_;
			}

			uint32_t imageCount = 0;
			VK_CHECK(device_.get().vk().getSwapchainImagesKHR(swapchain_, &imageCount, nullptr));
			images_.resize(imageCount);

			VK_CHECK(device_.get().vk().getSwapchainImagesKHR(swapchain_, &imageCount, images_.data()));

			return images_;
		}

		std::vector<vk::ImageView> imageViews()
		{
			if (imageViews_.size() != 0)
			{
				return imageViews_;
			}

			for (const auto& img : images())
			{
				vk::ImageViewCreateInfo imageViewCreateInfo = {};

				imageViewCreateInfo.image = img;
				imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
				imageViewCreateInfo.format = surfaceFormat_.format;

				imageViewCreateInfo.components.r = vk::ComponentSwizzle::eIdentity;
				imageViewCreateInfo.components.g = vk::ComponentSwizzle::eIdentity;
				imageViewCreateInfo.components.g = vk::ComponentSwizzle::eIdentity;

				imageViewCreateInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
				imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
				imageViewCreateInfo.subresourceRange.levelCount = 1;
				imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
				imageViewCreateInfo.subresourceRange.layerCount = 1;

				vk::ImageView imageView;
				VK_CHECK(device_.get().vk().createImageView(&imageViewCreateInfo, nullptr, &imageView));

				imageViews_.push_back(imageView);
			}

			return imageViews_;
		}

		uint32_t nextImage(Semaphore &signalSemaphore)
		{
			uint32_t imageIndex;
			VK_CHECK(device_.get().vk().acquireNextImageKHR(swapchain_, 1'000'000, signalSemaphore.vk(), nullptr, &imageIndex));

			return imageIndex;
		}

		void destroy()
		{
			if (destroyed_) { return; }
			device_.get().vk().destroySwapchainKHR(swapchain_);

			for (auto& i : imageViews_)
			{
				device_.get().vk().destroyImageView(i);
			}
			destroyed_ = true;
		}

		~Swapchain()
		{
			if (manual_) { return; }
			destroy();
		}

		vk::SwapchainKHR vk() const { return swapchain_; }
		vk::Extent2D extent() const  { return extent_; }
		vk::SurfaceFormatKHR surfaceFormat() const  { return surfaceFormat_; }
		vk::Format imageFormat() const  { return surfaceFormat_.format; }
		vk::PresentModeKHR presentMode() const  { return presentMode_; }
	private:
		ref<Device> device_;
		vk::SwapchainKHR swapchain_{};

		vk::SurfaceFormatKHR surfaceFormat_{};
		vk::PresentModeKHR presentMode_{};
		vk::Extent2D extent_{};

		std::vector<vk::Image> images_{};
		std::vector<vk::ImageView> imageViews_{};

	};

	//Declare before use for CommandPool
	class CommandPool;

	//COMMAND BUFFER
	//MAYB:Might want to tie swapchain to command buffer 
	class CommandBuffer : Destroyable
	{
	public:
		CommandBuffer(CommandPool& commandPool, vk::CommandBuffer commandBuffer) : commandPool_(commandPool), commandBuffer_(commandBuffer) {}

		//TODO:Implement move constructors
		CommandBuffer(CommandBuffer&& other) noexcept : commandPool_(other.commandPool_), commandBuffer_(other.commandBuffer_)
		{
			destroyed_ = other.destroyed_;
			other.destroyed_ = true;

			manual_ = other.manual_;
			other.manual_ = false;

			other.commandBuffer_ = vk::CommandBuffer(nullptr);
		}

		CommandBuffer& operator=(CommandBuffer&& other) = delete;

		//No copy constructors
		CommandBuffer(CommandBuffer& other) = delete;
		CommandBuffer& operator=(CommandBuffer& other) = delete;

		void begin()
		{
			commandBuffer_.reset(); //Resetting buffer before starting it

			vk::CommandBufferBeginInfo beginInfo = {};

			beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;; //INFO:Submit once and then record again

			beginInfo.pInheritanceInfo = nullptr;
			beginInfo.pNext = nullptr;

			VK_CHECK(commandBuffer_.begin(&beginInfo));
		}

		void end()
		{
			commandBuffer_.end();
		}

		void beginRendering(vk::ImageView colorView, vk::ImageView depthView, vk::Extent2D extent,
			vk::ClearColorValue clearColor)
		{
			//If command pool queue family is not general or graphics, do not begin rendering
			if (!graphics()) { return; }

			//Structs and actual command
			vk::RenderingAttachmentInfo colorAttachment = {};
			colorAttachment.imageView = colorView;
			colorAttachment.imageLayout = vk::ImageLayout::eAttachmentOptimal;

			colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
			colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;

			colorAttachment.clearValue.color = clearColor;

			vk::RenderingAttachmentInfo depthAttachment = {};
			depthAttachment.imageView = depthView;
			depthAttachment.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
			
			depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
			depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;

			depthAttachment.clearValue.depthStencil.depth = 1.f;
			//depthAttachment.clearValue.color = depthClear;

			vk::RenderingInfo renderingInfo = {};
			renderingInfo.renderArea = { 0, 0, extent.width, extent.height };
			renderingInfo.layerCount = 1; //INFO:There can be multiple layers in a single image to pack up things more efficiently

			renderingInfo.colorAttachmentCount = 1;
			renderingInfo.pColorAttachments = &colorAttachment;
			renderingInfo.pDepthAttachment = &depthAttachment;

			commandBuffer_.beginRendering(&renderingInfo);
		}

		void endRendering()
		{
			if (!graphics()) { return; }

			commandBuffer_.endRendering();
		}

		void imageLayoutTransition(vk::ImageLayout old, vk::ImageLayout next, vk::Image image,
								   vk::PipelineStageFlags2 src, vk::AccessFlags2 srcAccess,
								   vk::PipelineStageFlags2 dst, vk::AccessFlags2 dstAccess)
		{
			vk::ImageMemoryBarrier2 imageBarrier = {};
			imageBarrier.oldLayout = old;
			imageBarrier.newLayout = next;

			imageBarrier.image = image;
			imageBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor; //TODO:Understand what's a subresourcerange
			imageBarrier.subresourceRange.baseMipLevel = 0;
			imageBarrier.subresourceRange.levelCount = 1;
			imageBarrier.subresourceRange.baseArrayLayer = 0;
			imageBarrier.subresourceRange.layerCount = 1;

			imageBarrier.srcStageMask = src;
			imageBarrier.srcAccessMask = srcAccess;

			imageBarrier.dstStageMask = dst;
			imageBarrier.dstAccessMask = dstAccess;

			vk::DependencyInfo dependencyInfo = {};
			dependencyInfo.imageMemoryBarrierCount = 1;
			dependencyInfo.pImageMemoryBarriers = &imageBarrier;

			commandBuffer_.pipelineBarrier2(dependencyInfo);
		}

		bool graphics() const;//Defined after CommandPool definition

		vk::CommandBuffer vk() const  { return commandBuffer_; }

	private:
		vk::CommandBuffer commandBuffer_;
		ref<CommandPool> commandPool_;
	};

	//COMMAND POOL
	class CommandPool : Destroyable
	{
	public:
		//TODO:Change second parameter to QueueFamilyCapability instead of index, no need to expose that kind of complexity to the caller
		CommandPool(Device& device, uint32_t queueFamilyIndex) : device_(device), queueFamilyIndex_(queueFamilyIndex)
		{
			vk::CommandPoolCreateInfo createInfo = {};
			
			createInfo.queueFamilyIndex = queueFamilyIndex;
			createInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer; // Allows command buffers to be reset individually
			
			createInfo.pNext = nullptr;

			VK_CHECK(device_.get().vk().createCommandPool(&createInfo, nullptr, &pool_));
		}

		//Allocating only one command buffer for now since it is not trivial returning a vector of non copyable-non moveable objects
		CommandBuffer allocate()
		{
			vk::CommandBuffer vkBuffer;

			vk::CommandBufferAllocateInfo allocateInfo = {};
			allocateInfo.commandPool = pool_;
			allocateInfo.commandBufferCount = 1;
			allocateInfo.level = vk::CommandBufferLevel::ePrimary;

			allocateInfo.pNext = nullptr;

			VK_CHECK(device_.get().vk().allocateCommandBuffers(&allocateInfo, &vkBuffer)); //

			return CommandBuffer(*this, vkBuffer);
		}



		//TODO:Implement move constructors
		CommandPool(CommandPool&& other) noexcept : device_(other.device_), pool_(other.pool_), queueFamilyIndex_(other.queueFamilyIndex_)
		{
			destroyed_ = other.destroyed_;
			other.destroyed_ = true;

			manual_ = other.manual_;
			other.manual_ = false;

			other.pool_ = vk::CommandPool(nullptr);

		}
		CommandPool& operator=(CommandPool&& other) noexcept
		{
			destroy();

			destroyed_ = other.destroyed_;
			other.destroyed_ = true;

			manual_ = other.manual_;
			other.manual_ = false;

			device_ = other.device_;

			pool_ = other.pool_;
			other.pool_ = vk::CommandPool(nullptr);

			queueFamilyIndex_ = other.queueFamilyIndex_;

			return *this;
		}

		//No copy constructors
		CommandPool(CommandPool& other) = delete;
		CommandPool& operator=(CommandPool& other) = delete;

		void destroy()
		{
			if (destroyed_) { return; }
			device_.get().vk().destroyCommandPool(pool_);
			destroyed_ = true;
		}

		~CommandPool()
		{
			if (manual_) { return; }
			destroy();
		}

		vk::CommandPool vk() const { return pool_; }
		uint32_t index() const { return queueFamilyIndex_; }

		ref<Device> device() const { return device_; }

	private:
		ref<Device> device_;
		vk::CommandPool pool_;
		uint32_t queueFamilyIndex_;

	};

	//COMMAND BUFFER
	//Returns true if command buffer was allocated from a graphics capable command pool
	bool CommandBuffer::graphics() const
	{
		return (commandPool_.get().index() == commandPool_.get().device().get().queueFamilies()[INDEX(QueueFamilyCapability::GENERAL)]) ||
			(commandPool_.get().index() == commandPool_.get().device().get().queueFamilies()[INDEX(QueueFamilyCapability::GRAPHICS)]);
	}

	//QUEUE
	void Queue::submit(CommandBuffer& commandBuffer, Semaphore &waitSemaphore, Semaphore &signalSemaphore, Fence &signalFence)//Defined after CommandBuffer definition
	{
		vk::SubmitInfo submitInfo = {};

		vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

		submitInfo.pWaitDstStageMask = &waitStage;

		vk::Semaphore vkWaitSemaphore = waitSemaphore.vk();
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &vkWaitSemaphore;

		vk::Semaphore vkSignalSemaphore = signalSemaphore.vk();
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &vkSignalSemaphore;

		vk::CommandBuffer vkCommandBuffer = commandBuffer.vk();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &vkCommandBuffer;

		//signalFence 
		VK_CHECK(queue_.submit(1, &submitInfo, signalFence.vk()));
	}

	void Queue::submit(CommandBuffer& commandBuffer, Fence& signalFence)
	{
		vk::SubmitInfo submitInfo = {};
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = nullptr;
		submitInfo.pWaitDstStageMask = nullptr;

		submitInfo.signalSemaphoreCount = 0;
		submitInfo.pSignalSemaphores = nullptr;

		vk::CommandBuffer vkBuffer = commandBuffer.vk();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &vkBuffer;

		VK_CHECK(queue_.submit(1, &submitInfo, signalFence.vk()));
	}

	void Queue::present(Swapchain& swapchain, Semaphore &waitSemaphore, uint32_t imageIndex)
	{
		vk::PresentInfoKHR presentInfo = {};

		auto vkSwapchain = swapchain.vk();
		presentInfo.pSwapchains = &vkSwapchain;
		presentInfo.swapchainCount = 1;

		vk::Semaphore vkWaitSemaphore = waitSemaphore.vk();
		presentInfo.pWaitSemaphores = &vkWaitSemaphore;
		presentInfo.waitSemaphoreCount = 1;

		presentInfo.pImageIndices = &imageIndex;

		VK_CHECK(queue_.presentKHR(&presentInfo));
	}

	//TODO:Save compiled spirv to file for later re-use
	class Shader : Destroyable
	{
	public:
		Shader(ref<Device> device, std::string filename, vk::ShaderStageFlagBits shaderStage) : device_(device), filename_(filename), stage_(shaderStage) {}

		//TODO:Implement move constructors
		Shader(Shader&& other) noexcept : device_(other.device_), filename_(other.filename_), source_(other.source_),
			module_(other.module_), stage_(other.stage_)
		{
			destroyed_ = other.destroyed_;
			other.destroyed_ = true;

			manual_ = other.manual_;
			other.manual_ = false;

			other.filename_ = "";
			other.source_ = "";
			other.module_ = vk::ShaderModule(nullptr);
		}
		Shader& operator=(Shader&& other) noexcept
		{
			destroy();

			destroyed_ = other.destroyed_;
			other.destroyed_ = true;

			manual_ = other.manual_;
			other.manual_ = false;

			filename_ = other.filename_;
			other.filename_ = "";

			source_ = other.source_;
			other.source_ = "";

			module_ = other.module_;
			other.module_ = vk::ShaderModule(nullptr);

			stage_ = other.stage_;

			return *this;
		}

		//No copy constructors
		Shader(Shader& other) = delete;
		Shader& operator=(Shader& other) = delete;

		void destroy()
		{
			if (destroyed_) { return; }
			device_.get().vk().destroyShaderModule(module_);
			destroyed_ = true;
		}

		~Shader()
		{
			if (manual_) { return; }
			destroy();
		}
		
		//INFO:Expensive, reads entire file
		//INFO:Reads the entire file again if readAgain is set to true
		std::string source(bool readAgain = false)
		{
			if (source_.size() != 0 && !readAgain)
			{
				return source_;
			}

			std::ifstream in(filename_);

			if (!in.is_open()) { KILL("Shader file does not exist"); }

			std::stringstream buffer;
			buffer << in.rdbuf();

			source_ = buffer.str();

			in.close();

			return source_;
		}

		//INFO:Expensive, will compile glsl to spirv and then create module according to spirv binary if it has not been compiled yet
		//INFO:Recompile if needed
		vk::ShaderModule shader(bool recompile = false)
		{
			if (compiled)
			{
				if (recompile)
				{
					std::string oldSource = source_;
					std::string newSource = source(true);

					if (oldSource == newSource) 
					{
						//INFO:Same source, no modifications seen, returning same module
						return module_;
					}
				}
				else
				{
					return module_;
				}
				
			}

			shaderc::Compiler compiler;
			shaderc::CompileOptions options;

			shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source(), kind(), filename_.c_str());

			if (result.GetCompilationStatus() != shaderc_compilation_status_success)
			{
				std::string errorMessage = std::format("Could not compile shader : [{}], error: [{}]", filename_, result.GetErrorMessage());
				KILL(errorMessage);
			}

			std::vector<uint32_t> spirv;
			spirv.assign(result.cbegin(), result.cend());

			vk::ShaderModuleCreateInfo createInfo = {};
			createInfo.codeSize = spirv.size()*sizeof(uint32_t);
			createInfo.pCode = spirv.data();

			VK_CHECK(device_.get().vk().createShaderModule(&createInfo, nullptr, &module_));
			compiled = true;

			return module_;
		}

		vk::ShaderStageFlagBits stage() const
		{
			return stage_;
		}
	private:
		ref<Device> device_;
		std::string filename_{};
		std::string source_{};
		vk::ShaderModule module_;
		vk::ShaderStageFlagBits stage_;

		bool compiled = false; //Compilation state after latest changes

		shaderc_shader_kind kind()
		{
			if (stage_ == vk::ShaderStageFlagBits::eVertex) { return shaderc_shader_kind::shaderc_vertex_shader; }
			if (stage_ == vk::ShaderStageFlagBits::eFragment) { return shaderc_shader_kind::shaderc_fragment_shader; }

			return shaderc_shader_kind::shaderc_vertex_shader;
		}
	};

	class GraphicsPipeline : Destroyable
	{
	public:
		GraphicsPipeline(ref<Device> device) : device_(device) {}
		//INFO:Very expensive, might compile shader if not already compiled, lots of structs, ...
		GraphicsPipeline(ref<Device> device, vec_ref<Shader> shaders, vk::PrimitiveTopology topology, vk::PolygonMode polygonMode, vk::Extent2D extent, vk::Format imageFormat) :
			device_(device), shaders_(shaders), topology_(topology), polygonMode_(polygonMode), extent_(extent), imageFormat_(imageFormat)
		{
			//Shader stages
			std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
			for (auto s : shaders_)
			{
				vk::PipelineShaderStageCreateInfo createInfo = {};
				createInfo.stage = s.get().stage();
				createInfo.module = s.get().shader();//INFO:Expensive, might read file and compile
				createInfo.pName = "main";

				shaderStages.push_back(createInfo);
			}

			//Vertex input
			vk::PipelineVertexInputStateCreateInfo vertexInput = {};

			//Input assembly
			vk::PipelineInputAssemblyStateCreateInfo inputAssembly = {};
			inputAssembly.topology = topology_;

			//Rasterizer
			vk::PipelineRasterizationStateCreateInfo rasterization = {};
			rasterization.polygonMode = polygonMode_;
			rasterization.lineWidth = 1.0f;

			rasterization.cullMode = vk::CullModeFlagBits::eNone;
			rasterization.frontFace = vk::FrontFace::eClockwise;

			//Multisampling
			vk::PipelineMultisampleStateCreateInfo multisample = {};
			multisample.rasterizationSamples = vk::SampleCountFlagBits::e1;

			multisample.minSampleShading = 1.0f;

			//Color blend
			vk::PipelineColorBlendAttachmentState colorBlendAttachment = {};
			colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR |
												  vk::ColorComponentFlagBits::eG |
												  vk::ColorComponentFlagBits::eB |
												  vk::ColorComponentFlagBits::eA;

			vk::PipelineColorBlendStateCreateInfo colorBlend = {};
			colorBlend.logicOp = vk::LogicOp::eCopy;

			colorBlend.attachmentCount = 1;
			colorBlend.pAttachments = &colorBlendAttachment;

			//Depth

			vk::PipelineDepthStencilStateCreateInfo depth = {};
			depth.depthTestEnable = true; //Depth test
			depth.depthWriteEnable = true; //Depth write
			depth.depthCompareOp = vk::CompareOp::eLessOrEqual; //Draw if Z less or equal

			depth.minDepthBounds = 0.f;
			depth.maxDepthBounds = 1.f;

			//Scissor
			vk::Rect2D scissor = {};
			scissor.offset = vk::Offset2D{ 0, 0 };
			scissor.extent = extent_;

			//Viewport
			vk::Viewport viewport = {};
			viewport.width = static_cast<float>(extent_.width);
			viewport.height = static_cast<float>(extent_.height);
			
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			vk::PipelineViewportStateCreateInfo viewportState = {};
			viewportState.viewportCount = 1;
			viewportState.pViewports = &viewport;

			viewportState.scissorCount = 1;
			viewportState.pScissors = &scissor;


			//Pipeline layout
			vk::PipelineLayoutCreateInfo pipelineLayout = {};

			//INFO:Vertex buffer device address + Mesh matrix buffer device address + Mesh matrix index
			vk::PushConstantRange bdaPushConstants = {};
			bdaPushConstants.offset = 0;
			bdaPushConstants.size = 2*sizeof(vk::DeviceAddress)+1*sizeof(vk::DeviceSize);
			bdaPushConstants.stageFlags = vk::ShaderStageFlagBits::eVertex;

			pipelineLayout.pushConstantRangeCount = 1;
			pipelineLayout.pPushConstantRanges = &bdaPushConstants;
			VK_CHECK(device_.get().vk().createPipelineLayout(&pipelineLayout, nullptr, &pipelineLayout_));

			//Pipeline rendering
			vk::PipelineRenderingCreateInfo rendering = {};
			rendering.colorAttachmentCount = 1;
			rendering.pColorAttachmentFormats = &imageFormat;
			rendering.depthAttachmentFormat = vk::Format::eD32Sfloat;


			//Creating pipeline
			vk::GraphicsPipelineCreateInfo createInfo = {};
			createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
			createInfo.pStages = shaderStages.data();

			createInfo.pVertexInputState = &vertexInput;
			createInfo.pInputAssemblyState = &inputAssembly;

			createInfo.pViewportState = &viewportState;

			createInfo.pRasterizationState = &rasterization;

			createInfo.pMultisampleState = &multisample;

			createInfo.pColorBlendState = &colorBlend;

			createInfo.pDepthStencilState = &depth;

			createInfo.layout = pipelineLayout_;

			createInfo.pNext = &rendering;

			auto result = device_.get().vk().createGraphicsPipeline(nullptr, createInfo);
			VK_CHECK(result.result);

			pipeline_ = result.value;
		}

		//TODO:Implement move constructors
		GraphicsPipeline(GraphicsPipeline&& other) = delete;
		GraphicsPipeline& operator=(GraphicsPipeline&& other) noexcept
		{
			destroy();

			destroyed_ = other.destroyed_;
			other.destroyed_ = true;

			manual_ = other.manual_;
			other.manual_ = false;

			pipeline_ = other.pipeline_;
			other.pipeline_ = nullptr;

			pipelineLayout_ = other.pipelineLayout_;
			other.pipelineLayout_ = nullptr;

			//TODO:Device should be the same, or implement stuff in Device

			shaders_ = other.shaders_;
			other.shaders_ = vec_ref<Shader>();

			topology_ = other.topology_;
			polygonMode_ = other.polygonMode_;
			extent_ = other.extent_;
			imageFormat_ = other.imageFormat_;

			return *this;
		}

		//No copy constructors
		GraphicsPipeline(GraphicsPipeline& other) = delete;
		GraphicsPipeline& operator=(GraphicsPipeline& other) = delete;

		void destroy()
		{
			if (destroyed_) { return; }
			device_.get().vk().destroyPipelineLayout(pipelineLayout_);
			device_.get().vk().destroyPipeline(pipeline_);
			destroyed_ = true;
		}

		~GraphicsPipeline()
		{
			if (manual_) { return; }
			destroy();
		}

		vk::Pipeline vk() const  { return pipeline_; }
		vk::PipelineLayout layout() const  { return pipelineLayout_; }

	private:
		vk::Pipeline pipeline_{};
		vk::PipelineLayout pipelineLayout_{};

		ref<Device> device_;

		vec_ref<Shader> shaders_{};

		vk::PrimitiveTopology topology_{};
		vk::PolygonMode polygonMode_{};
		vk::Extent2D extent_{};
		vk::Format imageFormat_{};
	};

	class Allocator : Destroyable
	{
	public:
		Allocator(ref<Instance> instance, ref<Device> device) : instance_(instance), device_(device)
		{
			VmaAllocatorCreateInfo createInfo = {};

			createInfo.physicalDevice = device_.get().physicalDevice();
			createInfo.device = device_.get().vk();
			createInfo.instance = instance_.get().vk();
			createInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

			VK_CHECK(vk::Result(vmaCreateAllocator(&createInfo, &allocator_)));
		}

		//TODO:Implement move constructors
		Allocator(Allocator&& other) noexcept : allocator_(other.allocator_), instance_(other.instance_), device_(other.device_)
		{
			destroyed_ = other.destroyed_;
			other.destroyed_ = true;

			manual_ = other.manual_;
			other.manual_ = false;

			other.allocator_ = VmaAllocator(nullptr);
		}
		Allocator& operator=(Allocator&& other) noexcept
		{
			destroy();

			destroyed_ = other.destroyed_;
			other.destroyed_ = true;

			manual_ = other.manual_;
			other.manual_ = false;

			allocator_ = other.allocator_;
			other.allocator_ = VmaAllocator(nullptr);

			instance_ = other.instance_;

			device_ = other.device_;
		}

		//No copy constructors
		Allocator(Allocator& other) = delete;
		Allocator& operator=(Allocator& other) = delete;

		void destroy()
		{
			if (destroyed_) { return; }
			vmaDestroyAllocator(allocator_);
			destroyed_ = true;
		}

		~Allocator()
		{
			if (manual_) { return; }
			destroy();
		}

		VmaAllocator vma() const { return allocator_; }

	private:
		VmaAllocator allocator_{};

		ref<Instance> instance_;
		ref<Device> device_;
	};

	class Buffer : public Destroyable
	{
	public:
		Buffer(ref<Device> device, ref<Allocator> allocator, vk::Flags<vk::BufferUsageFlagBits> usage, vk::DeviceSize size, bool mappable = false) 
			: device_(device), allocator_(allocator), size_(size), mappable_(mappable)
		{
			vk::BufferCreateInfo createInfo = {};

			createInfo.size = size;
			createInfo.usage = usage | vk::BufferUsageFlagBits::eShaderDeviceAddress;
			

			VkBufferCreateInfo vkCreateInfo = static_cast<VkBufferCreateInfo>(createInfo);

			VmaAllocationCreateInfo allocInfo = {};
			allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
			if (mappable)
			{
				allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
			}

			VkBuffer buffer;

			VK_CHECK(vk::Result(vmaCreateBuffer(allocator_.get().vma(), &vkCreateInfo, &allocInfo, &buffer, &allocation_, nullptr)));

			buffer_ = vk::Buffer(buffer);

			//TODO:Look into VMA Persistently mapped memory
			if (mappable_)
			{
				vmaMapMemory(allocator_.get().vma(), allocation_, &mappedMemory);
			}
		}

		//TODO:Implement move constructors
		Buffer(Buffer&& other) noexcept : device_(other.device_), allocator_(other.allocator_), buffer_(other.buffer_),
			allocation_(other.allocation_), address_(other.address_)
		{
			destroyed_ = other.destroyed_;
			other.destroyed_ = true;

			manual_ = other.manual_;
			other.manual_ = false;

			other.buffer_ = vk::Buffer(nullptr);
			other.allocation_ = VmaAllocation(nullptr);
			other.address_ = 0;
		}
		Buffer& operator=(Buffer&& other) noexcept
		{
			destroy();

			destroyed_ = other.destroyed_;
			other.destroyed_ = true;

			manual_ = other.manual_;
			other.manual_ = false;

			device_ = other.device_;

			allocator_ = other.allocator_;

			buffer_ = other.buffer_;
			other.buffer_ = vk::Buffer(nullptr);

			allocation_ = other.allocation_;
			other.allocation_ = VmaAllocation(nullptr);

			address_ = other.address_;
			other.address_ = 0;
		}

		//No copy constructors
		Buffer(Buffer& other) = delete;
		Buffer& operator=(Buffer& other) = delete;


		void destroy()
		{
			if (destroyed_) { return; }
			if (mappable_) { vmaUnmapMemory(allocator_.get().vma(), allocation_); }
			vmaDestroyBuffer(allocator_.get().vma(), buffer_, allocation_);
			destroyed_ = true;
		}

		~Buffer()
		{
			if (manual_) { return; }
			destroy();
		}

		void upload(void *data, size_t size, uint32_t offset = 0)
		{
			if (!mappable_) { KILL("Trying to map to a buffer that is not mapapble"); }
			
			char* offsetDst = static_cast<char*>(mappedMemory) + offset;//We assume that sizeof(char) = 1

			memcpy(offsetDst, data, size);
		}

		vk::DeviceAddress address()
		{
			if (address_ != 0) { return address_; }

			vk::BufferDeviceAddressInfo addressInfo = {};
			addressInfo.buffer = buffer_;

			address_ = device_.get().vk().getBufferAddress(&addressInfo);
			return address_;
		}

		vk::DeviceSize size()
		{
			return size_;
		}

		vk::Buffer vk()
		{
			return buffer_;
		}

	protected:
		ref<Device> device_;
		ref<Allocator> allocator_;

		vk::Buffer buffer_;
		VmaAllocation allocation_;

		vk::DeviceSize size_ = 0;

		vk::DeviceAddress address_ = 0;

		bool mappable_;
		void* mappedMemory = nullptr;
		
	};

	class DepthImage : Destroyable
	{
	public:
		DepthImage(ref<Device> device, ref<Allocator> allocator, vk::Extent2D extent) : device_(device), allocator_(allocator)
		{
			vk::Extent3D depthImageExtent = {};
			depthImageExtent.width = extent.width;
			depthImageExtent.height = extent.height;
			depthImageExtent.depth = 1;

			vk::Format depthFormat = vk::Format::eD32Sfloat;

			vk::ImageCreateInfo createInfo = {};
			createInfo.imageType = vk::ImageType::e2D;

			createInfo.format = depthFormat;
			createInfo.extent = depthImageExtent;

			createInfo.mipLevels = 1;
			createInfo.arrayLayers = 1;
			createInfo.samples = vk::SampleCountFlagBits::e1;
			createInfo.tiling = vk::ImageTiling::eOptimal;
			createInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;

			VkImageCreateInfo vkCreateInfo = static_cast<VkImageCreateInfo>(createInfo);

			VmaAllocationCreateInfo allocationCreateInfo = {};
			allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO; //Use auto for depth-stencil https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/usage_patterns.html
			allocationCreateInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(vk::MemoryPropertyFlagBits::eDeviceLocal);//TODO:Not sure about that here

			VkImage vkImage;
			VK_CHECK(vk::Result(vmaCreateImage(allocator_.get().vma(), &vkCreateInfo, &allocationCreateInfo, &vkImage, &allocation_, nullptr)));
			image_ = vk::Image(vkImage);

			vk::ImageViewCreateInfo viewCreateInfo = {};
			viewCreateInfo.viewType = vk::ImageViewType::e2D;

			viewCreateInfo.image = image_;
			viewCreateInfo.format = depthFormat;

			viewCreateInfo.subresourceRange.baseMipLevel = 0;
			viewCreateInfo.subresourceRange.levelCount = 1;

			viewCreateInfo.subresourceRange.baseArrayLayer = 0;
			viewCreateInfo.subresourceRange.layerCount = 1;

			viewCreateInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

			VK_CHECK(device_.get().vk().createImageView(&viewCreateInfo, nullptr, &view_));
		}
		//TODO:Implement move constructors
		DepthImage(DepthImage&& other) noexcept : image_(other.image_), view_(other.view_), allocation_(other.allocation_),
			device_(other.device_), allocator_(other.allocator_)
		{
			destroyed_ = other.destroyed_;
			other.destroyed_ = true;

			manual_ = other.manual_;
			other.manual_ = false;

			other.image_ = vk::Image(nullptr);
			other.view_ = vk::ImageView(nullptr);
			other.allocation_ = VmaAllocation(nullptr);

		}
		DepthImage& operator=(DepthImage&& other) noexcept
		{
			destroy();

			destroyed_ = other.destroyed_;
			other.destroyed_ = true;

			manual_ = other.manual_;
			other.manual_ = false;

			image_ = other.image_;
			other.image_ = vk::Image(nullptr);

			view_ = other.view_;
			other.view_ = vk::ImageView(nullptr);

			allocation_ = other.allocation_;
			other.allocation_ = VmaAllocation(nullptr);

			device_ = other.device_;

			allocator_ = other.allocator_;
		}

		//No copy constructors
		DepthImage(DepthImage& other) = delete;
		DepthImage& operator=(DepthImage& other) = delete;

		void destroy()
		{
			if (destroyed_) { return; }
			device_.get().vk().destroyImageView(view_);
			vmaDestroyImage(allocator_.get().vma(), image_, allocation_);
			destroyed_ = true;
		}

		~DepthImage()
		{
			if (manual_) { return; }
			destroy();
		}

		vk::Image image() const { return image_; }
		vk::ImageView view() const { return view_; }

	private:
		vk::Image image_;
		vk::ImageView view_;
		VmaAllocation allocation_;

		ref<Device> device_;
		ref<Allocator> allocator_;
	};

	//Copyable
	struct Vertex
	{
		glm::vec4 position;
		glm::vec4 color;
	};

	//Non copyable movable
	class Mesh
	{
	public:
		Mesh(std::string name, std::vector<Vertex> vertices)
			: name_(name), vertices_(vertices)
		{
			
		}

		static Mesh triangleMesh()
		{
			Vertex bottomRight{ .position = glm::vec4(1.f, 1.f, 0.0f, 1.f), .color = glm::vec4(1.f, 0.f, 0.f, 1.f) };

			Vertex bottomLeft{ .position = glm::vec4(-1.f, 1.f, 0.0f,  1.f), .color = glm::vec4(0.f, 1.f, 0.f, 1.f) };

			Vertex top{ .position = glm::vec4(0.f, -1.f, 0.0f,  1.f), .color = glm::vec4(0.f, 0.f, 1.f, 1.f) };

			return Mesh("triangle", std::vector<Vertex>{bottomRight, bottomLeft, top});
		}

		static Mesh squareMesh()
		{
			Vertex bottomRight{ .position = glm::vec4(0.5f, 0.5f, 0.0f, 1.f), .color = glm::vec4(1.f, 0.f, 0.f, 1.f) };
			Vertex bottomLeft{ .position = glm::vec4(-0.5f, 0.5f, 0.0f,  1.f), .color = glm::vec4(0.f, 1.f, 0.f, 1.f) };
			Vertex topLeft{ .position = glm::vec4(-0.5f, -0.5f, 0.0f,  1.f), .color = glm::vec4(0.f, 0.f, 1.f, 1.f) };

			Vertex topRight{ .position = glm::vec4(0.5f, -0.5f, 0.0f,  1.f), .color = glm::vec4(1.f, 1.f, 1.f, 1.f) };

			return Mesh("square", std::vector<Vertex>{bottomRight, bottomLeft, topLeft,
				bottomRight, topLeft, topRight});
		}

		static Mesh heartMesh()
		{
			Vertex center{ .position = glm::vec4(0.0f, 0.0f, 0.0f, 1.f), .color = glm::vec4(1.f, 1.f, 1.f, 1.f) };

			Vertex la{ .position = glm::vec4(-0.5f, -0.5f, 0.0f,  1.f), .color = glm::vec4(1.f, 0.f, 0.f, 1.f) };
			Vertex lb{ .position = glm::vec4(-0.25f, -0.5f, 0.0f,  1.f), .color = glm::vec4(0.f, 0.f, 1.f, 1.f) };
			Vertex lc{ .position = glm::vec4(-0.5f, 0.0f, 0.0f, 1.f), .color = glm::vec4(1.f, 0.f, 0.f, 1.f) };

			Vertex down{ .position = glm::vec4(0.f, 1.f, 0.0f,  1.f), .color = glm::vec4(0.f, 0.f, 1.f, 1.f) };

			Vertex ra{ .position = glm::vec4(0.5f, -0.5f, 0.0f,  1.f), .color = glm::vec4(1.f, 0.f, 0.f, 1.f) };
			Vertex rb{ .position = glm::vec4(0.25f, -0.5f, 0.0f,  1.f), .color = glm::vec4(0.f, 0.f, 1.f, 1.f) };
			Vertex rc{ .position = glm::vec4(0.5f, 0.0f, 0.0f, 1.f), .color = glm::vec4(1.f, 0.f, 0.f, 1.f) };


			return Mesh("heart", std::vector<Vertex>{center, la, lb,//Left half
				center, lc, la,
				center, down, lc,
				center, ra, rb,//Right half
				center, rc, ra,
				center, down, rc});
		}

		static Mesh objMesh(std::string filename)
		{
			tinyobj::ObjReaderConfig readerConfig;

			tinyobj::ObjReader reader;

			if (!reader.ParseFromFile(filename, readerConfig))
			{
				if (!reader.Error().empty())//An error occured
				{
					std::string errorMessage = std::format("Killing process, error while parsing [{}]: {}", filename, reader.Error());
					KILL(errorMessage);
				}

				std::string errorMessage = std::format("Killing process, could not parse [{}]", filename);
				KILL(errorMessage);
			}

			if (!reader.Warning().empty())
			{
				std::string warningMessage = std::format("Warning while parsing [{}]: {}", filename, reader.Warning());
				std::cout << warningMessage << std::endl;
			}

			auto& attrib = reader.GetAttrib();
			auto& shapes = reader.GetShapes();

			std::vector<Vertex> vertices;

			for (size_t s = 0; s < shapes.size(); s++)//Looping over every shape in .obj
			{
				size_t indexOffset = 0;
				for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)//Looping over every face of current shape
				{
					size_t faceVertices = static_cast<size_t>(shapes[s].mesh.num_face_vertices[f]);

					for (size_t v = 0; v < faceVertices; v++)//Looping over every vertex in the face
					{
						Vertex vertex;

						tinyobj::index_t index = shapes[s].mesh.indices[indexOffset + v];

						tinyobj::real_t vx = attrib.vertices[3 * index.vertex_index + 0];
						tinyobj::real_t vy = attrib.vertices[3 * index.vertex_index + 1];
						tinyobj::real_t vz = attrib.vertices[3 * index.vertex_index + 2];

						vertex.position.x = vx;
						vertex.position.y = vy;
						vertex.position.z = vz;
						vertex.position.w = 1.f;

						vertex.color.x = 1.f;
						vertex.color.y = 0.f;
						vertex.color.z = 1.f;
						vertex.color.w = 1.f;

						if (index.normal_index >= 0) //Is there normal data
						{
							tinyobj::real_t nx = attrib.normals[3 * index.normal_index + 0];
							tinyobj::real_t ny = attrib.normals[3 * index.normal_index + 1];
							tinyobj::real_t nz = attrib.normals[3 * index.normal_index + 2];

							vertex.color.x = nx;
							vertex.color.y = ny;
							vertex.color.z = nz;
						}

						vertices.push_back(vertex);
					}

					indexOffset += faceVertices;
				}
			}

			return Mesh(filename, vertices);
		}

		std::string name()
		{
			return name_;
		}

		size_t vertexCount()
		{
			return vertices_.size();
		}

		vk::DeviceSize size()
		{
			return vertices_.size() * sizeof(Vertex);
		}

		void* data()
		{
			return vertices_.data();
		}
	private:
		std::string name_;
		std::vector<Vertex> vertices_;
	};
	
	//Small mappable buffer
	//Non copyable movable
	class StagingBuffer : public Buffer
	{
	public:
		StagingBuffer(ref<Device> device, ref<Allocator> allocator, vk::DeviceSize size)
			: Buffer(device, allocator, (vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc),
				size > 200'000'000 ? 200'000'000 : size, true), //Staging buffer cannot be bigger than 200MB
			  size_(size > 200'000'000 ? 200'000'000 : size)
		{}

		vk::DeviceSize size()
		{
			return size_;
		}
	private:
		vk::DeviceSize size_;
	};

	//ALEX:would be better to copy to staging in one go and then copying everything to the gpu in one go
	//instead of copying and waiting and copying .
	//Non copyable movable
	class LocalBuffer : public Buffer //TODO:Protected upload so end user cannot directly call VertexBuffer.upload() and mess things up
	{
	public:
		LocalBuffer(ref<Device> device, ref<Allocator> allocator, vk::DeviceSize localSize, vk::DeviceSize stagingSize = 10'000'000) :
			Buffer(device, allocator, (vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst), localSize),
			stagingBuffer_(device, allocator, stagingSize),
			transferPool_(device, device.get().queueIndex(QueueFamilyCapability::TRANSFER)),
			transferCommandBuffer_(transferPool_.allocate()),
			transferFence_(device),
			transferQueue_(device.get().queue(QueueFamilyCapability::TRANSFER, 0))
		{
			voids_[0] = localSize;//Whole buffer is free when first created
		}

		//Adds and uploads mesh to staging
		//TODO:Proper return, pair or return code
		void add(std::string name, void* data, size_t size)
		{
			//Find space in staging
			if (stagingVoidStart_ + size > stagingBuffer_.size()) //Not enough space
			{
				KILL(std::format("Not enough space in staging buffer (size = {} bytes) when trying to add following object: {} of size {} bytes", stagingBuffer_.size(), name, size));
			}

			//Upload to staging
			stagingBuffer_.upload(data, size, stagingVoidStart_);
			toBeUploaded_[name] = std::make_pair(stagingVoidStart_, size);

			stagingVoidStart_ += size;
		}

		void upload(bool overwriting = false)
		{
			//Wait for potential previous upload to have properly finished 
			device_.get().waitFence(transferFence_);
			device_.get().resetFence(transferFence_);

			std::vector<vk::BufferCopy2> copyRegions = {};
			//Looping over every to be uploaded mesh
			for (auto& [key, val] : toBeUploaded_)
			{
				//Find space in buffer
				bool elementPresent = elements_.find(key) != elements_.end();
				if (elementPresent && !overwriting) //Element already present + we don't overwrite
				{
					continue;
				}

				uint64_t selectedOffset = overwriting ? elements_[key].first : firstBufferVoid(val.second);
				if (selectedOffset == std::numeric_limits<uint64_t>::max())
				{
					KILL(std::format("Not enough space in local buffer for following mesh : {} of size {}", key, val.second));
				}

				//Adding copy region from staging to buffer
				vk::BufferCopy2 copyRegion = {};
				copyRegion.srcOffset = val.first;
				copyRegion.dstOffset = selectedOffset;
				copyRegion.size = val.second;

				copyRegions.push_back(copyRegion);

				//Updating meshes and voids if element is new 
				if (!elementPresent)
				{
					elements_[key] = std::make_pair(selectedOffset, val.second);

					vk::DeviceSize freeSpaceAtOffset = voids_[selectedOffset];
					voids_.erase(selectedOffset);
					voids_[selectedOffset + val.second] = freeSpaceAtOffset - val.second;
				}
			}

			toBeUploaded_.clear();

			//Upload to buffer
			device_.get().vk().resetCommandPool(transferPool_.vk());

			//Actual copy command
			transferCommandBuffer_.begin();

			vk::CopyBufferInfo2 bufferCopy = {};
			bufferCopy.srcBuffer = stagingBuffer_.vk();
			bufferCopy.dstBuffer = buffer_;
			bufferCopy.regionCount = copyRegions.size();

			bufferCopy.pRegions = copyRegions.data();

			transferCommandBuffer_.vk().copyBuffer2(&bufferCopy);

			//Barrier after write to ensure no two writes are being done concurrently + reads are executed after the whole write is done
			vk::MemoryBarrier2 barrier = {};
			barrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
			barrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;

			barrier.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands;
			barrier.dstAccessMask = vk::AccessFlagBits2::eMemoryRead;

			vk::DependencyInfo dependency = {};
			dependency.memoryBarrierCount = 1;
			dependency.pMemoryBarriers = &barrier;

			transferCommandBuffer_.vk().pipelineBarrier2(&dependency);

			transferCommandBuffer_.end();

			transferQueue_.submit(transferCommandBuffer_, transferFence_);
		}

		void remove(std::string name)
		{
			vk::DeviceSize elementOffset = elements_[name].first;
			vk::DeviceSize elementSize = elements_[name].second;

			//In case a void follows the one we're going to create
			//TODO:Do it for a void before this one
			voids_[elementOffset] = elementSize;
			if (voids_.find(elementOffset + elementSize) != voids_.end()) //Found
			{
				vk::DeviceSize extra = voids_[elementOffset + elementSize];

				voids_[elementOffset] = elementSize + extra;
			}
		}

		vk::DeviceAddress address()
		{
			return Buffer::address();
		}

	protected:
		//Elements located inside the device local buffer
		//["Monkey"] = (0, 1024) meaning that a mesh called monkey is located at offset 0 of the buffer and is of size 1024
		std::map<std::string, std::pair<vk::DeviceSize, vk::DeviceSize>> elements_{};

		//Free spaces, key being offset in buffer and value being size of free space starting at key
		std::map<vk::DeviceSize, vk::DeviceSize> voids_{};

		//Offset of the first free space in the stagingBuffer
		vk::DeviceSize stagingVoidStart_ = 0;

		//Meshes to be uploaded, awaiting transfer from staging to local
		std::map<std::string, std::pair<vk::DeviceSize, vk::DeviceSize>> toBeUploaded_{};

		StagingBuffer stagingBuffer_;

		CommandPool transferPool_;
		CommandBuffer transferCommandBuffer_;
		Fence transferFence_;
		Queue transferQueue_;

		vk::DeviceSize firstBufferVoid(vk::DeviceSize meshSize)
		{
			uint64_t selectedOffset = std::numeric_limits<uint64_t>::max();

			for (auto& [key, val] : voids_)
			{
				if (val > meshSize)//Enough space to store mesh at offset key
				{
					return key;
					break;
				}
			}

			return selectedOffset;
		}
	};
	
	//Big buffer holding lots of vertices in device_local memory
	class VertexBuffer : public LocalBuffer
	{
	public:
		VertexBuffer(ref<Device> device, ref<Allocator> allocator, vk::DeviceSize localSize, vk::DeviceSize stagingSize = 10'000'000) :
			LocalBuffer(device, allocator, localSize, stagingSize) {}

		//vertexMode = true -> (vertex offset, vertex count) will be returned
		//When set to false, (real offset, real count) will be returned (in bytes)
		std::pair<vk::DeviceSize, vk::DeviceSize> mesh(std::string name, bool vertexMode = true)
		{
			auto trueOffsetSize = elements_[name];
			return (vertexMode ? std::make_pair(trueOffsetSize.first / sizeof(Vertex), trueOffsetSize.second / sizeof(Vertex)) : trueOffsetSize);
		}
	private:

	};

	class MatrixBuffer : public LocalBuffer
	{
	public:
		MatrixBuffer(ref<Device> device, ref<Allocator> allocator, vk::DeviceSize localSize, vk::DeviceSize stagingSize = 10'000'000) :
			LocalBuffer(device, allocator, localSize, stagingSize) {}

		//vertexMode = true -> (vertex offset, vertex count) will be returned
		//When set to false, (real offset, real count) will be returned (in bytes)
		std::pair<vk::DeviceSize, vk::DeviceSize> matrix(std::string name, bool matrixMode = true)
		{
			auto trueOffsetSize = elements_[name];
			return (matrixMode ? std::make_pair(trueOffsetSize.first / sizeof(glm::mat4), trueOffsetSize.second / sizeof(glm::mat4)) : trueOffsetSize);
		}
	private:

	};

	//Copyable movable
	//TODO:Boolean indicating if offset and size are in bytes or vertex mode
	class BufferView
	{
	public:
		BufferView(ref<Buffer> buffer, vk::DeviceSize offset, vk::DeviceSize size) :
			buffer_(buffer), offset_(offset), size_(size) {}

		BufferView(ref<Buffer> buffer, std::pair<vk::DeviceSize, vk::DeviceSize> offsetSize) :
			buffer_(buffer), offset_(offsetSize.first), size_(offsetSize.second) {}

		vk::DeviceSize offset()
		{
			return offset_;
		}

		vk::DeviceSize size()
		{
			return size_;
		}
	private:
		ref<Buffer> buffer_;
		vk::DeviceSize offset_;
		vk::DeviceSize size_;
	};

	class MeshInstance
	{
	public:
		MeshInstance(std::string name, BufferView meshView, BufferView matrixView) :
			name_(name), meshView_(meshView), matrixView_(matrixView)
		{}

		std::string name()
		{
			return name_;
		}


		BufferView meshView()
		{
			return meshView_;
		}

		BufferView matrixView()
		{
			return matrixView_;
		}
	private:
		std::string name_;
		BufferView meshView_;
		BufferView matrixView_;
	};
}



namespace SOULKAN_TEST_NAMESPACE
{
	void error_test()
	{
		VK_CHECK(vk::Result::eTimeout);

		glfwCreateWindow(0, 0, "ok", 0, NULL);
		GLFW_CHECK();

	}

	void infos_test()
	{
		//TODO:Display infos about GPU
	}

	void move_semantics_test()
	{
		//TODO:Test move semantics of every class here
		SOULKAN_NAMESPACE::DeletionQueue dq;

		glfwInit();
		dq.push([]() { glfwTerminate(); });

		SOULKAN_NAMESPACE::Window w1(800, 600, "Hello");
		SOULKAN_NAMESPACE::Window w(std::move(w1));

		SOULKAN_NAMESPACE::Instance instance1(true);
		SOULKAN_NAMESPACE::Instance instance(std::move(instance1));


		uint32_t i = 0;
		while (!glfwWindowShouldClose(w.window()))
		{
			glfwPollEvents();
			w.rename(std::format("Hello ({})", i));
			i++;
		}

		dq.flush();
	}

	void triangle_test()
	{
		SOULKAN_NAMESPACE::DeletionQueue dq;

		glfwInit();
		dq.push([]() { glfwTerminate(); });

		uint32_t windowWidth = 1200;
		uint32_t windowHeight = 800;

		SOULKAN_NAMESPACE::Window window(windowWidth, windowHeight, "Hello");

		SOULKAN_NAMESPACE::Instance instance(true);

		vk::SurfaceKHR surface = instance.surface(window);

		vk::PhysicalDevice physicalDevice = instance.best();

		SOULKAN_NAMESPACE::Device device(physicalDevice, window, surface);

		SOULKAN_NAMESPACE::Allocator allocator(instance, device);

		SOULKAN_NAMESPACE::Swapchain swapchain(device);

		SOULKAN_NAMESPACE::DepthImage depthImage(device, allocator, swapchain.extent());

		SOULKAN_NAMESPACE::CommandPool graphicsCommandPool(device, device.queueIndex(SOULKAN_NAMESPACE::QueueFamilyCapability::GRAPHICS));

		SOULKAN_NAMESPACE::CommandBuffer commandBuffer = graphicsCommandPool.allocate();
		SOULKAN_NAMESPACE::Queue graphicsQueue = device.queue(SOULKAN_NAMESPACE::QueueFamilyCapability::GRAPHICS, 0);

		SOULKAN_NAMESPACE::Mesh mesh = SOULKAN_NAMESPACE::Mesh::objMesh("lost_empire.obj");
		SOULKAN_NAMESPACE::Mesh mesh2 = SOULKAN_NAMESPACE::Mesh::objMesh("moai.obj");
		
		//Mesh vertex buffer
		SOULKAN_NAMESPACE::VertexBuffer vertexBuffer(device, allocator, 15'625'000 * sizeof(SOULKAN_NAMESPACE::Vertex), 100'000'000);

		vertexBuffer.add(mesh.name(), mesh.data(), mesh.size());
		vertexBuffer.add(mesh2.name(), mesh2.data(), mesh2.size());

		vertexBuffer.upload();

		//Mesh matrix buffer
		SOULKAN_NAMESPACE::MatrixBuffer meshMatrixBuffer(device, allocator, 1'000 * sizeof(glm::mat4));
		
		glm::mat4 identityMat = glm::mat4(1.f);
		meshMatrixBuffer.add("identity", &identityMat, sizeof(glm::mat4));

		meshMatrixBuffer.add("rotatingSomewhere1", &identityMat, sizeof(glm::mat4));
		meshMatrixBuffer.add("rotatingSomewhere2", &identityMat, sizeof(glm::mat4));
		meshMatrixBuffer.add("rotatingSomewhere3", &identityMat, sizeof(glm::mat4));
		meshMatrixBuffer.add("rotatingSomewhere4", &identityMat, sizeof(glm::mat4));

		meshMatrixBuffer.upload();

		std::vector<SOULKAN_NAMESPACE::MeshInstance> meshInstances{};
		meshInstances.push_back(SOULKAN_NAMESPACE::MeshInstance("sponza1",
							    SOULKAN_NAMESPACE::BufferView(vertexBuffer, vertexBuffer.mesh("lost_empire.obj")),
							    SOULKAN_NAMESPACE::BufferView(meshMatrixBuffer, meshMatrixBuffer.matrix("identity"))));

		meshInstances.push_back(SOULKAN_NAMESPACE::MeshInstance("moai1",
							    SOULKAN_NAMESPACE::BufferView(vertexBuffer, vertexBuffer.mesh("moai.obj")),
							    SOULKAN_NAMESPACE::BufferView(meshMatrixBuffer, meshMatrixBuffer.matrix("rotatingSomewhere1"))));

		meshInstances.push_back(SOULKAN_NAMESPACE::MeshInstance("moai2",
							    SOULKAN_NAMESPACE::BufferView(vertexBuffer, vertexBuffer.mesh("moai.obj")),
							    SOULKAN_NAMESPACE::BufferView(meshMatrixBuffer, meshMatrixBuffer.matrix("rotatingSomewhere2"))));

		meshInstances.push_back(SOULKAN_NAMESPACE::MeshInstance("moai3",
							    SOULKAN_NAMESPACE::BufferView(vertexBuffer, vertexBuffer.mesh("moai.obj")),
							    SOULKAN_NAMESPACE::BufferView(meshMatrixBuffer, meshMatrixBuffer.matrix("rotatingSomewhere3"))));

		meshInstances.push_back(SOULKAN_NAMESPACE::MeshInstance("moai4",
							    SOULKAN_NAMESPACE::BufferView(vertexBuffer, vertexBuffer.mesh("moai.obj")),
							    SOULKAN_NAMESPACE::BufferView(meshMatrixBuffer, meshMatrixBuffer.matrix("rotatingSomewhere4"))));



		std::vector<vk::DeviceAddress> pushConstants{ vertexBuffer.address(), meshMatrixBuffer.address(), meshInstances[0].matrixView().offset()};

		SOULKAN_NAMESPACE::Fence renderFence(device);
		SOULKAN_NAMESPACE::Semaphore presentSemaphore(device);
		SOULKAN_NAMESPACE::Semaphore renderSemaphore(device);

		SOULKAN_NAMESPACE::Shader vertShader(device, "triangle.vert", vk::ShaderStageFlagBits::eVertex);

		SOULKAN_NAMESPACE::Shader fragShader(device, "triangle.frag", vk::ShaderStageFlagBits::eFragment);


		SOULKAN_NAMESPACE::vec_ref<SOULKAN_NAMESPACE::Shader> shaders{ vertShader, fragShader };

		SOULKAN_NAMESPACE::GraphicsPipeline solidPipelineTmp(device);
		SOULKAN_NAMESPACE::GraphicsPipeline solidPipeline(device, shaders, vk::PrimitiveTopology::eTriangleList, vk::PolygonMode::eFill, swapchain.extent(), swapchain.imageFormat());
		
		SOULKAN_NAMESPACE::GraphicsPipeline wireframePipelineTmp(device);
		SOULKAN_NAMESPACE::GraphicsPipeline wireframePipeline(device, shaders, vk::PrimitiveTopology::eTriangleList, vk::PolygonMode::eLine, swapchain.extent(), swapchain.imageFormat());

		vk::Pipeline boundPipeline = solidPipeline.vk();
		vk::PipelineLayout boundPipelineLayout = solidPipeline.layout();

		glm::vec3 cameraFront{ 0.f, 0.f, 1.f };
		glm::vec3 cameraUp{ 0.f, 1.f, 0.f };
		glm::vec3 cameraRight{ 1.f, 0.f, 0.f };

		glm::vec3 cameraPos{ 0.f,0.f,-2.f };
		float cameraFov = 70.f;
		float cameraMovementSpeed = 0.5f;
		float cameraSensitivity = 0.5f;

		float cameraYaw = -90.f;
		float cameraPitch = 0.f;


		glm::mat4 projection = glm::perspective(glm::radians(cameraFov), 1700.f / 900.f, 0.1f, 200.0f);
		projection[1][1] *= -1;


		float rotationSpeed = 0.3f;

		uint32_t i = 0;
		double lastInputTime = 0;
		double LastMouseInputTime = 0;
		std::atomic<bool> status(false);

		double lastMouseX = windowWidth / 2.f;
		double lastMouseY = windowHeight / 2.f;

		while (!glfwWindowShouldClose(window.window()))
		{
			//Checking if shader recompilation and pipeline rebuilding has been triggered and finished
			if (status)
			{
				std::cout << "Changing pipelines" << std::endl;
				device.vk().waitIdle(); //MAYB:Use vkQueueWaitIdle instead for better performance ?
				solidPipeline = std::move(solidPipelineTmp);
				wireframePipeline = std::move(wireframePipelineTmp);

				boundPipeline = solidPipeline.vk();
				boundPipelineLayout = solidPipeline.layout();

				status = false;
			}

			glfwPollEvents();
			window.rename(std::format("Hello ({})", i));

			if (lastMouseX != -1 && lastMouseY != -1 && glfwGetTime() > LastMouseInputTime + 0.01)
			{
				LastMouseInputTime = glfwGetTime();

				double currentMouseX = -1;
				double currentMouseY = -1;

				glfwGetCursorPos(window.window(), &currentMouseX, &currentMouseY);
				float xOffset = currentMouseX - lastMouseX;
				cameraYaw = std::fmod(cameraYaw + (xOffset * cameraSensitivity), 360.f); //Yaw can get big after adding lots of time, floating point imprecision can make movement janky, so we mod it out
				cameraPitch += (lastMouseY - currentMouseY) * cameraSensitivity;
				
				if (cameraPitch > 89.f) { cameraPitch = 89.f; }
				if (cameraPitch < -89.f) { cameraPitch = -89.f; }

				glm::vec3 newCamFront;
				newCamFront.x = cos(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
				newCamFront.y = sin(glm::radians(cameraPitch));
				newCamFront.z = sin(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
				cameraFront = glm::normalize(newCamFront);

				lastMouseX = currentMouseX;
				lastMouseY = currentMouseY;
			}

			//model rotation
			glm::mat4 model = glm::rotate(glm::mat4{ 1.0f }, glm::radians(i * rotationSpeed), glm::vec3(0, 1, 0));
			//glm::mat4 view = glm::translate(glm::mat4(1.f), camPos);
			glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

			//calculate final mesh matrix
			glm::mat4 meshMatrix = projection * view * glm::mat4{ 1.0f };
			glm::mat4 meshRotatingMatrix1 = projection * view * glm::translate(model, glm::vec3(3.0, 3.0, 0.0));
			glm::mat4 meshRotatingMatrix2 = projection * view * glm::translate(model, glm::vec3(-3.0, 3.0, 0.0));
			glm::mat4 meshRotatingMatrix3 = projection * view * glm::translate(model, glm::vec3(3.0, -3.0, 0.0));
			glm::mat4 meshRotatingMatrix4 = projection * view * glm::translate(model, glm::vec3(-3.0, -3.0, 0.0));

			meshMatrixBuffer.add("identity", &meshMatrix, sizeof(meshMatrix));
			meshMatrixBuffer.add("rotatingSomewhere1", &meshRotatingMatrix1, sizeof(meshRotatingMatrix1));
			meshMatrixBuffer.add("rotatingSomewhere2", &meshRotatingMatrix2, sizeof(meshRotatingMatrix2));
			meshMatrixBuffer.add("rotatingSomewhere3", &meshRotatingMatrix3, sizeof(meshRotatingMatrix3));
			meshMatrixBuffer.add("rotatingSomewhere4", &meshRotatingMatrix4, sizeof(meshRotatingMatrix4));

			meshMatrixBuffer.upload(true);


			//Shader recompilation and graphics pipeline rebuilding when pressing R
			int state = glfwGetKey(window.window(), GLFW_KEY_R);
			if (state == GLFW_PRESS && glfwGetTime() > lastInputTime + 1) //Pressed a key more than one second ago
			{
				lastInputTime = glfwGetTime();

				std::thread shaderCompilationThread([&]()
					{
						shaders[0].get().shader(true);//Vertex
						shaders[1].get().shader(true);//Fragment

						solidPipelineTmp = SOULKAN_NAMESPACE::GraphicsPipeline(device, shaders, vk::PrimitiveTopology::eTriangleList, vk::PolygonMode::eFill, swapchain.extent(), swapchain.imageFormat());

						wireframePipelineTmp = SOULKAN_NAMESPACE::GraphicsPipeline(device, shaders, vk::PrimitiveTopology::eTriangleList, vk::PolygonMode::eLine, swapchain.extent(), swapchain.imageFormat());

						status = true; //Signaling thread has finished
					});

				shaderCompilationThread.detach(); //Let it do its thing away from our render loop
			}


			//Switch to triangle pipeline when pressing t
			state = glfwGetKey(window.window(), GLFW_KEY_T);
			if (state == GLFW_PRESS && glfwGetTime() > lastInputTime + 1) //Pressed a key more than one second ago
			{
				lastInputTime = glfwGetTime();
				boundPipeline = solidPipeline.vk();
				boundPipelineLayout = solidPipeline.layout();
				std::cout << "Switched to triangle pipeline" << std::endl;
			}

			state = glfwGetMouseButton(window.window(), GLFW_MOUSE_BUTTON_RIGHT);
			if (state == GLFW_PRESS && glfwGetTime() > lastInputTime + 0.01)
			{
				glfwGetCursorPos(window.window(), &lastMouseX, &lastMouseY);
			}

			state = glfwGetMouseButton(window.window(), GLFW_MOUSE_BUTTON_RIGHT);
			if (state == GLFW_RELEASE)
			{
				lastMouseX = -1;
				lastMouseY = -1;
			}
			//Switch to wireframe pipeline when pressing w
			state = glfwGetKey(window.window(), GLFW_KEY_Z);
			if (state == GLFW_PRESS && glfwGetTime() > lastInputTime + 1)
			{
				lastInputTime = glfwGetTime();
				boundPipeline = wireframePipeline.vk();
				boundPipelineLayout = wireframePipeline.layout();
				std::cout << "Switched to wireframe pipeline" << std::endl;
			}

			state = glfwGetKey(window.window(), GLFW_KEY_A);
			if (state == GLFW_PRESS && glfwGetTime() > lastInputTime + 0.01) 
			{
				lastInputTime = glfwGetTime();

				cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraMovementSpeed;
			}

			state = glfwGetKey(window.window(), GLFW_KEY_D);
			if (state == GLFW_PRESS && glfwGetTime() > lastInputTime + 0.01)
			{
				lastInputTime = glfwGetTime();

				cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraMovementSpeed;
			}

			state = glfwGetKey(window.window(), GLFW_KEY_S);
			if (state == GLFW_PRESS && glfwGetTime() > lastInputTime + 0.01)
			{
				lastInputTime = glfwGetTime();

				cameraPos -= cameraFront * cameraMovementSpeed;
			}

			state = glfwGetKey(window.window(), GLFW_KEY_W);
			if (state == GLFW_PRESS && glfwGetTime() > lastInputTime + 0.01)
			{
				lastInputTime = glfwGetTime();

				cameraPos += cameraFront * cameraMovementSpeed;
			}

			state = glfwGetKey(window.window(), GLFW_KEY_SPACE);
			if (state == GLFW_PRESS && glfwGetTime() > lastInputTime + 0.01) 
			{
				lastInputTime = glfwGetTime();

				cameraPos += cameraUp * cameraMovementSpeed;
			}

			state = glfwGetKey(window.window(), GLFW_KEY_LEFT_SHIFT);
			if (state == GLFW_PRESS && glfwGetTime() > lastInputTime + 0.01)
			{
				lastInputTime = glfwGetTime();

				cameraPos -= cameraUp * cameraMovementSpeed;
			}

			//DRAWING
			device.waitFence(renderFence);
			device.resetFence(renderFence);

			uint32_t imageIndex = swapchain.nextImage(presentSemaphore);

			float flash = abs(sin(i / 120.f));
			vk::ClearColorValue clearColor = std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f};

			commandBuffer.begin();

			//INFO:Transitioning image from undefined to attachmentOptimal, this imageLayout is needed to begin rendering
			commandBuffer.imageLayoutTransition(vk::ImageLayout::eUndefined, vk::ImageLayout::eAttachmentOptimal, swapchain.images()[imageIndex],
												vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone,
												vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite); 

			commandBuffer.beginRendering(swapchain.imageViews()[imageIndex], depthImage.view(), swapchain.extent(), clearColor);

			commandBuffer.vk().bindPipeline(vk::PipelineBindPoint::eGraphics, boundPipeline);

			//MeshInstance drawing
			for (auto& meshInstance : meshInstances)
			{
				pushConstants[2] = meshInstance.matrixView().offset();
				commandBuffer.vk().pushConstants(boundPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, 2 * sizeof(vk::DeviceAddress) + 1 * sizeof(vk::DeviceSize), pushConstants.data());
				commandBuffer.vk().draw(meshInstance.meshView().size(), 1, 0, meshInstance.meshView().offset());
			}

			commandBuffer.endRendering();

			//INFO:Transitioning image layout to something presentable after rendering has finished
			commandBuffer.imageLayoutTransition(vk::ImageLayout::eAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR, swapchain.images()[imageIndex],
												vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
												vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone); 

			commandBuffer.end();

			graphicsQueue.submit(commandBuffer, presentSemaphore, renderSemaphore, renderFence);

			graphicsQueue.present(swapchain, renderSemaphore, imageIndex);

			i++;
		}

		device.waitFence(renderFence);

		//dq.flush();
	}
}
#endif
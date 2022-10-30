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
namespace SOULKAN_NAMESPACE
{

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
	/*---------------------GLFW---------------------*/
	class Window
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

		//TODO:Check if it should be kept
		~Window() { destroy(); }

		void rename(std::string newTitle) 
		{ 
			if (window_ == nullptr) { return; }
			glfwSetWindowTitle(window_, newTitle.c_str()); 
			title_ = newTitle; 
		}

		void destroy() { if (window_ != nullptr) { glfwDestroyWindow(window_); window_ = nullptr; /*TODO:Check if setting to nullptr is redundant*/ } }

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

	class Instance
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
		}

		//No copy constructor/assignment
		Instance(const Instance&) = delete;
		Instance& operator=(const Instance&) = delete;

		//INFO/TODO:Defining a destructor results in "exited with code -1073741819"
		//~Instance()
		//{
		//	destroy();
		//}


		void destroy()
		{
			auto dynamicLoader = vk::DispatchLoaderDynamic(instance_, vkGetInstanceProcAddr);
			instance_.destroyDebugUtilsMessengerEXT(debugMessenger_, nullptr, dynamicLoader);
			instance_.destroy();
		}

		//Not cached since instance can vary
		vk::SurfaceKHR surface(Window& window)
		{
			VkSurfaceKHR tmp;
			VK_CHECK(vk::Result(glfwCreateWindowSurface(instance_, window.window(), nullptr, &tmp)));
			GLFW_CHECK(/*Checking if window creation was succesfull, TODO:Not sure if necessary after VK_CHECK*/);

			if (tmp == VK_NULL_HANDLE) { KILL("An error occured in surface creation, killing process"); } //TODO:Not sure if necessary after VK_CHECK

			return vk::SurfaceKHR(tmp);
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

	//QUEUE
	class Queue
	{
	public:
		Queue(Device& device, vk::Queue queue, QueueFamilyCapability family, uint32_t index) :
			device_(device), queue_(queue), family_(family), index_(index) {}

		//TODO:Implement move constructors
		Queue(Queue&& other) = delete;
		Queue& operator=(Queue&& other) = delete;

		//No copy constructors
		Queue(Queue& other) = delete;
		Queue& operator=(Queue& other) = delete;

		void submit(vk::Semaphore waitSemaphore,
			vk::Semaphore signalSemaphore, CommandBuffer& commandBuffer,
			vk::Fence fence);//Defined after CommandBuffer definition

		void present(Swapchain& swapchain, vk::Semaphore waitSemaphore, uint32_t imageIndex);//Defined after swapchain definition (alongside submit)

		uint32_t index() { return index_; }
	private:
		Device& device_;
		vk::Queue queue_;
		QueueFamilyCapability family_;
		uint32_t index_;
	};

	//DEVICE
	class Device
	{
	public:
		Device(vk::PhysicalDevice physicalDevice, Window& window, vk::SurfaceKHR surface) :
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

			vk::PhysicalDeviceVulkan12Features features12 = {};
			features12.bufferDeviceAddress = true;
			features12.bufferDeviceAddressCaptureReplay = true;

			vk::PhysicalDeviceVulkan13Features features13 = {};
			features13.dynamicRendering = true;
			features13.synchronization2 = true;


			vk::PhysicalDeviceFeatures baseFeatures = {};
			baseFeatures.fillModeNonSolid = true; //INFO:Allows wireframe
			//baseFeatures.wideLines = true; //INFO:Allows lineWidth > 1.f

			deviceCreateInfo.pNext = &features;
			features.features = baseFeatures;//INFO:if a pNext chain is used (like here), do not use deviceCreateInfo.enabledFeatures = &enabledFeatures, use this instead
			features.pNext = &features11;
			features11.pNext = &features12;
			features12.pNext = &features13;

			//Building
			VK_CHECK(physicalDevice_.createDevice(&deviceCreateInfo, nullptr, &device_));
		}

		//TODO:Implement move constructors
		Device(Device&& other) = delete;
		Device& operator=(Device&& other) = delete;

		//No copy constructors
		Device(Device& other) = delete;
		Device& operator=(Device& other) = delete;

		void destroy()
		{
			device_.destroy();
		}

		//TODO:Implement generic destroy, calling getProcAddr to get correct destroyFunction according to type of parameter
		//Fence
		vk::Fence createFence()
		{
			vk::FenceCreateInfo createInfo = {};
			createInfo.flags = vk::FenceCreateFlagBits::eSignaled; //INFO:Fence is created as signaled so that the first call to waitForFences() returns instantly

			vk::Fence fence;
			VK_CHECK(device_.createFence(&createInfo, nullptr, &fence));

			return fence;
		}

		void waitFence(vk::Fence fence)
		{
			VK_CHECK(device_.waitForFences(1, &fence, true, 40'000'000));//TODO:Temporary hacky fix to avoid timeout when loading big .obj
		}

		void resetFence(vk::Fence fence)
		{
			VK_CHECK(device_.resetFences(1, &fence));
		}

		//Semaphore
		vk::Semaphore createSemaphore()
		{
			vk::SemaphoreCreateInfo createInfo = {};

			vk::Semaphore semaphore;
			VK_CHECK(device_.createSemaphore(&createInfo, nullptr, &semaphore));
			
			return semaphore;
		}


		vk::Extent2D extent()
		{
			auto surfaceCapabilities = physicalDevice_.getSurfaceCapabilitiesKHR(surface_);

			int width, height = 0;
			glfwGetFramebufferSize(window_.window(), &width, &height);

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
		Window& window_; //TODO:Unsure about this
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

	//SWAPCHAIN
	class Swapchain
	{
	public:

		Swapchain(Device& device) : device_(device)
		{
			auto surface = device_.surface();
			auto surfaceCapabilities = device_.physicalDevice().getSurfaceCapabilitiesKHR(surface);

			//Image Count
			uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
			if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount)
			{
				imageCount = surfaceCapabilities.maxImageCount;
			}

			//Extent
			extent_ = device_.extent();

			//Concurrency
			auto sharingMode = vk::SharingMode::eConcurrent; //TODO:Defaulting to concurrent, read up on it later on

			//SurfaceFormat
			surfaceFormat_ = device_.surfaceFormat();

			//PresentMode
			presentMode_ = device_.presentMode();

			//Queues
			auto queues = device_.queueConcentrate();

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

			VK_CHECK(device_.vk().createSwapchainKHR(&createInfo, nullptr, &swapchain_));

		}

		//TODO:Handle move constructors/assignements
		Swapchain(Swapchain&& swapchain) = delete;
		Swapchain& operator=(Swapchain& other) = delete;

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
			VK_CHECK(device_.vk().getSwapchainImagesKHR(swapchain_, &imageCount, nullptr));
			images_.resize(imageCount);

			VK_CHECK(device_.vk().getSwapchainImagesKHR(swapchain_, &imageCount, images_.data()));

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
				VK_CHECK(device_.vk().createImageView(&imageViewCreateInfo, nullptr, &imageView));

				imageViews_.push_back(imageView);
			}

			return imageViews_;
		}

		uint32_t nextImage(vk::Semaphore signalSemaphore)
		{
			uint32_t imageIndex;
			VK_CHECK(device_.vk().acquireNextImageKHR(swapchain_, 1'000'000, signalSemaphore, nullptr, &imageIndex));

			return imageIndex;
		}

		void destroy()
		{
			device_.vk().destroySwapchainKHR(swapchain_);

			for (auto& i : imageViews_)
			{
				device_.vk().destroyImageView(i);
			}
		}

		vk::SwapchainKHR vk() { return swapchain_; }
		vk::Extent2D extent() { return extent_; }
		vk::SurfaceFormatKHR surfaceFormat() { return surfaceFormat_; }
		vk::Format imageFormat() { return surfaceFormat_.format; }
		vk::PresentModeKHR presentMode() { return presentMode_; }
	private:
		Device& device_;
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
	class CommandBuffer
	{
	public:
		CommandBuffer(CommandPool& commandPool, vk::CommandBuffer commandBuffer) : commandPool_(commandPool), commandBuffer_(commandBuffer) {}

		//TODO:Implement move constructors
		/*CommandBuffer(CommandBuffer&& other) = delete;
		CommandBuffer& operator=(CommandBuffer&& other) = delete;*/

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

		bool graphics();//Defined after CommandPool definition

		vk::CommandBuffer vk() { return commandBuffer_; }

		const CommandPool& commandPool_;

	private:
		vk::CommandBuffer commandBuffer_;
	};

	//COMMAND POOL
	class CommandPool
	{
	public:
		//TODO:Change second parameter to QueueFamilyCapability instead of index, no need to expose that kind of complexity to the caller
		CommandPool(Device& device, uint32_t queueFamilyIndex) : device_(device), queueFamilyIndex_(queueFamilyIndex)
		{
			vk::CommandPoolCreateInfo createInfo = {};
			
			createInfo.queueFamilyIndex = queueFamilyIndex;
			createInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer; // Allows command buffers to be reset individually
			
			createInfo.pNext = nullptr;

			VK_CHECK(device_.vk().createCommandPool(&createInfo, nullptr, &pool_));
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

			VK_CHECK(device_.vk().allocateCommandBuffers(&allocateInfo, &vkBuffer)); //

			return CommandBuffer(*this, vkBuffer);
		}



		//TODO:Implement move constructors
		CommandPool(CommandPool&& other) = delete;
		CommandPool& operator=(CommandPool&& other) = delete;

		//No copy constructors
		CommandPool(CommandPool& other) = delete;
		CommandPool& operator=(CommandPool& other) = delete;

		void destroy()
		{
			device_.vk().destroyCommandPool(pool_);
		}

		vk::CommandPool vk() const { return pool_; }
		uint32_t index() const { return queueFamilyIndex_; }

		Device& device() const { return device_; }

	private:
		Device& device_;
		vk::CommandPool pool_;
		uint32_t queueFamilyIndex_;

	};

	//COMMAND BUFFER
	//Returns true if command buffer was allocated from a graphics capable command pool
	bool CommandBuffer::graphics()
	{
		return (commandPool_.index() == commandPool_.device().queueFamilies()[INDEX(QueueFamilyCapability::GENERAL)]) ||
			(commandPool_.index() == commandPool_.device().queueFamilies()[INDEX(QueueFamilyCapability::GRAPHICS)]);
	}

	//QUEUE
	void Queue::submit(vk::Semaphore waitSemaphore,
		vk::Semaphore signalSemaphore, CommandBuffer& commandBuffer,
		vk::Fence signalFence)
	{
		vk::SubmitInfo submitInfo = {};

		vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

		submitInfo.pWaitDstStageMask = &waitStage;

		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &waitSemaphore;

		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &signalSemaphore;

		auto vkCommandBuffer = commandBuffer.vk();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &vkCommandBuffer;

		//signalFence 
		VK_CHECK(queue_.submit(1, &submitInfo, signalFence));
	}

	void Queue::present(Swapchain& swapchain, vk::Semaphore waitSemaphore, uint32_t imageIndex)
	{
		vk::PresentInfoKHR presentInfo = {};

		auto vkSwapchain = swapchain.vk();

		presentInfo.pSwapchains = &vkSwapchain;
		presentInfo.swapchainCount = 1;

		presentInfo.pWaitSemaphores = &waitSemaphore;
		presentInfo.waitSemaphoreCount = 1;

		presentInfo.pImageIndices = &imageIndex;

		VK_CHECK(queue_.presentKHR(&presentInfo));
	}

	class Shader
	{
	public:
		Shader(Device& device, std::string filename, vk::ShaderStageFlagBits shaderStage) : device_(device), filename_(filename), stage_(shaderStage) {}

		//TODO:Implement move constructors
		Shader(Shader&& other) = delete;
		Shader& operator=(Shader&& other) = delete;

		//No copy constructors
		Shader(Shader& other) = delete;
		Shader& operator=(Shader& other) = delete;

		void destroy()
		{
			device_.vk().destroyShaderModule(module_);
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

			VK_CHECK(device_.vk().createShaderModule(&createInfo, nullptr, &module_));
			compiled = true;

			return module_;
		}

		vk::ShaderStageFlagBits stage()
		{
			return stage_;
		}
	private:
		Device& device_;
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

	class GraphicsPipeline
	{
	public:
		GraphicsPipeline(Device &device) : device_(device) {}
		//INFO:Very expensive, might compile shader if not already compiled, lots of structs, ...
		GraphicsPipeline(Device& device, vec_ref<Shader> shaders, vk::PrimitiveTopology topology, vk::PolygonMode polygonMode, vk::Extent2D extent, vk::Format imageFormat) :
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

			//INFO:Vertex buffer device address + Mesh matrix buffer device address
			vk::PushConstantRange bdaPushConstants = {};
			bdaPushConstants.offset = 0;
			bdaPushConstants.size = 2*sizeof(vk::DeviceAddress);
			bdaPushConstants.stageFlags = vk::ShaderStageFlagBits::eVertex;

			pipelineLayout.pushConstantRangeCount = 1;
			pipelineLayout.pPushConstantRanges = &bdaPushConstants;
			VK_CHECK(device_.vk().createPipelineLayout(&pipelineLayout, nullptr, &pipelineLayout_));

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

			auto result = device_.vk().createGraphicsPipeline(nullptr, createInfo);
			VK_CHECK(result.result);

			pipeline_ = result.value;
		}

		//TODO:Implement move constructors
		GraphicsPipeline(GraphicsPipeline&& other) = delete;
		GraphicsPipeline& operator=(GraphicsPipeline&& other) noexcept
		{
			destroy();

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
			device_.vk().destroyPipelineLayout(pipelineLayout_);
			device_.vk().destroyPipeline(pipeline_);
		}

		vk::Pipeline vk() { return pipeline_; }
		vk::PipelineLayout layout() { return pipelineLayout_; }

	private:
		vk::Pipeline pipeline_{};
		vk::PipelineLayout pipelineLayout_{};

		Device& device_;

		vec_ref<Shader> shaders_{};

		vk::PrimitiveTopology topology_{};
		vk::PolygonMode polygonMode_{};
		vk::Extent2D extent_{};
		vk::Format imageFormat_{};
	};

	class Allocator
	{
	public:
		Allocator(Instance& instance, Device& device) : instance_(instance), device_(device)
		{
			VmaAllocatorCreateInfo createInfo = {};

			createInfo.physicalDevice = device_.physicalDevice();
			createInfo.device = device_.vk();
			createInfo.instance = instance_.vk();
			createInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

			VK_CHECK(vk::Result(vmaCreateAllocator(&createInfo, &allocator_)));
		}

		//TODO:Implement move constructors
		Allocator(Allocator&& other) = delete;
		Allocator& operator=(Allocator&& other) = delete;

		//No copy constructors
		Allocator(Allocator& other) = delete;
		Allocator& operator=(Allocator& other) = delete;

		void destroy()
		{
			vmaDestroyAllocator(allocator_);
		}

		VmaAllocator vma() const { return allocator_; }

	private:
		VmaAllocator allocator_{};

		Instance& instance_;
		Device& device_;
	};

	class Buffer
	{
	public:
		Buffer(Device &device, Allocator& allocator, vk::BufferUsageFlagBits usage, vk::DeviceSize size) : device_(device), allocator_(allocator)
		{
			vk::BufferCreateInfo createInfo = {};

			createInfo.size = static_cast<vk::DeviceSize>(size);
			createInfo.usage = usage | vk::BufferUsageFlagBits::eShaderDeviceAddress;

			VkBufferCreateInfo vkCreateInfo = static_cast<VkBufferCreateInfo>(createInfo);

			VmaAllocationCreateInfo allocInfo = {};
			allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
			allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

			VkBuffer buffer;

			VK_CHECK(vk::Result(vmaCreateBuffer(allocator_.vma(), &vkCreateInfo, &allocInfo, &buffer, &allocation_, nullptr)));

			buffer_ = vk::Buffer(buffer);
		}

		//TODO:Implement move constructors
		Buffer(Buffer&& other) = delete;
		Buffer& operator=(Buffer&& other) = delete;

		//No copy constructors
		Buffer(Buffer& other) = delete;
		Buffer& operator=(Buffer& other) = delete;


		void destroy()
		{
			vmaDestroyBuffer(allocator_.vma(), buffer_, allocation_);
		}

		void upload(void *data, size_t size)
		{
			void* dst;
			vmaMapMemory(allocator_.vma(), allocation_, &dst);

			memcpy(dst, data, size);

			vmaUnmapMemory(allocator_.vma(), allocation_);
		}

		vk::DeviceAddress address()
		{
			if (address_ != 0) { return address_; }

			vk::BufferDeviceAddressInfo addressInfo = {};
			addressInfo.buffer = buffer_;

			address_ = device_.vk().getBufferAddress(&addressInfo);
			return address_;
		}

	private:
		Device& device_;
		Allocator& allocator_;

		vk::Buffer buffer_;
		VmaAllocation allocation_;

		vk::DeviceAddress address_ = 0;
	};

	class DepthImage
	{
	public:
		DepthImage(Device &device, Allocator &allocator, vk::Extent2D extent) : device_(device), allocator_(allocator)
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
			VK_CHECK(vk::Result(vmaCreateImage(allocator_.vma(), &vkCreateInfo, &allocationCreateInfo, &vkImage, &allocation_, nullptr)));
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

			VK_CHECK(device_.vk().createImageView(&viewCreateInfo, nullptr, &view_));
		}
		//TODO:Implement move constructors
		DepthImage(DepthImage&& other) = delete;
		DepthImage& operator=(DepthImage&& other) = delete;

		//No copy constructors
		DepthImage(DepthImage& other) = delete;
		DepthImage& operator=(DepthImage& other) = delete;

		void destroy()
		{
			device_.vk().destroyImageView(view_);
			vmaDestroyImage(allocator_.vma(), image_, allocation_);
		}

		vk::Image image() const { return image_; }
		vk::ImageView view() const { return view_; }

	private:
		vk::Image image_;
		vk::ImageView view_;
		VmaAllocation allocation_;

		Device& device_;
		Allocator& allocator_;
	};

	struct Vertex
	{
		glm::vec4 position;
		glm::vec4 color;

		static std::vector<Vertex> triangleMesh()
		{
			Vertex bottomRight{ .position = glm::vec4(1.f, 1.f, 0.0f, 1.f), .color = glm::vec4(1.f, 0.f, 0.f, 1.f)};

			Vertex bottomLeft{ .position = glm::vec4(-1.f, 1.f, 0.0f,  1.f), .color = glm::vec4(0.f, 1.f, 0.f, 1.f)};

			Vertex top{ .position = glm::vec4(0.f, -1.f, 0.0f,  1.f), .color = glm::vec4(0.f, 0.f, 1.f, 1.f)};

			return std::vector<Vertex>{bottomRight, bottomLeft, top};
		}

		static std::vector<Vertex> squareMesh()
		{
			Vertex bottomRight{ .position = glm::vec4(0.5f, 0.5f, 0.0f, 1.f), .color = glm::vec4(1.f, 0.f, 0.f, 1.f) };
			Vertex bottomLeft{ .position = glm::vec4(-0.5f, 0.5f, 0.0f,  1.f), .color = glm::vec4(0.f, 1.f, 0.f, 1.f) };
			Vertex topLeft{ .position = glm::vec4(-0.5f, -0.5f, 0.0f,  1.f), .color = glm::vec4(0.f, 0.f, 1.f, 1.f)};

			Vertex topRight{ .position = glm::vec4(0.5f, -0.5f, 0.0f,  1.f), .color = glm::vec4(1.f, 1.f, 1.f, 1.f) };

			return std::vector<Vertex>{bottomRight, bottomLeft, topLeft,
									   bottomRight, topLeft, topRight};
		}

		static std::vector<Vertex> heartMesh()
		{
			Vertex center{ .position = glm::vec4(0.0f, 0.0f, 0.0f, 1.f), .color = glm::vec4(1.f, 1.f, 1.f, 1.f) };

			Vertex la{ .position = glm::vec4(-0.5f, -0.5f, 0.0f,  1.f), .color = glm::vec4(1.f, 0.f, 0.f, 1.f) };
			Vertex lb{ .position = glm::vec4(-0.25f, -0.5f, 0.0f,  1.f), .color = glm::vec4(0.f, 0.f, 1.f, 1.f) };
			Vertex lc{ .position = glm::vec4(-0.5f, 0.0f, 0.0f, 1.f), .color = glm::vec4(1.f, 0.f, 0.f, 1.f) };
			
			Vertex down{ .position = glm::vec4(0.f, 1.f, 0.0f,  1.f), .color = glm::vec4(0.f, 0.f, 1.f, 1.f) };

			Vertex ra{ .position = glm::vec4(0.5f, -0.5f, 0.0f,  1.f), .color = glm::vec4(1.f, 0.f, 0.f, 1.f) };
			Vertex rb{ .position = glm::vec4(0.25f, -0.5f, 0.0f,  1.f), .color = glm::vec4(0.f, 0.f, 1.f, 1.f) };
			Vertex rc{ .position = glm::vec4(0.5f, 0.0f, 0.0f, 1.f), .color = glm::vec4(1.f, 0.f, 0.f, 1.f) };


			return std::vector<Vertex>{center, la, lb,//Left half
									   center, lc, la,
									   center, down, lc,
									   center, ra, rb,//Right half
									   center, rc, ra,
								       center, down, rc};
		}

		static std::vector<Vertex> objMesh(std::string filename)
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

			return vertices;
		}
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
		SOULKAN_NAMESPACE::DeletionQueue dq;

		glfwInit();
		dq.push([]() { glfwTerminate(); });

		SOULKAN_NAMESPACE::Instance instance(true);
		dq.push([&]() { instance.destroy(); });

		//Instance extensions
		auto supportedExtensions = instance.supportedExtensions();
		std::cout << std::format("--Supported instance extensions ({}):", supportedExtensions.size()) << std::endl;
		for (const auto& ext : supportedExtensions)
		{
			std::cout << "__" << ext << std::endl;
		}
		std::cout << std::endl;

		//Physical devices
		auto availables = instance.availables();
		std::cout << std::format("--Available physical devices ({}):", availables.size()) << std::endl;
		for (const auto& a : availables)
		{
			std::cout << "__" << a.getProperties().deviceName << std::endl;
		}

		auto suitables = instance.suitables();
		std::cout << std::format("--Suitable physical devices ({}):", suitables.size()) << std::endl;
		for (const auto& s : suitables)
		{
			std::cout << "__" << s.getProperties().deviceName << std::endl;
		}

		auto best = instance.best();
		std::cout << "--Best physical devices: " << std::endl;

		std::cout << best.getProperties().deviceName << ":" << VK_API_VERSION_FULL(best.getProperties().apiVersion) << std::endl;

		/*SOULKAN_NAMESPACE::Device dev(best, );

		auto supportedDeviceExtensions = dev.supportedExtensions();
		std::cout << std::format("--Supported device extensions ({}):", supportedDeviceExtensions.size()) << std::endl;

		for (const auto& e : supportedDeviceExtensions)
		{
			std::cout << "__" << e << std::endl;
		}
		std::cout << std::endl;

		std::cout << "VK_KHR_dynamic_rendering is " << (dev.isSupported("VK_KHR_dynamic_rendering") ? "supported" : "not supported") << std::endl;*/

		dq.flush();
	}

	void move_semantics_test()
	{
		SOULKAN_NAMESPACE::DeletionQueue dq;

		glfwInit();
		dq.push([]() { glfwTerminate(); });

		SOULKAN_NAMESPACE::Window w1(800, 600, "Salut maeba");
		SOULKAN_NAMESPACE::Window w(std::move(w1));
		dq.push([&]() { w.destroy(); /*Not necessary since glfwTerminate() destroys every remaining window*/});

		SOULKAN_NAMESPACE::Instance instance1(true);
		SOULKAN_NAMESPACE::Instance instance(std::move(instance1));
		dq.push([&]() { instance.destroy(); });


		uint32_t i = 0;
		while (!glfwWindowShouldClose(w.window()))
		{
			glfwPollEvents();
			w.rename(std::format("Salut maeba ({})", i));
			i++;
		}

		dq.flush();
	}

	void triangle_test()
	{
		SOULKAN_NAMESPACE::DeletionQueue dq;

		glfwInit();
		dq.push([]() { glfwTerminate(); });

		SOULKAN_NAMESPACE::Window window(800, 600, "Salut maeba");
		dq.push([&]() { window.destroy(); /*Not necessary since glfwTerminate() destroys every remaining window*/});

		SOULKAN_NAMESPACE::Instance instance(true);
		dq.push([&]() { instance.destroy(); });

		vk::SurfaceKHR surface = instance.surface(window);
		dq.push([&]() { instance.vk().destroySurfaceKHR(surface); });

		vk::PhysicalDevice physicalDevice = instance.best();

		SOULKAN_NAMESPACE::Device device(physicalDevice, window, surface);
		dq.push([&]() { device.destroy(); });

		SOULKAN_NAMESPACE::Allocator allocator(instance, device);
		dq.push([&]() { allocator.destroy(); });

		SOULKAN_NAMESPACE::Swapchain swapchain(device);
		dq.push([&]() { swapchain.destroy(); });

		SOULKAN_NAMESPACE::DepthImage depthImage(device, allocator, swapchain.extent());
		dq.push([&]() { depthImage.destroy(); });

		SOULKAN_NAMESPACE::CommandPool graphicsCommandPool(device, device.queueIndex(SOULKAN_NAMESPACE::QueueFamilyCapability::GRAPHICS));
		dq.push([&]() { graphicsCommandPool.destroy(); });

		SOULKAN_NAMESPACE::CommandBuffer commandBuffer = graphicsCommandPool.allocate();
		SOULKAN_NAMESPACE::Queue graphicsQueue = device.queue(SOULKAN_NAMESPACE::QueueFamilyCapability::GRAPHICS, 0);

		std::vector<SOULKAN_NAMESPACE::Vertex> triangleMesh = SOULKAN_NAMESPACE::Vertex::triangleMesh();
		std::vector<SOULKAN_NAMESPACE::Vertex> monkeyMesh = SOULKAN_NAMESPACE::Vertex::objMesh("monkey.obj");

		//TODO:Try to use buffer to store mvp matrix (storage buffer or uniform buffer)

		SOULKAN_NAMESPACE::Buffer monkeyMeshBuffer(device, allocator, vk::BufferUsageFlagBits::eVertexBuffer, monkeyMesh.size()*sizeof(SOULKAN_NAMESPACE::Vertex));
		dq.push([&]() { monkeyMeshBuffer.destroy(); });

		monkeyMeshBuffer.upload(monkeyMesh.data(), monkeyMesh.size() * sizeof(SOULKAN_NAMESPACE::Vertex));

		SOULKAN_NAMESPACE::Buffer triangleMeshBuffer(device, allocator, vk::BufferUsageFlagBits::eVertexBuffer, triangleMesh.size() * sizeof(SOULKAN_NAMESPACE::Vertex));
		dq.push([&]() { triangleMeshBuffer.destroy(); });

		monkeyMeshBuffer.upload(triangleMesh.data(), triangleMesh.size() * sizeof(SOULKAN_NAMESPACE::Vertex));

		SOULKAN_NAMESPACE::Buffer meshMatrixBuffer(device, allocator, vk::BufferUsageFlagBits::eUniformBuffer, sizeof(glm::mat4));
		dq.push([&]() { meshMatrixBuffer.destroy(); });
		
		glm::mat4 identityMat = glm::mat4(1.f);
		meshMatrixBuffer.upload(&identityMat, sizeof(identityMat));
		vk::DeviceAddress meshMatrixBufferAddress = meshMatrixBuffer.address();

		std::vector<vk::DeviceAddress> bufferAddresses1{ monkeyMeshBuffer.address(), meshMatrixBuffer.address() };
		std::vector<vk::DeviceAddress> bufferAddresses2{ triangleMeshBuffer.address(), meshMatrixBuffer.address() };

		vk::Fence renderFence = device.createFence();
		dq.push([&]() { device.vk().destroyFence(renderFence); });

		vk::Semaphore presentSemaphore = device.createSemaphore();
		dq.push([&]() { device.vk().destroySemaphore(presentSemaphore); });

		vk::Semaphore renderSemaphore = device.createSemaphore();
		dq.push([&]() { device.vk().destroySemaphore(renderSemaphore); });

		SOULKAN_NAMESPACE::Shader vertShader(device, "triangle.vert", vk::ShaderStageFlagBits::eVertex);
		dq.push([&]() { vertShader.destroy(); });

		SOULKAN_NAMESPACE::Shader fragShader(device, "triangle.frag", vk::ShaderStageFlagBits::eFragment);
		dq.push([&]() { fragShader.destroy(); });


		SOULKAN_NAMESPACE::vec_ref<SOULKAN_NAMESPACE::Shader> shaders{ vertShader, fragShader };

		SOULKAN_NAMESPACE::GraphicsPipeline solidPipelineTmp(device);
		SOULKAN_NAMESPACE::GraphicsPipeline solidPipeline(device, shaders, 
															 vk::PrimitiveTopology::eTriangleList, vk::PolygonMode::eFill, 
															 swapchain.extent(), swapchain.imageFormat());
		dq.push([&]() { solidPipeline.destroy(); });
		
		SOULKAN_NAMESPACE::GraphicsPipeline wireframePipelineTmp(device);
		SOULKAN_NAMESPACE::GraphicsPipeline wireframePipeline(device, shaders,
															  vk::PrimitiveTopology::eTriangleList, vk::PolygonMode::eLine,
															  swapchain.extent(), swapchain.imageFormat());
		dq.push([&]() { wireframePipeline.destroy(); });

		vk::Pipeline boundPipeline = solidPipeline.vk();
		vk::PipelineLayout boundPipelineLayout = solidPipeline.layout();

		glm::vec3 camPos = { 0.f,0.f,-2.f };
		float rotationSpeed = 0.1f;

		uint32_t i = 0;
		double lastInputTime = 0;
		std::atomic<bool> status(false);
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
			window.rename(std::format("Salut maeba ({})", i));


			//Shader recompilation and graphics pipeline rebuilding when pressing R
			int state = glfwGetKey(window.window(), GLFW_KEY_R);
			if (state == GLFW_PRESS && glfwGetTime() > lastInputTime + 1) //Pressed a key more than one second ago
			{
				lastInputTime = glfwGetTime();

				std::thread shaderCompilationThread([&]()
					{
						shaders[0].get().shader(true);//Vertex
						shaders[1].get().shader(true);//Fragment

						solidPipelineTmp = SOULKAN_NAMESPACE::GraphicsPipeline(device, shaders,
																			   vk::PrimitiveTopology::eTriangleList, vk::PolygonMode::eFill,
																			   swapchain.extent(), swapchain.imageFormat());

						wireframePipelineTmp = SOULKAN_NAMESPACE::GraphicsPipeline(device, shaders,
																			   vk::PrimitiveTopology::eTriangleList, vk::PolygonMode::eLine,
																			   swapchain.extent(), swapchain.imageFormat());

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

			//Switch to wireframe pipeline when pressing w
			state = glfwGetKey(window.window(), GLFW_KEY_Z);
			if (state == GLFW_PRESS && glfwGetTime() > lastInputTime + 1) //Pressed a key more than one second ago
			{
				lastInputTime = glfwGetTime();
				boundPipeline = wireframePipeline.vk();
				boundPipelineLayout = wireframePipeline.layout();
				std::cout << "Switched to triangle pipeline" << std::endl;
			}

			state = glfwGetKey(window.window(), GLFW_KEY_A);
			if (state == GLFW_PRESS && glfwGetTime() > lastInputTime + 0.01) //Pressed a key more than one second ago
			{
				lastInputTime = glfwGetTime();

				camPos.x += 0.2f;
			}

			state = glfwGetKey(window.window(), GLFW_KEY_D);
			if (state == GLFW_PRESS && glfwGetTime() > lastInputTime + 0.01) //Pressed a key more than one second ago
			{
				lastInputTime = glfwGetTime();

				camPos.x -= 0.2f;
			}

			state = glfwGetKey(window.window(), GLFW_KEY_S);
			if (state == GLFW_PRESS && glfwGetTime() > lastInputTime + 0.01) //Pressed a key more than one second ago
			{
				lastInputTime = glfwGetTime();

				camPos.z -= 0.2f;
			}

			state = glfwGetKey(window.window(), GLFW_KEY_W);
			if (state == GLFW_PRESS && glfwGetTime() > lastInputTime + 0.01) //Pressed a key more than one second ago
			{
				lastInputTime = glfwGetTime();

				camPos.z += 0.2f;
			}

			state = glfwGetKey(window.window(), GLFW_KEY_SPACE);
			if (state == GLFW_PRESS && glfwGetTime() > lastInputTime + 0.01) //Pressed a key more than one second ago
			{
				lastInputTime = glfwGetTime();

				camPos.y -= 0.2f;
			}

			state = glfwGetKey(window.window(), GLFW_KEY_LEFT_SHIFT);
			if (state == GLFW_PRESS && glfwGetTime() > lastInputTime + 0.01) //Pressed a key more than one second ago
			{
				lastInputTime = glfwGetTime();

				camPos.y += 0.2f;
			}

			state = glfwGetKey(window.window(), GLFW_KEY_O);
			if (state == GLFW_PRESS && glfwGetTime() > lastInputTime + 0.05) //Pressed a key more than one second ago
			{
				lastInputTime = glfwGetTime();

				rotationSpeed += 0.01f;
			}

			state = glfwGetKey(window.window(), GLFW_KEY_P);
			if (state == GLFW_PRESS && glfwGetTime() > lastInputTime + 0.05) //Pressed a key more than one second ago
			{
				lastInputTime = glfwGetTime();

				rotationSpeed -= 0.01f;
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

			glm::mat4 view = glm::translate(glm::mat4(1.f), camPos);
			//camera projection
			glm::mat4 projection = glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 200.0f);
			projection[1][1] *= -1;

			//model rotation
			glm::mat4 model = glm::rotate(glm::mat4{ 1.0f }, glm::radians(i * rotationSpeed), glm::vec3(0, 1, 0));

			//calculate final mesh matrix
			glm::mat4 meshMatrix = projection * view * model;

			meshMatrixBuffer.upload(&meshMatrix, sizeof(meshMatrix));

			commandBuffer.vk().pushConstants(boundPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, 2*sizeof(vk::DeviceAddress), bufferAddresses1.data());
			
			commandBuffer.vk().draw(monkeyMesh.size(), 1, 0, 0);

			//

			commandBuffer.vk().pushConstants(boundPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, 2 * sizeof(vk::DeviceAddress), bufferAddresses2.data());

			commandBuffer.vk().draw(triangleMesh.size(), 1, 0, 0);

			commandBuffer.endRendering();

			//INFO:Transitioning image layout to something presentable after rendering has finished
			commandBuffer.imageLayoutTransition(vk::ImageLayout::eAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR, swapchain.images()[imageIndex],
												vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
												vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone); 

			commandBuffer.end();

			graphicsQueue.submit(presentSemaphore, renderSemaphore,
				                 commandBuffer, renderFence);

			graphicsQueue.present(swapchain, renderSemaphore, imageIndex);

			i++;
		}

		device.waitFence(renderFence);

		dq.flush();
	}
}
#endif
/*Include guards to avoid including this .hpp file multiple times*/
#ifndef SOULKAN_HPP
#define SOULKAN_HPP

#define SOULKAN_NAMESPACE sk
#define SOULKAN_TEST_NAMESPACE skt

/*Vulkan/GLFW includes*/
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#define GLFW_INCLUDE_VULKAN

/*Std includes*/
#include <fstream>
#include <iostream>
#include <format>
#include <memory>
#include <utility>
#include <deque>
#include <functional>
#include <cstdlib>

/*Informations about comments and their "captions" (INFO, TODO, ...):
 *INFO: A purely informational and context relevant comment
 *TODO: Something must be tackled and taken care of here, in the near future
 *MAYB: Something worth researching, not TODO worthy as it may not be worth implementing
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

#define GLFW_CHECK(x)                                                                                                                                             \
{       x;                                                                                                                                                        \
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


		//Not cached since instance can vary
		vk::SurfaceKHR surface(Instance& instance)
		{
			VkSurfaceKHR tmp;
			VK_CHECK(vk::Result(glfwCreateWindowSurface(instance.instance(), window_, nullptr, &tmp)));
			GLFW_CHECK(/*Checking if window creation was succesfull, TODO:Not sure if necessary after VK_CHECK*/);

			if (tmp == VK_NULL_HANDLE) { KILL("An error occured in surface creation, killing process"); } //TODO:Not sure if necessary after VK_CHECK

			return vk::SurfaceKHR(tmp);
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
					vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
					vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
					vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose);

				debugCI.setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
					vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
					vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance);

				debugCI.setPfnUserCallback(debugCallback);
				debugCI.setPUserData(nullptr);
			}

			createInfo_.flags = vk::InstanceCreateFlags();
			createInfo_.pApplicationInfo = &appInfo_;

			createInfo_.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo_.ppEnabledLayerNames = validationLayers.data();

			createInfo_.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
			createInfo_.ppEnabledExtensionNames = extensions.data();

			instance_ = vk::createInstance(createInfo_);

			if (validation)
			{
				auto dynamicLoader = vk::DispatchLoaderDynamic(instance_, vkGetInstanceProcAddr);
				VK_CHECK(instance_.createDebugUtilsMessengerEXT(&debugCI, nullptr, &debugMessenger_, dynamicLoader));
			}
		}

		Instance(Instance&& other) noexcept : 
			instance_(other.instance_), debugMessenger_(other.debugMessenger_), appInfo_(other.appInfo_), createInfo_(other.createInfo_) 
		{
			other.instance_ = nullptr;
			other.debugMessenger_ = nullptr;
			other.appInfo_ = vk::ApplicationInfo{};
			other.createInfo_ = vk::InstanceCreateInfo{};
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

			createInfo_ = other.createInfo_;
			other.createInfo_ = vk::InstanceCreateInfo{};
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

		vk::Instance instance() const                      { return instance_; }
		vk::DebugUtilsMessengerEXT debugMessenger() const  { return debugMessenger_; }
		vk::ApplicationInfo appInfo() const                { return appInfo_; }
		vk::InstanceCreateInfo createInfo() const          { return createInfo_; }

	private:
		vk::Instance instance_ = nullptr;
		vk::DebugUtilsMessengerEXT debugMessenger_ = nullptr;

		vk::ApplicationInfo appInfo_ = {};
		vk::InstanceCreateInfo createInfo_ = {};

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

	class Device
	{
	public:
		Device() {}
		Device(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface) :
			physicalDevice_(physicalDevice), surface_(surface)
		{
			//Queue families
			std::vector<vk::DeviceQueueCreateInfo> deviceQueueCreateInfos = {};
			float queuePriority = 1.0f; //MAYB:Look into queue priority
			for (const auto& q : queueConcentrate())
			{
				deviceQueueCreateInfos.push_back(vk::DeviceQueueCreateInfo(vk::DeviceQueueCreateFlags(), q, 1, &queuePriority));
			}

			//Extensions

			//Building
		}


		Device(Device&& other) = default;
		Device& operator=(Device&& other) = default;

		//No copy constructors
		Device(Device& other) = delete;
		Device& operator=(Device& other) = delete;


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

		vk::Device device() const { return device_; }
		vk::PhysicalDevice physicalDevice() const { return physicalDevice_; }

	private:
		vk::Device device_ = nullptr;
		vk::SurfaceKHR surface_ = nullptr;
		std::array<uint32_t, INDEX(QueueFamilyCapability::COUNT)> queueFamilies_ = { std::numeric_limits<uint32_t>::max() }; //Debug value

		vk::PhysicalDevice physicalDevice_ = nullptr;
		std::vector<std::string> supportedExtensions_ = {};

		//If queue at index is defined (other than uint32_t max) then it is available to use
		bool queueAvailable(QueueFamilyCapability capability)
		{
			return (queueFamilies_[INDEX(capability)] != std::numeric_limits<uint32_t>::max());
		}

		std::vector<uint32_t> queueConcentrate()
		{
			std::vector<uint32_t> concentrate = {};

			//Filter out not found queues
			for (const auto& q : queueFamilies())
			{
				if ((q != std::numeric_limits<uint32_t>::max()) && (std::find(concentrate.begin(), concentrate.end(), q) != concentrate.end()))
				{
					concentrate.push_back(q);
				}
			}

			return concentrate;
		}
	};
}



namespace SOULKAN_TEST_NAMESPACE
{
	void error_test()
	{
		VK_CHECK(vk::Result::eTimeout);

		GLFW_CHECK(glfwCreateWindow(0, 0, "ok", 0, NULL));

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

		SOULKAN_NAMESPACE::Device dev(best);

		auto supportedDeviceExtensions = dev.supportedExtensions();
		std::cout << std::format("--Supported device extensions ({}):", supportedDeviceExtensions.size()) << std::endl;

		for (const auto& e : supportedDeviceExtensions)
		{
			std::cout << "__" << e << std::endl;
		}
		std::cout << std::endl;

		std::cout << "VK_KHR_dynamic_rendering is " << (dev.isSupported("VK_KHR_dynamic_rendering") ? "supported" : "not supported") << std::endl;

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

		SOULKAN_NAMESPACE::Window w(800, 600, "Salut maeba");
		dq.push([&]() { w.destroy(); /*Not necessary since glfwTerminate() destroys every remaining window*/});

		SOULKAN_NAMESPACE::Instance instance(true);
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
}
#endif
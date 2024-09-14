/**
 * Vulkan instance and device context management.
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/logging.h>
#include <VKFW/vector.h>
#include <VKFW/vkfw.h>
#include <VKFW/window_api.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

struct extension_request {
	char *name;
	bool required;
};

static VKFWvector<extension_request> requested_instance_extensions;
static VKFWvector<extension_request> requested_device_extensions;
static VKFWvector<extension_request> requested_layers;

static VkResult
handle_request (const char *name, bool required, VKFWvector<extension_request> &v)
{
	for (extension_request &r : v) {
		if (!strcmp (r.name, name)) {
			if (required)
				r.required = true;
			return VK_SUCCESS;
		}
	}

	extension_request r;
	r.name = strdup (name);
	r.required = required;
	if (!name)
		return VK_ERROR_OUT_OF_HOST_MEMORY;

	if (v.push_back (r))
		return VK_SUCCESS;

	free (r.name);
	return VK_ERROR_OUT_OF_HOST_MEMORY;
}

extern "C"
VKFWAPI VkResult
vkfwRequestInstanceExtension (const char *name, bool required)
{
	return handle_request (name, required, requested_instance_extensions);
}

extern "C"
VKFWAPI VkResult
vkfwRequestDeviceExtension (const char *name, bool required)
{
	return handle_request (name, required, requested_device_extensions);
}

extern "C"
VKFWAPI VkResult
vkfwRequestLayer (const char *name, bool required)
{
	return handle_request (name, required, requested_layers);
}

static VKFWstringvec enabled_instance_extensions;
static VKFWstringvec enabled_layers;

VkInstance vkfwLoadedInstance;
static VkDebugUtilsMessengerEXT debug_messenger;

bool vkfwHasInstance11;
bool vkfwHasInstance12;
bool vkfwHasInstance13;
bool vkfwHasDebugUtils;

static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_utils_handler (VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT type,
	const VkDebugUtilsMessengerCallbackDataEXT *data,
	void *user)
{
	(void) severity;
	(void) user;

	/**
	 * The Vulkan loader is __very__ verbose. Silence it.
	 */
	if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
		&& (severity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT))
			return VK_FALSE;

	const char *typ = "VULKAN";
	if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
		typ = "VULKAN-GENERAL";
	if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
		typ = "VULKAN-VALIDATION";
	if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
		typ = "VULKAN-PERFORMANCE";

	vkfwPrintf (VKFW_LOG_CORE, "%s: %s\n", typ, data->pMessage);
	return VK_FALSE;
}

extern "C"
VKFWAPI VkResult
vkfwCreateInstance (VkInstance *out, const VkInstanceCreateInfo *ci,
	uint32_t flags)
{
	VkResult result;
	VkInstanceCreateInfo info {};
	VkDebugUtilsMessengerCreateInfoEXT debug_ci {};
	uint32_t count;

	info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	info.pNext = ci->pNext;
	info.flags = ci->flags;
	info.pApplicationInfo = ci->pApplicationInfo;

	if (flags & VKFW_CREATE_INSTANCE_DEBUG_MESSENGER) {
		result = vkfwRequestInstanceExtension (VK_EXT_DEBUG_UTILS_EXTENSION_NAME, false);
		if (result != VK_SUCCESS)
			return result;

		result = vkfwRequestLayer ("VK_LAYER_KHRONOS_validation", false);
		if (result != VK_SUCCESS)
			return result;
	}

	if (vkfwCurrentWindowBackend->request_instance_extensions) {
		result = vkfwCurrentWindowBackend->request_instance_extensions ();
		if (result != VK_SUCCESS)
			return result;
	}

	count = 0;
	result = vkEnumerateInstanceExtensionProperties (
		nullptr, &count, nullptr);
	if (result != VK_SUCCESS)
		return result;

	VKFWvector<VkExtensionProperties> available_extensions;
	if (!available_extensions.resize (count))
		return VK_ERROR_OUT_OF_HOST_MEMORY;

	result = vkEnumerateInstanceExtensionProperties (
		nullptr, &count, available_extensions.data ());
	if (result != VK_SUCCESS)
		return result;

	count = 0;
	result = vkEnumerateInstanceLayerProperties (&count, nullptr);
	if (result != VK_SUCCESS)
		return result;

	VKFWvector<VkLayerProperties> available_layers;
	if (!available_layers.resize (count))
		return VK_ERROR_OUT_OF_HOST_MEMORY;

	result = vkEnumerateInstanceLayerProperties (&count, available_layers.data ());
	if (result != VK_SUCCESS)
		return result;

	for (const extension_request &r : requested_instance_extensions) {
		bool exists = false;
		for (const VkExtensionProperties &p : available_extensions) {
			if (!strcmp (r.name, p.extensionName)) {
				exists = true;
				break;
			}
		}

		if (exists) {
			char *s = strdup (r.name);
			if (!s) {
				result = VK_ERROR_OUT_OF_HOST_MEMORY;
				goto error;
			}
			if (!enabled_instance_extensions.push_back (s)) {
				result = VK_ERROR_OUT_OF_HOST_MEMORY;
				free (s);
				goto error;
			}
		} else if (r.required) {
			result = VK_ERROR_EXTENSION_NOT_PRESENT;
			goto error;
		}
	}

	for (const extension_request &r : requested_layers) {
		bool exists = false;
		for (const VkLayerProperties &p : available_layers) {
			if (!strcmp (r.name, p.layerName)) {
				exists = true;
				break;
			}
		}

		if (exists) {
			char *s = strdup (r.name);
			if (!s) {
				result = VK_ERROR_OUT_OF_HOST_MEMORY;
				goto error;
			}
			if (!enabled_layers.push_back (s)) {
				result = VK_ERROR_OUT_OF_HOST_MEMORY;
				free (s);
				goto error;
			}
		} else if (r.required) {
			result = VK_ERROR_LAYER_NOT_PRESENT;
			goto error;
		}
	}

	for (const char *s : enabled_instance_extensions)
		vkfwPrintf (VKFW_LOG_CORE, "VKFW: enabling instance extension %s\n", s);

	for (const char *s : enabled_layers)
		vkfwPrintf (VKFW_LOG_CORE, "VKFW: enabling layer %s\n", s);

	info.enabledExtensionCount = enabled_instance_extensions.size ();
	info.ppEnabledExtensionNames = enabled_instance_extensions.data ();
	info.enabledLayerCount = enabled_layers.size ();
	info.ppEnabledLayerNames = enabled_layers.data ();

	if (info.pApplicationInfo) {
		if (info.pApplicationInfo->apiVersion >= VK_API_VERSION_1_1)
			vkfwHasInstance11 = true;
		if (info.pApplicationInfo->apiVersion >= VK_API_VERSION_1_2)
			vkfwHasInstance12 = true;
		if (info.pApplicationInfo->apiVersion >= VK_API_VERSION_1_3)
			vkfwHasInstance13 = true;
	}

	for (const char *s : enabled_instance_extensions) {
		if (!strcmp (s, VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
			vkfwHasDebugUtils = true;
	}

	debug_ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debug_ci.messageSeverity = 0x1111;
	debug_ci.messageType = 7;
	debug_ci.pfnUserCallback = debug_utils_handler;
	if (vkfwHasDebugUtils && (flags & VKFW_CREATE_INSTANCE_DEBUG_MESSENGER))
		vkfw_append_struct (&info, &debug_ci);

	VkInstance instance;
	result = vkCreateInstance (&info, nullptr, &instance);
	if (result != VK_SUCCESS)
		goto error;

	vkfwLoadInstance (instance);

	if (vkfwHasDebugUtils && (flags & VKFW_CREATE_INSTANCE_DEBUG_MESSENGER)) {
		debug_ci.pNext = nullptr;
		result = vkCreateDebugUtilsMessengerEXT (instance,
			&debug_ci, nullptr, &debug_messenger);
		if (result != VK_SUCCESS)
			vkfwPrintf (VKFW_LOG_CORE, "VKFW: could not create debug utils messenger\n");
	}

	vkfwLoadedInstance = instance;
	*out = instance;
	return VK_SUCCESS;
error:
	for (char *s : enabled_instance_extensions)
		free (s);
	enabled_instance_extensions.resize (0);
	for (char *s : enabled_layers)
		free (s);
	enabled_layers.resize (0);
	return result;
}

void
vkfwShutdownInstance (void)
{
	if (debug_messenger)
		vkDestroyDebugUtilsMessengerEXT (vkfwLoadedInstance,
			debug_messenger, nullptr);
	vkDestroyInstance (vkfwLoadedInstance, nullptr);
	vkfwLoadedInstance = VK_NULL_HANDLE;
}

VkDevice vkfwLoadedDevice;
VkPhysicalDevice vkfwPhysicalDevice;

static VKFWstringvec enabled_device_extensions;

extern "C"
VKFWAPI VkResult
vkfwCreateDevice (VkDevice *out, VkPhysicalDevice physical_device,
	const VkDeviceCreateInfo *ci)
{
	VkResult result;
	VkDeviceCreateInfo info;
	uint32_t count;

	info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	info.pNext = ci->pNext;
	info.flags = ci->flags;
	info.queueCreateInfoCount = ci->queueCreateInfoCount;
	info.pQueueCreateInfos = ci->pQueueCreateInfos;
	info.enabledLayerCount = enabled_layers.size ();
	info.ppEnabledLayerNames = enabled_layers.data ();
	info.pEnabledFeatures = ci->pEnabledFeatures;

	count = 0;
	result = vkEnumerateDeviceExtensionProperties (physical_device,
		nullptr, &count, nullptr);
	if (result != VK_SUCCESS)
		return result;

	VKFWvector<VkExtensionProperties> available_extensions;
	if (!available_extensions.resize (count))
		return VK_ERROR_OUT_OF_HOST_MEMORY;

	result = vkEnumerateDeviceExtensionProperties (physical_device,
		nullptr, &count, available_extensions.data ());
	if (result != VK_SUCCESS)
		return result;

	for (const extension_request &r : requested_device_extensions) {
		bool exists = false;
		for (const VkExtensionProperties &p : available_extensions) {
			if (!strcmp (r.name, p.extensionName)) {
				exists = true;
				break;
			}
		}

		if (exists) {
			char *s = strdup (r.name);
			if (!s) {
				result = VK_ERROR_OUT_OF_HOST_MEMORY;
				goto error;
			}
			if (!enabled_device_extensions.push_back (s)) {
				result = VK_ERROR_OUT_OF_HOST_MEMORY;
				free (s);
				goto error;
			}
		} else if (r.required) {
			result = VK_ERROR_EXTENSION_NOT_PRESENT;
			goto error;
		}
	}

	for (const char *s : enabled_device_extensions)
		vkfwPrintf (VKFW_LOG_CORE, "VKFW: enabling device extension %s\n", s);

	info.enabledExtensionCount = enabled_device_extensions.size ();
	info.ppEnabledExtensionNames = enabled_device_extensions.data ();

	VkDevice device;
	result = vkCreateDevice (physical_device, &info, nullptr, &device);
	if (result != VK_SUCCESS)
		goto error;

	vkfwLoadDevice (device);

	vkfwLoadedDevice = device;
	*out = device;
	return VK_SUCCESS;
error:
	for (char *s : enabled_device_extensions)
		free (s);
	enabled_device_extensions.resize (0);
	return result;
}

void
vkfwShutdownDevice (void)
{
	vkDestroyDevice (vkfwLoadedDevice, nullptr);
	vkfwLoadedDevice = VK_NULL_HANDLE;
}

extern "C"
VKFWAPI bool
vkfwHasInstanceExtension (const char *extension_name)
{
	for (const char *extension : enabled_instance_extensions)
		if (!strcmp (extension, extension_name))
			return true;
	return false;
}

extern "C"
VKFWAPI bool
vkfwHasDeviceExtension (const char *extension_name)
{
	for (const char *extension : enabled_device_extensions)
		if (!strcmp (extension, extension_name))
			return true;
	return false;
}

extern "C"
VKFWAPI VkResult
vkfwGetPhysicalDevicePresentSupport (VkPhysicalDevice device, uint32_t queue,
	VkBool32 *out)
{
	if (vkfwCurrentWindowBackend->query_present_support)
		return vkfwCurrentWindowBackend->query_present_support (device, queue, out);

	/** If the window backend doesn't provide an implementation, assume true. */

	*out = VK_TRUE;
	return VK_SUCCESS;
}

	/**
	 * Select and create a Vulkan device on behalf of the application.
	 *
	 * This function is stupidly large (approx. 450 LoC), but it is flexible
	 * enough for many applications.
	 *
	 *  Please let this be a one-off.
	 * ~david
	 */

extern "C"
VKFWAPI VkResult
vkfwAutoCreateDevice (VkDevice *out, VkPhysicalDevice *out_physical,
	const VkPhysicalDeviceFeatures2 *features,
	uint32_t *graphics_queue, uint32_t *compute_queue,
	uint32_t *present_queue, uint32_t *transfer_queue)
{
	VkResult result;
	VkPhysicalDevice best_device;
	int best_score = -1;
	uint32_t best_gqueue, best_cqueue, best_pqueue, best_tqueue;

	const VkPhysicalDeviceFeatures *req_feat10 = nullptr;
	const VkPhysicalDeviceVulkan11Features *req_feat11 = nullptr;
	const VkPhysicalDeviceVulkan12Features *req_feat12 = nullptr;
	const VkPhysicalDeviceVulkan13Features *req_feat13 = nullptr;

	VkPhysicalDeviceProperties props;

	if (features) {
		req_feat10 = &features->features;
		const VkBaseInStructure *s =
			(const VkBaseInStructure *) features->pNext;

		for (; s; s = s->pNext) {
			switch (s->sType) {
			case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES:
				if (req_feat11)
					return VK_ERROR_UNKNOWN;
				req_feat11 = (const VkPhysicalDeviceVulkan11Features *) s;
				break;
			case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES:
				if (req_feat12)
					return VK_ERROR_UNKNOWN;
				req_feat12 = (const VkPhysicalDeviceVulkan12Features *) s;
				break;
			case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES:
				if (req_feat13)
					return VK_ERROR_UNKNOWN;
				req_feat13 = (const VkPhysicalDeviceVulkan13Features *) s;
				break;
			default:
				return VK_ERROR_UNKNOWN;
			}
		}
	}

	if ((req_feat11 && !vkfwHasInstance11) || (req_feat12 && !vkfwHasInstance12)
		|| (req_feat13 && !vkfwHasInstance13))
			return VK_ERROR_UNKNOWN;

	uint32_t count = 0;
	result = vkEnumeratePhysicalDevices (vkfwLoadedInstance,
		&count, nullptr);
	if (result != VK_SUCCESS)
		return result;

	VKFWvector<VkPhysicalDevice> devices;
	if (!devices.resize (count))
		return VK_ERROR_OUT_OF_HOST_MEMORY;

	result = vkEnumeratePhysicalDevices (vkfwLoadedInstance,
		&count, devices.data ());
	if (result != VK_SUCCESS)
		return result;

	for (VkPhysicalDevice device : devices) {
		vkGetPhysicalDeviceProperties (device, &props);

		if ((req_feat11 && props.apiVersion < VK_API_VERSION_1_1)
			|| (req_feat12 && props.apiVersion < VK_API_VERSION_1_2)
			|| (req_feat13 && props.apiVersion < VK_API_VERSION_1_3)) {
				vkfwPrintf (VKFW_LOG_CORE, "VKFW: vkfwAutoCreateDevice: \"%s\" lacks support for the required API version\n",
					props.deviceName);
				continue;
		}

		count = 0;
		result = vkEnumerateDeviceExtensionProperties (device,
			nullptr, &count, nullptr);
		if (result != VK_SUCCESS)
			return result;

		VKFWvector<VkExtensionProperties> extensions;
		if (!extensions.resize (count))
			return VK_ERROR_OUT_OF_HOST_MEMORY;

		result = vkEnumerateDeviceExtensionProperties (device,
			nullptr, &count, extensions.data ());
		if (result != VK_SUCCESS)
			return result;

		bool has_required_extensions = true;
		const char *tmp = "";
		for (const extension_request &r : requested_device_extensions) {
			tmp = r.name;
			if (!r.required)
				continue;
			bool exists = false;
			for (const VkExtensionProperties &p : extensions) {
				if (!strcmp (r.name, p.extensionName)) {
					exists = true;
					break;
				}
			}
			if (!exists) {
				has_required_extensions = false;
				break;
			}
		}

		if (!has_required_extensions) {
			vkfwPrintf (VKFW_LOG_CORE, "VKFW: vkfwAutoCreateDevice: \"%s\" lacks support for %s\n",
				props.deviceName, tmp);
			continue;
		}

		VkPhysicalDeviceFeatures features10 {};
		VkPhysicalDeviceVulkan11Features features11 {};
		VkPhysicalDeviceVulkan12Features features12 {};
		VkPhysicalDeviceVulkan13Features features13 {};
		features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
		features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

		if (props.apiVersion >= VK_API_VERSION_1_1) {
			VkPhysicalDeviceFeatures2 features2 {};
			features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			vkfw_append_struct (&features2, &features11);
			if (props.apiVersion >= VK_API_VERSION_1_2)
				vkfw_append_struct (&features2, &features12);
			if (props.apiVersion >= VK_API_VERSION_1_3)
				vkfw_append_struct (&features2, &features13);
			vkGetPhysicalDeviceFeatures2 (device, &features2);
			features10 = features2.features;
		} else
			vkGetPhysicalDeviceFeatures (device, &features10);

	/**
	 * Macro to check if a requested feature is supported by the device.
	 *
	 * note: use goto to force the compiler to emit more efficient code.
	 */
#define F(v, name)						\
		if (req_feat##v->name && !features##v.name) {	\
			tmp = "Vulkan" #v "Features::" #name;	\
			goto device_lacks_feature;		\
		}

	/**
	 * approx. 140 lines of feature checks. totally reasonable stuff
	 */

		if (req_feat10) {
F(10, robustBufferAccess)
F(10, fullDrawIndexUint32)
F(10, imageCubeArray)
F(10, independentBlend)
F(10, geometryShader)
F(10, tessellationShader)
F(10, sampleRateShading)
F(10, dualSrcBlend)
F(10, logicOp)
F(10, multiDrawIndirect)
F(10, drawIndirectFirstInstance)
F(10, depthClamp)
F(10, depthBiasClamp)
F(10, fillModeNonSolid)
F(10, depthBounds)
F(10, wideLines)
F(10, largePoints)
F(10, alphaToOne)
F(10, multiViewport)
F(10, samplerAnisotropy)
F(10, textureCompressionETC2)
F(10, textureCompressionASTC_LDR)
F(10, textureCompressionBC)
F(10, occlusionQueryPrecise)
F(10, pipelineStatisticsQuery)
F(10, vertexPipelineStoresAndAtomics)
F(10, fragmentStoresAndAtomics)
F(10, shaderTessellationAndGeometryPointSize)
F(10, shaderImageGatherExtended)
F(10, shaderStorageImageExtendedFormats)
F(10, shaderStorageImageMultisample)
F(10, shaderStorageImageReadWithoutFormat)
F(10, shaderStorageImageWriteWithoutFormat)
F(10, shaderUniformBufferArrayDynamicIndexing)
F(10, shaderSampledImageArrayDynamicIndexing)
F(10, shaderStorageBufferArrayDynamicIndexing)
F(10, shaderStorageImageArrayDynamicIndexing)
F(10, shaderClipDistance)
F(10, shaderCullDistance)
F(10, shaderFloat64)
F(10, shaderInt64)
F(10, shaderInt16)
F(10, shaderResourceResidency)
F(10, shaderResourceMinLod)
F(10, sparseBinding)
F(10, sparseResidencyBuffer)
F(10, sparseResidencyImage2D)
F(10, sparseResidencyImage3D)
F(10, sparseResidency2Samples)
F(10, sparseResidency4Samples)
F(10, sparseResidency8Samples)
F(10, sparseResidency16Samples)
F(10, sparseResidencyAliased)
F(10, variableMultisampleRate)
F(10, inheritedQueries)
		}
		if (req_feat11) {
F(11, storageBuffer16BitAccess)
F(11, uniformAndStorageBuffer16BitAccess)
F(11, storagePushConstant16)
F(11, storageInputOutput16)
F(11, multiview)
F(11, multiviewGeometryShader)
F(11, multiviewTessellationShader)
F(11, variablePointersStorageBuffer)
F(11, variablePointers)
F(11, protectedMemory)
F(11, samplerYcbcrConversion)
F(11, shaderDrawParameters)
		}
		if (req_feat12) {
F(12, samplerMirrorClampToEdge)
F(12, drawIndirectCount)
F(12, storageBuffer8BitAccess)
F(12, uniformAndStorageBuffer8BitAccess)
F(12, storagePushConstant8)
F(12, shaderBufferInt64Atomics)
F(12, shaderSharedInt64Atomics)
F(12, shaderFloat16)
F(12, shaderInt8)
F(12, descriptorIndexing)
F(12, shaderInputAttachmentArrayDynamicIndexing)
F(12, shaderUniformTexelBufferArrayDynamicIndexing)
F(12, shaderStorageTexelBufferArrayDynamicIndexing)
F(12, shaderUniformBufferArrayNonUniformIndexing)
F(12, shaderSampledImageArrayNonUniformIndexing)
F(12, shaderStorageBufferArrayNonUniformIndexing)
F(12, shaderStorageImageArrayNonUniformIndexing)
F(12, shaderInputAttachmentArrayNonUniformIndexing)
F(12, shaderUniformTexelBufferArrayNonUniformIndexing)
F(12, shaderStorageTexelBufferArrayNonUniformIndexing)
F(12, descriptorBindingUniformBufferUpdateAfterBind)
F(12, descriptorBindingSampledImageUpdateAfterBind)
F(12, descriptorBindingStorageImageUpdateAfterBind)
F(12, descriptorBindingStorageBufferUpdateAfterBind)
F(12, descriptorBindingUniformTexelBufferUpdateAfterBind)
F(12, descriptorBindingStorageTexelBufferUpdateAfterBind)
F(12, descriptorBindingUpdateUnusedWhilePending)
F(12, descriptorBindingPartiallyBound)
F(12, descriptorBindingVariableDescriptorCount)
F(12, runtimeDescriptorArray)
F(12, samplerFilterMinmax)
F(12, scalarBlockLayout)
F(12, imagelessFramebuffer)
F(12, uniformBufferStandardLayout)
F(12, shaderSubgroupExtendedTypes)
F(12, separateDepthStencilLayouts)
F(12, hostQueryReset)
F(12, timelineSemaphore)
F(12, bufferDeviceAddress)
F(12, bufferDeviceAddressCaptureReplay)
F(12, bufferDeviceAddressMultiDevice)
F(12, vulkanMemoryModel)
F(12, vulkanMemoryModelDeviceScope)
F(12, vulkanMemoryModelAvailabilityVisibilityChains)
F(12, shaderOutputViewportIndex)
F(12, shaderOutputLayer)
F(12, subgroupBroadcastDynamicId)
		}
		if (req_feat13) {
F(13, robustImageAccess)
F(13, inlineUniformBlock)
F(13, descriptorBindingInlineUniformBlockUpdateAfterBind)
F(13, pipelineCreationCacheControl)
F(13, privateData)
F(13, shaderDemoteToHelperInvocation)
F(13, shaderTerminateInvocation)
F(13, subgroupSizeControl)
F(13, computeFullSubgroups)
F(13, synchronization2)
F(13, textureCompressionASTC_HDR)
F(13, shaderZeroInitializeWorkgroupMemory)
F(13, dynamicRendering)
F(13, shaderIntegerDotProduct)
F(13, maintenance4)
		}
#undef F

			/**
			 *   Vim macros are magic. Please take the time to learn
			 *   the basics if you don't know them already.
			 *
			 * ~david
			 */

		if (0) {
device_lacks_feature:
			vkfwPrintf (VKFW_LOG_CORE, "VKFW: vkfwAutoCreateDevice: \"%s\" lacks support for %s\n",
				props.deviceName, tmp);
			continue;
		}

		count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties (device,
			&count, nullptr);

		VKFWvector<VkQueueFamilyProperties> families;
		if (!families.resize (count))
			return VK_ERROR_OUT_OF_HOST_MEMORY;

		vkGetPhysicalDeviceQueueFamilyProperties (device,
			&count, families.data ());

		uint32_t gqueue = -1U, cqueue = -1U, pqueue = -1U, tqueue = -1U;
		for (uint32_t i = 0; i < count; i++) {
			const VkQueueFamilyProperties &qp = families[i];
			bool g = (qp.queueFlags & VK_QUEUE_GRAPHICS_BIT) ? true : false;
			bool c = (qp.queueFlags & VK_QUEUE_COMPUTE_BIT) ? true : false;
			bool t = (qp.queueFlags & VK_QUEUE_TRANSFER_BIT) ? true : false;
			bool p = false;
			VkBool32 p_ = VK_FALSE;
			result = vkfwGetPhysicalDevicePresentSupport (device, i, &p_);
			if (result != VK_SUCCESS)
				return result;
			if (p_)
				p = true;
			if (g || c)
				t = true;

			vkfwPrintf (VKFW_LOG_CORE, "VKFW: \"%s\"->queues[%" PRIu32 "] supports%s%s%s%s\n",
				props.deviceName, i, g ? " GRAPHICS" : "", c ? " COMPUTE" : "",
				t ? " TRANSFER" : "", p ? " PRESENT" : "");

			if (g && gqueue == -1U)
				gqueue = i;
			if (c && cqueue == -1U)
				cqueue = i;
			if (p && pqueue == -1U)
				pqueue = i;
			if (t && tqueue == -1U)
				tqueue = i;

			/**
			 * Prefer the same queue for graphics and present
			 * operations.
			 *
			 * rationale: this avoids the need for resource sharing
			 * on swapchain images.
			 */
			if (g && p && gqueue != pqueue) {
				gqueue = i;
				pqueue = i;
			}

			/**
			 * Prefer keeping the transfer queue separate from the
			 * graphics queue.
			 *
			 * rationale: modern GPUs have fast DMA transfer engines
			 * that are separate from the rest of the GPU. These can
			 * map to a separate queue family. A separate transfer
			 * queue is useful for resource uploading.
			 */
			if (t && gqueue == tqueue)
				tqueue = i;
		}

		if ((graphics_queue && gqueue == -1U)
			|| (compute_queue && cqueue == -1U)
			|| (transfer_queue && tqueue == -1U)
			|| (present_queue && pqueue == -1U))
				continue;

		int score = 0;
		if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			score += 100;
		else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
			score += 50;

		if (score > best_score) {
			best_score = score;
			best_device = device;
			best_gqueue = gqueue;
			best_cqueue = cqueue;
			best_tqueue = tqueue;
			best_pqueue = pqueue;
		}
	}

	if (best_score < 0)
		return VK_ERROR_INITIALIZATION_FAILED;

	uint32_t queueCount = 0;
	VkDeviceQueueCreateInfo queueInfos[4];

	if (graphics_queue) {
		*graphics_queue = best_gqueue;
		queueInfos[queueCount++].queueFamilyIndex = best_gqueue;
	}
	if (compute_queue && best_gqueue != best_cqueue) {
		*compute_queue = best_cqueue;
		queueInfos[queueCount++].queueFamilyIndex = best_cqueue;
	}
	if (transfer_queue && best_gqueue != best_tqueue
		&& best_cqueue != best_tqueue) {
			*transfer_queue = best_tqueue;
			queueInfos[queueCount++].queueFamilyIndex = best_tqueue;
	}
	if (present_queue && best_gqueue != best_pqueue
		&& best_cqueue != best_pqueue && best_tqueue != best_pqueue) {
			*present_queue = best_pqueue;
			queueInfos[queueCount++].queueFamilyIndex = best_pqueue;
	}

	float constant_1 = 1.0f;
	for (uint32_t i = 0; i < queueCount; i++) {
		queueInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfos[i].pNext = nullptr;
		queueInfos[i].flags = 0;
		queueInfos[i].queueCount = 1;
		queueInfos[i].pQueuePriorities = &constant_1;
	}

	VkDeviceCreateInfo ci {};
	ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	ci.pNext = features;
	ci.queueCreateInfoCount = queueCount;
	ci.pQueueCreateInfos = queueInfos;

	vkGetPhysicalDeviceProperties (best_device, &props);
	vkfwPrintf (VKFW_LOG_CORE, "VKFW: vkfwAutoCreateDevice: selected \"%s\"\n",
		props.deviceName);

	result = vkfwCreateDevice (out, best_device, &ci);
	if (result != VK_SUCCESS)
		return result;

	*out_physical = best_device;
	return VK_SUCCESS;
}

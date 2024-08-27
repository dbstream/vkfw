/**
 * VKFW example application.
 * Copyright (C) 2024  dbstream
 */
#include <VKFW/vkfw.h>
#include <stdio.h>
#include <new>

#include "example.frag.txt"
#include "example.vert.txt"

#define N 3

struct PerframeData {
	VkImage image;
	VkImageView view;
	VkFramebuffer framebuffer;
};

static VkInstance instance;
static VkDevice device;
static VkPhysicalDevice physical_device;
static VkQueue graphics_queue, present_queue;
static uint32_t graphics_queue_idx, present_queue_idx;
static VKFWwindow *window;
static VkSurfaceKHR surface;
static VkCommandPool command_pools[N];
static VkCommandBuffer command_buffers[N];
static VkFence acquire_fences[N];
static VkSemaphore acquire_semaphores[N];
static VkSemaphore present_semaphores[N];
static VkRenderPass render_pass;
static VkPipelineLayout pipeline_layout;
static VkPipeline pipeline;
static VkSwapchainKHR swapchain;
static VkExtent2D framebuffer_size;
static VkImage *images;
static VkImageView *views;
static VkFramebuffer *framebuffers;
static uint32_t swapchain_size;
static int frame_index;
static bool swapchain_dirty;

/**
 * TODO: check for format support at runtime.
 */

static const VkFormat swapchain_format = VK_FORMAT_B8G8R8A8_SRGB;
static const VkColorSpaceKHR swapchain_colorspace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

static const VkPresentModeKHR swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;

static bool
setup (void)
{
	VkResult result;
	VkApplicationInfo app_ci {};
	VkInstanceCreateInfo instance_ci {};

	vkfwEnableDebugLogging (VKFW_LOG_ALL);

	if (vkfwInit () != VK_SUCCESS)
		return false;

	result = vkfwRequestInstanceExtension ("VK_KHR_surface", true);
	if (result != VK_SUCCESS)
		goto err1;

	instance_ci.pApplicationInfo = &app_ci;
	app_ci.apiVersion = VK_API_VERSION_1_3;
	result = vkfwCreateInstance (&instance, &instance_ci,
		VKFW_CREATE_INSTANCE_DEBUG_MESSENGER);
	if (result != VK_SUCCESS)
		goto err1;

	result = vkfwRequestDeviceExtension ("VK_KHR_swapchain", true);
	if (result != VK_SUCCESS)
		goto err1;

	result = vkfwAutoCreateDevice (&device, &physical_device, nullptr,
		&graphics_queue_idx, nullptr, &present_queue_idx, nullptr);
	if (result != VK_SUCCESS)
		goto err1;

	vkGetDeviceQueue (device, graphics_queue_idx, 0, &graphics_queue);
	vkGetDeviceQueue (device, present_queue_idx, 0, &present_queue);

	result = vkfwCreateWindow (&window, {1280, 720});
	if (result != VK_SUCCESS)
		goto err1;

	result = vkfwSetWindowTitle (window, "VKFW example");
	if (result != VK_SUCCESS)
		goto err2;

	result = vkfwCreateSurface (window, &surface);
	if (result != VK_SUCCESS)
		goto err2;

	result = vkfwShowWindow (window);
	if (result != VK_SUCCESS)
		goto err3;

	return true;

err3:	vkDestroySurfaceKHR (instance, surface, nullptr);
err2:	vkfwDestroyWindow (window);
err1:	vkfwTerminate ();
	return false;
}

static void
teardown (void)
{
	vkDestroySurfaceKHR (instance, surface, nullptr);	
	vkfwDestroyWindow (window);
	vkfwTerminate ();
}

static bool
setup_command_resources (void)
{
	VkResult result;
	int i;

	VkCommandPoolCreateInfo pool_ci {};
	pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_ci.queueFamilyIndex = graphics_queue_idx;

	VkCommandBufferAllocateInfo cmd_ai {};
	cmd_ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmd_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmd_ai.commandBufferCount = 1;

	VkFenceCreateInfo fence_ci {};
	fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkSemaphoreCreateInfo sema_ci {};
	sema_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	for (i = 0; i < N; i++) {
		result = vkCreateCommandPool (device,
			&pool_ci, nullptr, &command_pools[i]);
		if (result != VK_SUCCESS)
			goto fail;
		cmd_ai.commandPool = command_pools[i];
		result = vkAllocateCommandBuffers (device,
			&cmd_ai, &command_buffers[i]);
		if (result != VK_SUCCESS) {
			vkDestroyCommandPool (device, command_pools[i], nullptr);
			goto fail;
		}
		result = vkCreateFence (device,
			&fence_ci, nullptr, &acquire_fences[i]);
		if (result != VK_SUCCESS) {
			vkDestroyCommandPool (device, command_pools[i], nullptr);
			goto fail;
		}
		result = vkCreateSemaphore (device,
			&sema_ci, nullptr, &acquire_semaphores[i]);
		if (result != VK_SUCCESS) {
			vkDestroyFence (device, acquire_fences[i], nullptr);
			vkDestroyCommandPool (device, command_pools[i], nullptr);
			goto fail;
		}
		result = vkCreateSemaphore (device,
			&sema_ci, nullptr, &present_semaphores[i]);
		if (result != VK_SUCCESS) {
			vkDestroySemaphore (device, acquire_semaphores[i], nullptr);
			vkDestroyFence (device, acquire_fences[i], nullptr);
			vkDestroyCommandPool (device, command_pools[i], nullptr);
			goto fail;
		}
	}

	return true;
fail:
	while (i) {
		i--;
		vkDestroySemaphore (device, present_semaphores[i], nullptr);
		vkDestroySemaphore (device, acquire_semaphores[i], nullptr);
		vkDestroyFence (device, acquire_fences[i], nullptr);
		vkDestroyCommandPool (device, command_pools[i], nullptr);
	}
	return false;
}

static void
teardown_command_resources (void)
{
	int i = N;
	while (i) {
		i--;
		vkDestroySemaphore (device, present_semaphores[i], nullptr);
		vkDestroySemaphore (device, acquire_semaphores[i], nullptr);
		vkDestroyFence (device, acquire_fences[i], nullptr);
		vkDestroyCommandPool (device, command_pools[i], nullptr);
	}
}

static bool
setup_rendering_resources (void)
{
	VkResult result;

	VkAttachmentDescription ad {};
	ad.format = swapchain_format;
	ad.samples = VK_SAMPLE_COUNT_1_BIT;
	ad.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	ad.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	ad.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	ad.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	ad.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ad.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference ar {};
	ar.attachment = 0;
	ar.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription sd {};
	sd.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sd.colorAttachmentCount = 1;
	sd.pColorAttachments = &ar;

	VkRenderPassCreateInfo rp_info {};
	rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rp_info.attachmentCount = 1;
	rp_info.pAttachments = &ad;
	rp_info.subpassCount = 1;
	rp_info.pSubpasses = &sd;

	result = vkCreateRenderPass (device, &rp_info, nullptr, &render_pass);
	if (result != VK_SUCCESS)
		return false;

	VkPipelineLayoutCreateInfo layout_ci {};
	layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	result = vkCreatePipelineLayout (device, &layout_ci, nullptr, &pipeline_layout);
	if (result != VK_SUCCESS) {
		vkDestroyRenderPass (device, render_pass, nullptr);
		return false;
	}

	VkShaderModule frag, vert;
	VkShaderModuleCreateInfo shader_ci {};
	shader_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

	shader_ci.codeSize = sizeof (example_frag);
	shader_ci.pCode = example_frag;
	result = vkCreateShaderModule (device, &shader_ci, nullptr, &frag);
	if (result != VK_SUCCESS) {
		vkDestroyPipelineLayout (device, pipeline_layout, nullptr);
		vkDestroyRenderPass (device, render_pass, nullptr);
		return false;
	}

	shader_ci.codeSize = sizeof (example_vert);
	shader_ci.pCode = example_vert;
	result = vkCreateShaderModule (device, &shader_ci, nullptr, &vert);
	if (result != VK_SUCCESS) {
		vkDestroyShaderModule (device, frag, nullptr);
		vkDestroyPipelineLayout (device, pipeline_layout, nullptr);
		vkDestroyRenderPass (device, render_pass, nullptr);
		return false;
	}

	VkPipelineShaderStageCreateInfo shader_stages[2] {};
	shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stages[0].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shader_stages[0].module = frag;
	shader_stages[0].pName = "main";
	shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stages[1].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shader_stages[1].module = vert;
	shader_stages[1].pName = "main";

	VkPipelineVertexInputStateCreateInfo vertex_input_ci {};
	vertex_input_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkPipelineInputAssemblyStateCreateInfo input_asm_ci {};
	input_asm_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_asm_ci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineViewportStateCreateInfo viewport_ci {};
	viewport_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_ci.viewportCount = 1;
	viewport_ci.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo raster_ci {};
	raster_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	raster_ci.cullMode = VK_CULL_MODE_BACK_BIT;
	raster_ci.frontFace = VK_FRONT_FACE_CLOCKWISE;
	raster_ci.depthBiasConstantFactor = 0.0f;
	raster_ci.depthBiasClamp = 0.0f;
	raster_ci.depthBiasSlopeFactor = 0.0f;
	raster_ci.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisample_ci {};
	multisample_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample_ci.minSampleShading = 1.0f;

	VkPipelineColorBlendAttachmentState color_blend {};
	color_blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend.alphaBlendOp = VK_BLEND_OP_ADD;
	color_blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo color_blend_ci {};
	color_blend_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend_ci.logicOp = VK_LOGIC_OP_COPY;
	color_blend_ci.attachmentCount = 1;
	color_blend_ci.pAttachments = &color_blend;
	color_blend_ci.blendConstants[0] = 0.0f;
	color_blend_ci.blendConstants[1] = 0.0f;
	color_blend_ci.blendConstants[2] = 0.0f;
	color_blend_ci.blendConstants[3] = 0.0f;

	VkDynamicState dynamic_states[2] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamic_state_ci {};
	dynamic_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state_ci.dynamicStateCount = 2;
	dynamic_state_ci.pDynamicStates = dynamic_states;

	VkGraphicsPipelineCreateInfo gp_ci {};
	gp_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gp_ci.stageCount = 2;
	gp_ci.pStages = shader_stages;
	gp_ci.pVertexInputState = &vertex_input_ci;
	gp_ci.pInputAssemblyState = &input_asm_ci;
	gp_ci.pViewportState = &viewport_ci;
	gp_ci.pRasterizationState = &raster_ci;
	gp_ci.pMultisampleState = &multisample_ci;
	gp_ci.pColorBlendState = &color_blend_ci;
	gp_ci.pDynamicState = &dynamic_state_ci;
	gp_ci.layout = pipeline_layout;
	gp_ci.renderPass = render_pass;
	gp_ci.basePipelineIndex = -1;
	result = vkCreateGraphicsPipelines (device, VK_NULL_HANDLE, 1,
		&gp_ci, nullptr, &pipeline);

	vkDestroyShaderModule (device, vert, nullptr);
	vkDestroyShaderModule (device, frag, nullptr);
	if (result != VK_SUCCESS) {
		vkDestroyPipelineLayout (device, pipeline_layout, nullptr);
		vkDestroyRenderPass (device, render_pass, nullptr);
		return false;
	}

	return true;
}

static void
teardown_rendering_resources (void)
{
	vkDestroyPipeline (device, pipeline, nullptr);
	vkDestroyPipelineLayout (device, pipeline_layout, nullptr);
	vkDestroyRenderPass (device, render_pass, nullptr);
}

static void
teardown_pfd (void)
{
	for (uint32_t i = 0; i < swapchain_size; i++) {
		vkDestroyFramebuffer (device, framebuffers[i], nullptr);
		vkDestroyImageView (device, views[i], nullptr);
	}
	delete[] framebuffers;
	delete[] views;
	delete[] images;
}

static bool
create_swapchain (void)
{
	VkResult result;

	VkSurfaceCapabilitiesKHR caps {};
	result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR (physical_device,
		surface, &caps);
	if (result != VK_SUCCESS)
		return false;

	VkExtent2D new_size = vkfwGetFramebufferExtent (window);
	if (caps.minImageExtent.width > new_size.width)
		new_size.width = caps.minImageExtent.width;
	else if (caps.maxImageExtent.width < new_size.width)
		new_size.width = caps.maxImageExtent.width;
	if (caps.minImageExtent.height > new_size.height)
		new_size.height = caps.minImageExtent.height;
	else if (caps.maxImageExtent.height < new_size.height)
		new_size.height = caps.maxImageExtent.height;

	framebuffer_size = new_size;
	if (!new_size.width || !new_size.height) {
		swapchain_dirty = false;
		return true;
	}

	uint32_t queues[2] = { graphics_queue_idx, present_queue_idx };

	VkSwapchainCreateInfoKHR ci {};
	ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	ci.surface = surface;
	ci.minImageCount = N;
	if (caps.minImageCount > N)
		ci.minImageCount = caps.minImageCount;
	else if (caps.maxImageCount && caps.maxImageCount < N)
		ci.minImageCount = caps.maxImageCount;
	ci.imageFormat = swapchain_format;
	ci.imageColorSpace = swapchain_colorspace;
	ci.imageExtent = new_size;
	ci.imageArrayLayers = 1;
	ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if (graphics_queue_idx == present_queue_idx) {
		ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		ci.queueFamilyIndexCount = 1;
	} else {
		ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		ci.queueFamilyIndexCount = 2;
	}
	ci.pQueueFamilyIndices = queues;
	ci.preTransform = caps.currentTransform;
	ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	ci.presentMode = swapchain_present_mode;
	ci.clipped = VK_FALSE;
	ci.oldSwapchain = swapchain;

	VkSwapchainKHR new_swapchain;
	result = vkCreateSwapchainKHR (device, &ci, nullptr, &new_swapchain);
	if (result != VK_SUCCESS)
		return false;

	if (swapchain) {
		vkDeviceWaitIdle (device);
		teardown_pfd ();
		vkDestroySwapchainKHR (device, swapchain, nullptr);
		swapchain = VK_NULL_HANDLE;
	}

	uint32_t count = 0;
	result = vkGetSwapchainImagesKHR (device, new_swapchain, &count, nullptr);
	if (result != VK_SUCCESS) {
		vkDestroySwapchainKHR (device, new_swapchain, nullptr);
		return false;
	}

	VkImage *new_images = new (std::nothrow) VkImage[count];
	if (!new_images)
		return false;

	result = vkGetSwapchainImagesKHR (device, new_swapchain, &count, new_images);
	if (result != VK_SUCCESS) {
		delete[] new_images;
		vkDestroySwapchainKHR (device, new_swapchain, nullptr);
		return false;
	}

	VkImageView *new_views = new (std::nothrow) VkImageView[count];
	if (!new_views) {
		delete[] new_images;
		vkDestroySwapchainKHR (device, new_swapchain, nullptr);
		return false;
	}

	VkImageViewCreateInfo view_ci {};
	view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_ci.format = swapchain_format;
	view_ci.subresourceRange = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.baseMipLevel = 0,
		.levelCount = 1,
		.baseArrayLayer = 0,
		.layerCount = 1
	};

	for (uint32_t i = 0; i < count; i++) {
		view_ci.image = new_images[i];
		result = vkCreateImageView (device, &view_ci, nullptr, &new_views[i]);
		if (result != VK_SUCCESS) {
			while (i)
				vkDestroyImageView (device, new_views[--i], nullptr);
			delete[] new_views;
			delete[] new_images;
			vkDestroySwapchainKHR (device, new_swapchain, nullptr);
			return false;
		}
	}

	VkFramebuffer *new_framebuffers = new (std::nothrow) VkFramebuffer[count];
	if (!new_framebuffers) {
		for (uint32_t i = 0; i < count; i++)
			vkDestroyImageView (device, new_views[i], nullptr);
		delete[] new_views;
		delete[] new_images;
		vkDestroySwapchainKHR (device, new_swapchain, nullptr);
		return false;
	}

	VkFramebufferCreateInfo fb_ci {};
	fb_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fb_ci.renderPass = render_pass;
	fb_ci.attachmentCount = 1;
	fb_ci.width = new_size.width;
	fb_ci.height = new_size.height;
	fb_ci.layers = 1;
	for (uint32_t i = 0; i < count; i++) {
		fb_ci.pAttachments = &new_views[i];
		result = vkCreateFramebuffer (device, &fb_ci, nullptr, &new_framebuffers[i]);
		if (result != VK_SUCCESS) {
			while (i)
				vkDestroyFramebuffer (device, new_framebuffers[--i], nullptr);
			for (; i < count; i++)
				vkDestroyImageView (device, new_views[i], nullptr);
			delete[] new_framebuffers;
			delete[] new_views;
			delete[] new_images;
			vkDestroySwapchainKHR (device, new_swapchain, nullptr);
			return false;
		}
	}

	swapchain = new_swapchain;
	images = new_images;
	views = new_views;
	framebuffers = new_framebuffers;
	swapchain_size = count;
	swapchain_dirty = false;
	return true;
}

static bool
setup_everything (void)
{
	if (!setup ())
		return false;
	if (!setup_command_resources ()) {
		teardown ();
		return false;
	}
	if (!setup_rendering_resources ()) {
		teardown_command_resources ();
		teardown ();
		return false;
	}
	if (!create_swapchain ()) {
		teardown_rendering_resources ();
		teardown_command_resources ();
		teardown ();
		return false;
	}
	return true;
}

static void
teardown_everything (void)
{
	if (swapchain) {
		vkDeviceWaitIdle (device);
		teardown_pfd ();
		vkDestroySwapchainKHR (device, swapchain, nullptr);
	}
	teardown_rendering_resources ();
	teardown_command_resources ();
	teardown ();
}

static VkResult
draw (void)
{
	VkResult result;
	uint32_t i;
	int fi = frame_index;

	if (!framebuffer_size.width || !framebuffer_size.height)
		return VK_SUCCESS;

	result = vkWaitForFences (device, 1, &acquire_fences[fi], VK_TRUE, UINT64_MAX);
	if (result != VK_SUCCESS)
		return result;

	result = vkAcquireNextImageKHR (device, swapchain, UINT64_MAX,
		acquire_semaphores[fi], VK_NULL_HANDLE, &i);
	switch (result) {
	case VK_SUCCESS:
		break;
	case VK_SUBOPTIMAL_KHR:
		swapchain_dirty = true;
		break;
	case VK_ERROR_OUT_OF_DATE_KHR:
		swapchain_dirty = true;
		return VK_SUCCESS;
	default:
		return result;
	}

	frame_index++;
	if (frame_index >= N)
		frame_index = 0;

	result = vkResetCommandPool (device, command_pools[fi], 0);
	if (result != VK_SUCCESS)
		return result;

	VkCommandBufferBeginInfo begin_info {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	result = vkBeginCommandBuffer (command_buffers[fi], &begin_info);
	if (result != VK_SUCCESS)
		return result;

	VkClearValue cv = {
		.color = {
			.float32 = { 0.0f, 0.0f, 0.0f, 1.0f }
		}
	};

	VkRenderPassBeginInfo rp_begin_info {};
	rp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rp_begin_info.renderPass = render_pass;
	rp_begin_info.framebuffer = framebuffers[i];
	rp_begin_info.renderArea.offset = { 0, 0 };
	rp_begin_info.renderArea.extent = framebuffer_size;
	rp_begin_info.clearValueCount = 1;
	rp_begin_info.pClearValues = &cv;
	vkCmdBeginRenderPass (command_buffers[fi],
		&rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline (command_buffers[fi],
		VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	VkViewport viewport;
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = (float) framebuffer_size.width;
	viewport.height = (float) framebuffer_size.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor;
	scissor.offset = { 0, 0 };
	scissor.extent = framebuffer_size;

	vkCmdSetViewport (command_buffers[fi], 0, 1, &viewport);
	vkCmdSetScissor (command_buffers[fi], 0, 1, &scissor);

	vkCmdDraw (command_buffers[fi], 3, 1, 0, 0);

	vkCmdEndRenderPass (command_buffers[fi]);

	result = vkEndCommandBuffer (command_buffers[fi]);
	if (result != VK_SUCCESS)
		return result;

	VkPipelineStageFlags psf = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	result = vkResetFences (device, 1, &acquire_fences[fi]);
	if (result != VK_SUCCESS)
		return result;

	VkSubmitInfo si {};
	si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	si.waitSemaphoreCount = 1;
	si.pWaitSemaphores = &acquire_semaphores[fi];
	si.pWaitDstStageMask = &psf;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &command_buffers[fi];
	si.signalSemaphoreCount = 1;
	si.pSignalSemaphores = &present_semaphores[fi];
	result = vkQueueSubmit (graphics_queue, 1, &si, acquire_fences[fi]);
	if (result != VK_SUCCESS)
		return result;

	VkPresentInfoKHR pi {};
	pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	pi.waitSemaphoreCount = 1;
	pi.pWaitSemaphores = &present_semaphores[fi];
	pi.swapchainCount = 1;
	pi.pSwapchains = &swapchain;
	pi.pImageIndices = &i;
	result = vkQueuePresentKHR (present_queue, &pi);
	if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR) {
		swapchain_dirty = true;
		return VK_SUCCESS;
	} else
		return result;
}

/**
 * Quick and dirty UTF-8 encoding for printing codepoints.
 */
static bool
encode_utf8 (char *buf, uint32_t codepoint)
{
	for (int i = 0; i < 5; i++)
		buf[i] = 0;

	if (codepoint < 0x80) {
		buf[0] = codepoint;
	} else if (codepoint < 0x800) {
		buf[0] = (codepoint >> 6) | 0xc0;
		buf[1] = (codepoint & 0x3f) | 0x80;
	} else if (codepoint < 0x10000) {
		buf[0] = (codepoint >> 12) | 0xe0;
		buf[1] = ((codepoint >> 6) & 0x3f) | 0x80;
		buf[2] = (codepoint & 0x3f) | 0x80;
	} else if (codepoint < 0x110000) {
		buf[0] = (codepoint >> 18) | 0xf0;
		buf[1] = ((codepoint >> 12) & 0x3f) | 0x80;
		buf[2] = ((codepoint >> 6) & 0x3f) | 0x80;
		buf[3] = (codepoint & 0x3f) | 0x80;
	} else
		return false;

	return true;
}

int
main (void)
{
	VkResult result;
	VKFWevent e;
	char buf[5];

	if (!setup_everything ())
		return 1;

	vkfwEnableTextInput (window);

	for (;;) {
		result = vkfwGetNextEvent (&e);
		if (result != VK_SUCCESS) {
			teardown_everything ();
			return 1;
		}

		switch (e.type) {
		case VKFW_EVENT_NONE:
			if (swapchain_dirty)
				if (!create_swapchain ()) {
					teardown_everything ();
					return 1;
				}
			result = draw ();
			if (result != VK_SUCCESS && result != VK_ERROR_OUT_OF_DATE_KHR) {
				teardown_everything ();
				return 1;
			}
			break;
		case VKFW_EVENT_WINDOW_CLOSE_REQUEST:
			teardown_everything ();
			return 0;
		case VKFW_EVENT_WINDOW_RESIZE_NOTIFY:
			swapchain_dirty = true;
			break;
		case VKFW_EVENT_KEY_PRESSED:
			if (e.key > 0 && e.key <= 255)
				printf ("key %d was pressed ('%c')\n", e.keycode, (char) e.key);
			else
				printf ("key %d was pressed (%d)\n", e.keycode, e.key);
			break;
		case VKFW_EVENT_KEY_RELEASED:
			if (e.key > 0 && e.key < 255)
				printf ("key %d was released ('%c')\n", e.keycode, (char) e.key);
			else
				printf ("key %d was released (%d)\n", e.keycode, e.key);
			break;
		case VKFW_EVENT_TEXT_INPUT:
			if (e.codepoint < 0x20 || (e.codepoint >= 0x7f && e.codepoint < 0xa0))
				printf ("text input U+%04X\n", e.codepoint);
			else if (encode_utf8 (buf, e.codepoint))
				printf ("text input '%s'\n", buf);
			else
				printf ("text input U+%04X (encoding failed)\n", e.codepoint);
			break;
		default:
			vkfwUnhandledEvent (&e);
			break;
		}
	}
}

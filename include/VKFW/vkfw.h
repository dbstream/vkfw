/**
 * VKFW is a framework for window creation, event handling, and Vulkan graphics.
 * Copyright (C) 2024  dbstream
 *
 * This is the only VKFW header that an application normally needs to include.
 */
#ifndef VKFW_H
#define VKFW_H 1

/**
 * note: programs which link dynamically to VKFW must define VKFW_DLL before
 * including VKFW/vkfw.h
 */

#define VKFW_VERSION_MAJOR 2
#define VKFW_VERSION_MINOR 2
#define VKFW_VERSION_PATCH 0

#define VKFW_VERSION VK_MAKE_API_VERSION(0, VKFW_VERSION_MAJOR, VKFW_VERSION_MINOR, VKFW_VERSION_PATCH)

#ifdef VKFW_DLL
#ifdef VKFW_BUILDING
#ifdef _WIN32
#define VKFWAPI __declspec (dllexport)
#else
#define VKFWAPI __attribute__ ((__visibility__ ("default")))
#endif
#else
#ifdef _WIN32
#define VKFWAPI __declspec (dllimport)
#endif
#endif
#endif

#ifndef VKFWAPI
#define VKFWAPI
#endif

#define VK_NO_PROTOTYPES 1

#include <vulkan/vulkan.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <VKFW/vk_functions.h>

typedef struct VKFWevent_T VKFWevent;
typedef struct VKFWwindow_T VKFWwindow;

/**
 * note that there is a difference between EVENT_NONE and EVENT_NULL; whereas
 * EVENT_NULL indicates that an empty event was delivered from the platform,
 * EVENT_NONE indicates that no event at all was delivered.
 *
 * The purpose of EVENT_NULL is for VKFW to handle certain events sent by the
 * window manager internally. For example, this is used by the X11/Xcb backend
 * to handle ReparentNotify events, which are sent by the X11 server but don't
 * correspond to any VKFW event.
 *
 * Event loops should generally look something like this:
 *	vkfwWaitNextEventUntil (&e, next_frame);
 *	while (e.type != VKFW_EVENT_NONE) {
 *		handle_event (&e);
 *		vkfwWaitNextEventUntil (&e, next_frame);
 *	}
 */

/**
 * VKFW time is in microseconds. These macros make time values more readable.
 */
#define VKFW_SECONDS 1000000
#define VKFW_MILLIS 1000
#define VKFW_MICROS 1

#define VKFW_LEFT_MOUSE_BUTTON 1
#define VKFW_RIGHT_MOUSE_BUTTON 2
#define VKFW_SCROLL_WHEEL_BUTTON 3

#define VKFW_SCROLL_VERTICAL 0
#define VKFW_SCROLL_HORIZONTAL 1

/**
 * Pointer mode configuration bits. See vkfwSetPointerMode.
 */
#define VKFW_POINTER_NORMAL 0U
#define VKFW_POINTER_HIDDEN 1U
#define VKFW_POINTER_CONFINED 2U
#define VKFW_POINTER_GRABBED 4U
#define VKFW_POINTER_RELATIVE 8U

/**
 * Notes on VKFW_KEY_*:
 * - VKFW_KEY_UNKNOWN means that a platform keycode doesn't correspond to a
 *   VKFW key assignment.
 * - numbers and letters should correspond to printable ASCII whenever
 *   possible.
 * - anything which is not printable should be above 255.
 */

#define VKFW_KEY_UNKNOWN -1

#define VKFW_KEY_SPACE 32
#define VKFW_KEY_0 48
#define VKFW_KEY_1 49
#define VKFW_KEY_2 50
#define VKFW_KEY_3 51
#define VKFW_KEY_4 52
#define VKFW_KEY_5 53
#define VKFW_KEY_6 54
#define VKFW_KEY_7 55
#define VKFW_KEY_8 56
#define VKFW_KEY_9 57
#define VKFW_KEY_A 65
#define VKFW_KEY_B 66
#define VKFW_KEY_C 67
#define VKFW_KEY_D 68
#define VKFW_KEY_E 69
#define VKFW_KEY_F 70
#define VKFW_KEY_G 71
#define VKFW_KEY_H 72
#define VKFW_KEY_I 73
#define VKFW_KEY_J 74
#define VKFW_KEY_K 75
#define VKFW_KEY_L 76
#define VKFW_KEY_M 77
#define VKFW_KEY_N 78
#define VKFW_KEY_O 79
#define VKFW_KEY_P 80
#define VKFW_KEY_Q 81
#define VKFW_KEY_R 82
#define VKFW_KEY_S 83
#define VKFW_KEY_T 84
#define VKFW_KEY_U 85
#define VKFW_KEY_V 86
#define VKFW_KEY_W 87
#define VKFW_KEY_X 88
#define VKFW_KEY_Y 89
#define VKFW_KEY_Z 90
#define VKFW_KEY_BACKSPACE 256
#define VKFW_KEY_LEFT_CTRL 257
#define VKFW_KEY_LEFT_SHIFT 258
#define VKFW_KEY_LEFT_ALT 259
#define VKFW_KEY_RIGHT_CTRL 260
#define VKFW_KEY_RIGHT_SHIFT 261
#define VKFW_KEY_RIGHT_ALT 262
#define VKFW_KEY_F1 263
#define VKFW_KEY_F2 264
#define VKFW_KEY_F3 265
#define VKFW_KEY_F4 266
#define VKFW_KEY_F5 267
#define VKFW_KEY_F6 268
#define VKFW_KEY_F7 269
#define VKFW_KEY_F8 270
#define VKFW_KEY_F9 271
#define VKFW_KEY_F10 272
#define VKFW_KEY_F11 273
#define VKFW_KEY_F12 274
#define VKFW_KEY_F13 275
#define VKFW_KEY_F14 276
#define VKFW_KEY_F15 277
#define VKFW_KEY_F16 278
#define VKFW_KEY_F17 279
#define VKFW_KEY_F18 280
#define VKFW_KEY_F19 281
#define VKFW_KEY_F20 282
#define VKFW_KEY_F21 283
#define VKFW_KEY_F22 284
#define VKFW_KEY_F23 285
#define VKFW_KEY_F24 286
#define VKFW_KEY_F25 287
#define VKFW_KEY_ESC 288
#define VKFW_KEY_DEL 289
#define VKFW_KEY_INSERT 290
#define VKFW_KEY_HOME 291
#define VKFW_KEY_END 292
#define VKFW_KEY_PG_UP 293
#define VKFW_KEY_PG_DOWN 294
#define VKFW_KEY_NUMPAD_DIVIDE 295
#define VKFW_KEY_NUMPAD_MULTIPLY 296
#define VKFW_KEY_NUMPAD_SUBTRACT 297
#define VKFW_KEY_NUMPAD_ADD 298
#define VKFW_KEY_NUMPAD_ENTER 299
#define VKFW_KEY_NUMPAD_0 300
#define VKFW_KEY_NUMPAD_1 301
#define VKFW_KEY_NUMPAD_2 302
#define VKFW_KEY_NUMPAD_3 303
#define VKFW_KEY_NUMPAD_4 304
#define VKFW_KEY_NUMPAD_5 305
#define VKFW_KEY_NUMPAD_6 306
#define VKFW_KEY_NUMPAD_7 307
#define VKFW_KEY_NUMPAD_8 308
#define VKFW_KEY_NUMPAD_9 309
#define VKFW_KEY_ARROW_LEFT 310
#define VKFW_KEY_ARROW_RIGHT 311
#define VKFW_KEY_ARROW_UP 312
#define VKFW_KEY_ARROW_DOWN 313
#define VKFW_KEY_NUMPAD_COMMA 314
#define VKFW_MAX_KEYS 512

#define VKFW_MODIFIER_CTRL 1U
#define VKFW_MODIFIER_SHIFT 2U
#define VKFW_MODIFIER_LEFT_ALT 4U
#define VKFW_MODIFIER_RIGHT_ALT 8U
#define VKFW_MODIFIER_CAPS_LOCK 16U
#define VKFW_MODIFIER_NUM_LOCK 32U

#define VKFW_EVENT_NONE 0
#define VKFW_EVENT_NULL 1
#define VKFW_EVENT_WINDOW_CLOSE_REQUEST 2
#define VKFW_EVENT_WINDOW_RESIZE_NOTIFY 3
#define VKFW_EVENT_WINDOW_LOST_FOCUS 4
#define VKFW_EVENT_WINDOW_GAINED_FOCUS 5
#define VKFW_EVENT_POINTER_MOTION 6
#define VKFW_EVENT_BUTTON_PRESSED 7
#define VKFW_EVENT_BUTTON_RELEASED 8
#define VKFW_EVENT_SCROLL 9
#define VKFW_EVENT_KEY_PRESSED 10
#define VKFW_EVENT_KEY_RELEASED 11
#define VKFW_EVENT_TEXT_INPUT 12
#define VKFW_EVENT_RELATIVE_POINTER_MOTION 13

/**
 * VKFW event structure. Adding or removing fields in this struct is an
 * API-breaking change and must increment the major revision number.
 */
struct VKFWevent_T {
	/**
	 * Type of the event that is delivered to the application.
	 */
	int type;

	/**
	 * If the event references a window, this is a pointer to that window.
	 */
	VKFWwindow *window;

	union {
		/**
		 * VKFW_EVENT_WINDOW_RESIZE_NOFITY:
		 *   New size of the window.
		 */
		VkExtent2D extent;

		/**
		 * VKFW_EVENT_POINTER_MOTION:
		 *   New position of the pointer within the window.
		 *
		 * VKFW_EVENT_BUTTON_PRESSED, VKFW_EVENT_BUTTON_RELEASED,
		 * VKFW_EVENT_KEY_PRESSED, VKFW_EVENT_KEY_RELEASED,
		 * VKFW_EVENT_SCROLL, VKFW_EVENT_TEXT_INPUT:
		 *   Pointer location of input.
		 *
		 * VKFW_EVENT_RELATIVE_POINTER_MOTION:
		 *   Change in pointer position.
		 */
		struct {
			int x, y;
		};
	};

	union {
		/**
		 * VKFW_EVENT_BUTTON_PRESSED, VKFW_EVENT_BUTTON_RELEASED:
		 *    Which button was pressed/released.
		 *    1 - left mouse button
		 *    2 - right mouse button
		 *    3 - scroll wheel
		 *    4...N - additional mouse buttons
		 */
		int button;

		/**
		 * VKFW_EVENT_KEY_PRESSED, VKFW_EVENT_KEY_RELEASED:
		 *   Which key was pressed/released.
		 *
		 *   key is a VKFW_KEY_*
		 *   keycode is the underlying platform keycode
		 *
		 *   Games should use keycode for key bindings.
		 */
		struct {
			int key, keycode;
		};

		/**
		 * VKFW_EVENT_TEXT_INPUT:
		 *   Unicode codepoint of input. This can be backspace, delete,
		 *   or other control codes, which should be handled by the
		 *   application.
		 */
		uint32_t codepoint;

		/**
		 * VKFW_EVENT_SCROLL:
		 *   Direction and distance of scrolling. Note that not all mice
		 *   support horizontal scrolling.
		 *
		 * FIXME: what directions do positive and negative scroll_value
		 * correspond to?
		 */
		struct {
			int scroll_direction, scroll_value;
		};
	};

	/**
	 * VKFW_EVENT_BUTTON_PRESSED, VKFW_EVENT_BUTTON_RELEASED,
	 * VKFW_EVENT_KEY_PRESSED, VKFW_EVENT_KEY_RELEASED,
	 * VKFW_EVENT_TEXT_INPUT, VKFW_EVENT_SCROLL:
	 *   A bitmask of active modifiers when the event occured.
	 */
	unsigned int modifiers;
};

	/* Initialization */

#define VKFW_LOG_CORE 0
#define VKFW_LOG_PLATFORM 1
#define VKFW_LOG_BACKEND 2
#define VKFW_LOG_NUM 3
#define VKFW_LOG_NONE 1001
#define VKFW_LOG_ALL 1002

/**
 * Enable debug logging to stdout within VKFW.
 */
VKFWAPI void
vkfwEnableDebugLogging (int source);

/**
 * Initialize VKFW and load the Vulkan loader. version is the VKFW header
 * version the application was compiled against.
 */
VKFWAPI VkResult
vkfwInitVersion (uint32_t version);

/**
 * Helper macro to correctly call vkfwInitVersion.
 */
#define vkfwInit() vkfwInitVersion (VKFW_VERSION)

/**
 * Terminate VKFW. If an instance was created with vkfwCreateInstance, it is
 * destroyed. If a device was created with vkfwCreateDevice, it is destroyed.
 */
VKFWAPI void
vkfwTerminate (void);

	/* Vulkan */

/**
 * Wrapper around vkEnumerateInstanceVersion.
 */
VKFWAPI uint32_t
vkfwGetVkInstanceVersion (void);

/**
 * Request an instance extension. If required is true, instance creation will
 * fail unless the extension is available.
 */
VKFWAPI VkResult
vkfwRequestInstanceExtension (const char *extension_name, bool required);

/**
 * Request a device extension. If required is true, device creation will fail
 * unless the extension is available.
 */
VKFWAPI VkResult
vkfwRequestDeviceExtension (const char *extension_name, bool required);

/**
 * Request a layer. If required is true, instance creation will fail unless the
 * layer is available.
 */
VKFWAPI VkResult
vkfwRequestLayer (const char *layer_name, bool required);

/**
 * Create the Vulkan instance and load instance functions.
 * note: extensions and layers are filled in by VKFW.
 *
 * flags is not VkInstanceCreateFlags, but a bitmask of VKFW_CREATE_INSTANCE_*
 * that can toggle various VKFW behaviors.
 */
VKFWAPI VkResult
vkfwCreateInstance (VkInstance *out, const VkInstanceCreateInfo *ci,
	uint32_t flags);

#define VKFW_CREATE_INSTANCE_DEBUG_MESSENGER 1

/**
 * Create the Vulkan device and load device functions.
 * note: extensions and layers are filled in by VKFW.
 */
VKFWAPI VkResult
vkfwCreateDevice (VkDevice *out, VkPhysicalDevice physical_device,
	const VkDeviceCreateInfo *ci);

/**
 * Select and create the Vulkan device based on what VKFW deems most suitable.
 *
 * If features is non-NULL, it is a pointer to a VkPhysicalDeviceFeatures2
 * describing required device features. If a _queue is non-NULL, it will be
 * filled in with a queue of the corresponding type.
 *
 * note: sophisticated applications should implement their own device selection
 * routine, as this one is very limited.
 */
VKFWAPI VkResult
vkfwAutoCreateDevice (VkDevice *out, VkPhysicalDevice *out_physical,
	const VkPhysicalDeviceFeatures2 *features,
	uint32_t *graphics_queue, uint32_t *compute_queue,
	uint32_t *present_queue, uint32_t *transfer_queue);

VKFWAPI VkResult
vkfwGetPhysicalDevicePresentSupport (VkPhysicalDevice device, uint32_t queue,
	VkBool32 *out);

	/* Window management */

/**
 * Create a VKFW window. The window will not be shown until vkfwShowWindow is
 * called.
 */
VKFWAPI VkResult
vkfwCreateWindow (VKFWwindow **handle, VkExtent2D size);

/**
 * Create a VkSurfaceKHR for a window.
 */
VKFWAPI VkResult
vkfwCreateSurface (VKFWwindow *handle, VkSurfaceKHR *out);

/**
 * Destroy a VKFW window.
 */
VKFWAPI void
vkfwDestroyWindow (VKFWwindow *handle);

/**
 * Set a user pointer in the VKFWwindow. Returns the previous value of
 * the pointer.
 */
VKFWAPI void *
vkfwSetWindowUserPointer (VKFWwindow *handle, void *user);

/**
 * Get a user pointer from the VKFWwindow.
 */
VKFWAPI void *
vkfwGetWindowUserPointer (VKFWwindow *handle);

/**
 * Set the window title.
 */
VKFWAPI VkResult
vkfwSetWindowTitle (VKFWwindow *handle, const char *title);

/**
 * Show a VKFW window.
 */
VKFWAPI VkResult
vkfwShowWindow (VKFWwindow *handle);

/**
 * Hide a VKFW window.
 */
VKFWAPI VkResult
vkfwHideWindow (VKFWwindow *handle);

/**
 * Get the size of the framebuffer for a window.
 * note: it is unnecessary to call this after a WINDOW_RESIZE_NOTIFY; instead,
 * use e->extent.
 */
VKFWAPI VkExtent2D
vkfwGetFramebufferExtent (VKFWwindow *handle);

/**
 * Set the pointer mode for a window.
 * 'mode' is a bitmask of VKFW_POINTER_*, which have the following effects:
 *   VKFW_POINTER_HIDDEN     cursor will not be rendered
 *   VKFW_POINTER_CONFINED   cursor cannot leave window
 *   VKFW_POINTER_GRABBED    cursor events will be received even if the pointer
 *                           leaves the window
 *   VKFW_POINTER_RELATIVE   VKFW_EVENT_RELATIVE_POINTER_MOTION will be
 *                           generated instead of VKFW_EVENT_POINTER_MOTION
 */
VKFWAPI void
vkfwSetPointerMode (VKFWwindow *handle, unsigned int mode);

	/* Input */

/**
 * Convert a platform keycode into a VKFW key. Returns VKFW_KEY_UNKNOWN if the
 * translation could not be determined or if the keycode does not correspond to
 * a VKFW key.
 */
VKFWAPI int
vkfwTranslateKeycode (int keycode);

/**
 * Convert a VKFW key into a platform keycode. Returns VKFW_KEY_UNKNOWN if the
 * translation could not be determined or if the key does not have a keycode in
 * the current keyboard layout.
 */
VKFWAPI int
vkfwTranslateKey (int key);

/**
 * Note on text input:
 *   A game may have a keybind for opening the chat, for example 'T'. Now, when
 *   the user presses 'T', first the VKFW_EVENT_KEY_PRESSED event will be sent,
 *   then the VKFW_EVENT_TEXT_INPUT event will be sent. This will cause the
 *   letter 't' to be entered immediately upon the chat dialog opening.
 *
 *   To avoid this undesirable behavior, VKFW_EVENT_TEXT_INPUT events are only
 *   generated in between calls to the functions vkfwEnableTextInput and
 *   vkfwDisableTextInput When the application has opened up the text dialog,
 *   it should call vkfwEnableTextInput to enable the generation of
 *   VKFW_EVENT_TEXT_INPUT. After the text dialog is closed, the application
 *   should call vkfwDisableTextInput.
 *
 *   The default state for text input is disabled.
 *
 *   If the application wishes to support pasting text using a shortcut, for
 *   example C-v, the correct behavior is to call vkfwDisableTextInput followed
 *   by calling vkfwEnableTextInput immediately. This is necessary as otherwise
 *   VKFW can generate text input for the key combination.
 */

/**
 * Enable the generation of VKFW_EVENT_TEXT_INPUT events.
 */
VKFWAPI void
vkfwEnableTextInput (VKFWwindow *window);

/**
 * Disable the generation of VKFW_EVENT_TEXT_INPUT events.
 */
VKFWAPI void
vkfwDisableTextInput (VKFWwindow *window);

	/* Events */

/**
 * This function is required to be called when an application doesn't handle an
 * event.
 */
VKFWAPI void
vkfwUnhandledEvent (VKFWevent *e);

/**
 * Poll for pending events. Returns VKFW_EVENT_* in *e if an event was delivered
 * to the application, otherwise VKFW_EVENT_NULL for no event.
 */
VKFWAPI VkResult
vkfwGetNextEvent (VKFWevent *e);

/**
 * Wait for an event to be delivered to the application. timeout is an optional
 * timeout in microseconds, or UINT64_MAX for no timeout.
 */
VKFWAPI VkResult
vkfwWaitNextEvent (VKFWevent *e, uint64_t timeout);

/**
 * Wait for an event to be delivered to the application. deadline is an optional
 * deadline in microseconds, or UINT64_MAX for no deadline.
 */
VKFWAPI VkResult
vkfwWaitNextEventUntil (VKFWevent *e, uint64_t deadline);

/**
 * Query a platform timer (ideally the same timer used by WaitNextEvent and
 * WaitNextEventUntil). Return value is in microseconds. The platform timer is
 * expected to be monotonic.
 */
VKFWAPI uint64_t
vkfwGetTime (void);

VKFWAPI void
vkfwDelay (uint64_t t);

VKFWAPI void
vkfwDelayUntil (uint64_t t);

#ifdef __cplusplus
}
#endif

#ifdef VKFW_BUILDING

extern bool vkfwHasInstance11;
extern bool vkfwHasInstance12;
extern bool vkfwHasInstance13;
extern bool vkfwHasDebugUtils;

/**
 * Helper function for constructing pNext chains.
 */
static inline void
vkfw_append_struct (void *base, void *ext)
{
	VkBaseOutStructure *base_ = (VkBaseOutStructure *) base;
	VkBaseOutStructure *ext_ = (VkBaseOutStructure *) ext;
	ext_->pNext = base_->pNext;
	base_->pNext = ext_;
}

/**
 * Search for a struct with a specific sType in a pNext chain.
 */
static inline const void *
vkfw_find_struct (void *base, VkStructureType sType)
{
	const VkBaseInStructure *base_ = (VkBaseInStructure *) base;
	for (; base_; base_ = base_->pNext)
		if (base_->sType == sType)
			return base_;
	return nullptr;
}

typedef struct VKFWplatform_T VKFWplatform;
typedef struct VKFWwindowbackend_T VKFWwindowbackend;

/**
 * These functions and variables are for internal use by VKFW. Do not use them
 * externally.
 */

extern VkInstance vkfwLoadedInstance;
extern VkDevice vkfwLoadedDevice;
extern VkPhysicalDevice vkfwPhysicalDevice;

void
vkfwLoadInstance (VkInstance instance);

void
vkfwLoadDevice (VkDevice device);

void
vkfwLoadLoader (PFN_vkGetInstanceProcAddr load);

void
vkfwShutdownDevice (void);

void
vkfwShutdownInstance (void);

#endif

#endif /* VKFW_H */

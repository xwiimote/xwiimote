/*
 * XWiimote - lib
 * Written 2010-2013 by David Herrmann <dh.herrmann@gmail.com>
 * Dedicated to the Public Domain
 */

#ifndef XWII_XWIIMOTE_H
#define XWII_XWIIMOTE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * Main libxwiimote API
 *
 * This file defines the public libxwiimote API and ABI. All identifiers are
 * prefixed either with **XWII_** or **xwii_**. Note that all identifiers
 * prefixed with a double-underscore (**XWII__** or **xwii__**) are not part
 * of the stable ABI and may change at any time.
 */

#if (__GNUC__ > 3)
#define XWII__DEPRECATED __attribute__((__deprecated__))
#else
#define XWII__DEPRECATED
#endif /* __GNUC__ */

/**
 * @defgroup kernel Kernel ABI
 * Kernel ABI constants
 *
 * Several constants and objects that are used by the kernel to communicate
 * with user-space. These indirectly define the kernel ABI, which is guaranteed
 * to be stable at all times.
 * Note that the direct kernel ABI is defined through kernel headers. The ABI
 * defined here extends it with information that we also guarantee to be stable
 * but isn't part of the direct ABI.
 *
 * The kernel ABI is almost complete abstracted by this library so these
 * constants are only needed for integration into existing applications. You
 * should try to avoid them and use them only if you need direct kernel access.
 *
 * The kernel driver **hid-wiimote** provides connected Wii-Remotes, and all
 * Nintendo or 3rd party devices that are compatible (including balance-boards,
 * pro-controllers, gamepads, ...), as HID devices. All HID devices can be
 * found in /sys/bus/hid/devices/. The kernel creates one directory for each
 * device.
 * A wiimote compatible device (**wiimote**) can be detected via normal
 * udev-filters. The **subsystem** field is **hid** and the **driver** field is
 * **wiimote**. If both match, the device is guaranteed to be handled by the
 * hid-wiimote driver and compatible with this library.
 *
 * Each wiimote provides several sub-devices as child-devices of the HID node.
 * During device-setup and device-detection, the kernel sets up most of these
 * nodes and sends a **change** event on the HID device after it is done.
 * Userspace must react to this event by re-reading the device state. Otherwise,
 * userspace might miss some nodes.
 * For each hotpluggable sub-device (like extensions or motion-plus), the
 * kernel attaches/detaches such nodes during runtime. Userspace must use
 * udev-monitors to react to those events, if interested. All available
 * interfaces on the HID device are explained below. From now on we assume that
 * `/sys/bus/hid/devices/[dev]/` is a valid wiimote device.
 *
 * Global Interfaces
 * =================
 *
 * The following interfaces are always present, regardless of the device-type
 * and extension-type.
 *
 * devtype
 * -------
 *
 * The HID device has a **devtype** attribute which can be found in
 * `/sys/bus/hid/devices/[dev]/devtype`. This attribute provides a
 * newline-terminated string which describes the device-type. If
 * support for new devices is added to the kernel, new identifiers may
 * be added. Valid values are:
 *
 * * **pending**: Device detection is not done, yet. The kernel will
 *                send a **change** uevent after it is done. A device
 *                must not be in this state for longer than a few hundred
 *                milliseconds.
 *
 * * **unknown**: The device-type is unknown and couldn't be initialized.
 *                Such devices are normally useles and should be ignored
 *                by most applications.
 *
 * * **generic**: The device-type could not be detected, but the device
 *                was successfully initialized. That means, most standard
 *                interfaces are available on this device, but may not
 *                function properly.
 *                Nearly all interfaces are enabled for such devices so
 *                no special policies apply.
 *
 * * **gen1[num]**: First generation of Wii-Remotes. This is mostly
 *                  **gen10**, but there are also 1st-gen devices with
 *                  built-in motion-plus which might be reported as
 *                  **gen15** or similar.
 *                  Newer WiiRemotePlus devices with built-in motion-plus
 *                  extensions belong to the 2nd-gen devices, though.
 *                  Nearly all interfaces are enabled for such devices.
 *
 * * **gen2[num]**: Second generation of Wii-Remotes. These are
 *                  guaranteed to have motion-plus built-in and use a
 *                  different bluetooth-chipset. Hence, there VID/PID
 *                  changed and a few details differ to gen1 devices.
 *                  Nearly all interfaces are enabled for such devices.
 *
 * * **balanceboard**: Balance-boards and compatible devices. Nearly no
 *                     interfaces are available on balance-boards. They
 *                     are limited to an extension port (which is
 *                     normally occupied by the balance-board extension),
 *                     one LED and the battery interface. No MotionPlus
 *                     is available.
 *
 * * **procontroller**: Wii-U Pro Controller and compatible devices.
 *                      Nearly no interfaces are available. One extension
 *                      port is supported (which is normally occupied by
 *                      the pro-controller extension), 4 LEDs and a
 *                      battery. MotionPlus may be available but is
 *                      currently not supported.
 *
 * Note that this attribute does not describe the extensions. Instead,
 * it describes the type of device. So users might build custom
 * extensions which allow a balance-board extension to be plugged on
 * a regular WiiRemote. This would cause **devtype** to be **gen10** but
 * **extension** to be **balanceboard**.
 *
 * extension
 * ---------
 *
 * An **extension** attribute is provided as
 * `/sys/bus/hid/devices/[dev]/extension` and provides a newline-terminated
 * string that describes the currently attached extension. New identifiers
 * might be added if we add support for new extensions to the kernel driver.
 * Note that normal Wii-Remotes provide a physical I2C extension port, but
 * other devices might not. So if **devtype** reports a different device-type
 * than a normal Wii-Remote, the extension might be built-in and not physically
 * unpluggable. Valid values are:
 *
 * * **none**: No extension is plugged.
 *
 * * **unknown**: An unknown extension is plugged or the driver
 *               failed to initialize it.
 *
 * * **nunchuk**: A Nintendo Nunchuk extension is plugged.
 *
 * * **classic**: A Classic Controller or Classic Controller Pro
 *                extension is plugged.
 *
 * * **balanceboard**: A balance-board extension is plugged. This is
 *                     normally a built-in extension.
 *
 * * **procontroller**: A pro-controller extension is plugged. This is
 *                      normally a built-in extension.
 *
 * Device-dependant interfaces
 * ===========================
 *
 * The following interfaces depend on the device-type. They may be present,
 * depending on the device. However, not that even if they are present, they
 * are not guaranteed to be physically available. For instance, many 3rd
 * party-device pretend to have a built-in battery, but do not report real
 * battery-capacity values. Instead they return a constant or fake value.
 * All the following interfaces are created during device-detection. After
 * device-detection is done, a **change** uevent is sent. Device-detection is
 * normally performed only once, but may be triggered via debug-hooks from
 * user-space at any time. Applications should be aware of that.
 *
 * Battery
 * -------
 *
 * A **power_supply** device is available as
 * `/sys/bus/hid/devices/[dev]/power_supply/wiimote_battery_[bdaddr]/`
 * and the interface is defined by the kernel **power_supply** interface.
 * *[bdaddr]* is the bluetooth-address of the remote device.
 *
 * LEDs
 * ----
 *
 * Player-LEDs on a device are available as
 * `/sys/bus/hid/devices/[dev]/leds/[dev]:blue:p[num]/` and the interface
 * is defined by the kernel **led** interface. *[dev]* is the same as the
 * device-name. *[num]* can be any non-negative integer and defines which LED
 * this is. Normally these are 0-3 for the 4 player-LEDs which can be found on
 * any Nintendo Remote. However, newer hardware may use more or less LEDs or
 * skip some (unlikely).
 *
 * Input: Core
 * ----------
 *
 * Input-Core: Core input device. It is available as
 * `/sys/bus/hid/device/[dev]/input/input[num]/` and can be detected via the
 * device name @ref XWII_NAME_CORE.
 *
 * TODO: Describe the provided interface
 *
 * Input: Accelerometer
 * --------------------
 *
 * Input-Accel: Accelerometer input device. Available as
 * `/sys/bus/hid/device/[dev]/input/input[num]/` and can be detected via the
 * device name @ref XWII_NAME_ACCEL. If this input-interface is not opened by
 * user-space, the accelerometer on the remote is disabled to save energy.
 *
 * TODO: Describe the provided interface
 *
 * Input: IR
 * ---------
 *
 * IR input device. It is available as
 * `/sys/bus/hid/device/[dev]/input/input[num]/` and can be detected via the
 * device name @ref XWII_NAME_IR. If this input-interface is not opened by
 * user-space, the IR-cam on the remote is disabled to save energy.
 *
 * TODO: Describe the provided interface
 *
 * Motion-Plus Interfaces
 * ======================
 *
 * The following interfaces belong to motion-plus capabilities. Motion-Plus
 * extension may be hotplugged or built-in. Even if built-in, they are handled
 * as special hotplugged extensions. So if you want to use Motion-Plus, you
 * must handle uevents properly.
 * As MotionPlus hotplug events are not generated by the device, the kernel
 * driver needs to periodically poll for them (only if not built-in). Hence,
 * hotplug-events may be delayed by up to 5s.
 * MotionPlus and related hardware is often abbreviated with **MP** or **M+**.
 *
 * Input: MP
 * ---------
 *
 * Motion-Plus input device. Is is available as
 * `/sys/bus/hid/device/[dev]/input/input[num]/` and can be detected via the
 * device name @ref XWII_NAME_MOTION_PLUS. If this input-interface is not
 * opened by user-space, the MP device is disabled to save energy. While the
 * interface is opened, MP hardware hotplug events are generated by the remote
 * device so we don't need to poll for MP availability.
 *
 * TODO: Describe the provided interface
 *
 * Extension Interfaces
 * ====================
 *
 * The following interfaces are extension interfaces. They are created whenever
 * an extension is hotplugged to a device. Only one extension-port is currently
 * available on each hardware (exposed via **extension** attribute), but newer
 * hardware may introduce more ports. Therefore, these extension might be
 * available simultaneously. However, in this case additional **extension2**
 * or similar attributes will also be introduced.
 * Note that some devices have built-in extensions which cannot be hotplugged.
 * But these extensions are handled as if they were normal hotpluggable
 * extensions.
 *
 * Extension-changes are advertized via udev uevents. The remote device sends
 * hotplug-events for regular extensions so they are deteced immediately (in a
 * few hundred milliseconds).
 * Note that devices are not initialized unless userspace opens them. This
 * saves energy as we don't need to power them up or stream any data.
 *
 * Input: Nunchuk
 * --------------
 *
 * Nunchuk extension input device. Available as
 * `/sys/bus/hid/device/[dev]/input/input[num]/` and can be detected via the
 * device name @ref XWII_NAME_NUNCHUK.
 *
 * TODO: Describe the provided interface
 *
 * Input: Classic Controller
 * -------------------------
 *
 * Classic Controller extension input device. Available as
 * `/sys/bus/hid/device/[dev]/input/input[num]/` and can be detected via the
 * device name @ref XWII_NAME_CLASSIC_CONTROLLER. The Classic Controller Pro is
 * also reported via this interface, but cannot be distinguished from a normal
 * classic controller extension.
 *
 * TODO: Describe the provided interface
 *
 * Input: Balance Board
 * --------------------
 *
 * BalanceBoard extension input device. Available as
 * `/sys/bus/hid/device/[dev]/input/input[num]/` and can be detected via the
 * device name @ref XWII_NAME_BALANCE_BOARD.
 *
 * TODO: Describe the provided interface
 *
 * Input: Pro Controller
 * ---------------------
 *
 * Wii-U Pro Controller extension input device. Available as
 * `/sys/bus/hid/device/[dev]/input/input[num]/` and can be detected via the
 * device name @ref XWII_NAME_PRO_CONTROLLER.
 *
 * TODO: Describe the provided interface
 *
 * Input: Drums Controller
 * ---------------------
 *
 * Drums controller extension input device. Available as
 * `/sys/bus/hid/device/[dev]/input/input[num]/` and can be detected via the
 * device name @ref XWII_NAME_DRUMS.
 * The different available devices are handled by the kernel and provided as
 * vendor-generic controller.
 *
 * TODO: Describe the provided interface
 *
 * Input: Guitar Controller
 * ---------------------
 *
 * Guitar controller extension input device. Available as
 * `/sys/bus/hid/device/[dev]/input/input[num]/` and can be detected via the
 * device name @ref XWII_NAME_GUITAR.
 * The different available devices are handled by the kernel and provided as
 * vendor-generic controller.
 *
 * TODO: Describe the provided interface
 *
 * @{
 */

#define XWII__NAME			"Nintendo Wii Remote"

/** Name of the core input device */
#define XWII_NAME_CORE			XWII__NAME
/** Name of the accelerometer input device */
#define XWII_NAME_ACCEL			XWII__NAME " Accelerometer"
/** Name of the IR input device */
#define XWII_NAME_IR			XWII__NAME " IR"

/** Name of the motion-plus input device */
#define XWII_NAME_MOTION_PLUS		XWII__NAME " Motion Plus"
/** Name of the nunchuk input device */
#define XWII_NAME_NUNCHUK		XWII__NAME " Nunchuk"
/** Name of the classic-controller input device */
#define XWII_NAME_CLASSIC_CONTROLLER	XWII__NAME " Classic Controller"
/** Name of the balance-board input device */
#define XWII_NAME_BALANCE_BOARD		XWII__NAME " Balance Board"
/** Name of the pro-controller input device */
#define XWII_NAME_PRO_CONTROLLER	XWII__NAME " Pro Controller"
/** Name of the drums-controller input device */
#define XWII_NAME_DRUMS			XWII__NAME " Drums"
/** Name of the guitar-controller input device */
#define XWII_NAME_GUITAR		XWII__NAME " Guitar"

/** @} */

/**
 * @defgroup events Device Events
 * Device event handling
 *
 * Devices notify users about any state-changes via events. These events can
 * contain peripheral-data, hotplug-information or more.
 *
 * @{
 */

/**
 * Event Types
 *
 * Each event can be identified by the type field. New types might be added
 * at any time so unknown event-types must be ignored by applications. The
 * given payload of an event is described for each type. Unused payload-space
 * is zeroed by the library. However, the payload may be extended in new
 * revisions so applications must not depend on it being 0 or untouched.
 */
enum xwii_event_types {
	/**
	 * Core-interface key event
	 *
	 * The payload of such events is struct xwii_event_key. Valid
	 * key-events include all the events reported by the core-interface,
	 * which is normally only LEFT, RIGHT, UP, DOWN, A, B, PLUS, MINUS,
	 * HOME, ONE, TWO.
	 */
	XWII_EVENT_KEY,

	/**
	 * Accelerometer event
	 *
	 * Provides accelerometer data. Payload is struct xwii_event_abs
	 * and only the first element in the abs-array is used. The x, y and z
	 * fields contain the accelerometer data.
	 * Note that the accelerometer reports acceleration data, not speed
	 * data!
	 */
	XWII_EVENT_ACCEL,

	/**
	 * IR-Camera event
	 *
	 * Provides IR-camera events. The camera can track up two four IR
	 * sources. As long as a single source is tracked, it stays at it's
	 * pre-allocated slot. The four available slots are reported as
	 * struct xwii_event_abs
	 * payload. The x and y fields contain the position of each slot.
	 *
	 * Use xwii_event_ir_is_valid() to see whether a specific slot is
	 * currently valid or whether it currently doesn't track any IR source.
	 */
	XWII_EVENT_IR,

	/**
	 * Balance-Board event
	 *
	 * Provides balance-board weight data. Four sensors report weight-data
	 * for each of the four edges of the board. The data is available as
	 * struct xwii_event_abs
	 * payload. The x fields of the first four array-entries contain the
	 * weight-value.
	 */
	XWII_EVENT_BALANCE_BOARD,

	/**
	 * Motion-Plus event
	 *
	 * Motion-Plus gyroscope events. These describe rotational speed, not
	 * acceleration, of the motion-plus extension. The payload is available
	 * as struct xwii_event_abs
	 * and the x, y and z field of the first array-element describes the
	 * motion-events in the 3 dimensions.
	 */
	XWII_EVENT_MOTION_PLUS,

	/**
	 * Pro-Controller key event
	 *
	 * Button events of the pro-controller are reported via this interface
	 * and not via the core-interface (which only reports core-buttons).
	 * Valid buttons include: LEFT, RIGHT, UP, DOWN, PLUS, MINUS, HOME, X,
	 * Y, A, B, TR, TL, ZR, ZL, THUMBL, THUMBR.
	 * Payload type is struct xwii_event_key.
	 */
	XWII_EVENT_PRO_CONTROLLER_KEY,

	/**
	 * Pro-Controller movement event
	 *
	 * Movement of analog sticks are reported via this event. The payload
	 * is a struct xwii_event_abs
	 * and the first two array elements contain the absolute x and y
	 * position of both analog sticks.
	 */
	XWII_EVENT_PRO_CONTROLLER_MOVE,

	/**
	 * Hotplug Event
	 *
	 * This event is sent whenever an extension was hotplugged (plugged or
	 * unplugged), a device-detection finished or some other static data
	 * changed which cannot be monitored separately. No payload is provided.
	 * An application should check what changed by examining the device is
	 * testing whether all required interfaces are still available.
	 * Non-hotplug aware devices may discard this event.
	 *
	 * This is only returned if you explicitly watched for hotplug events.
	 * See xwii_iface_watch().
	 *
	 * This event is also returned if an interface is closed because the
	 * kernel closed our file-descriptor (for whatever reason). This is
	 * returned regardless whether you watch for hotplug events or not.
	 */
	XWII_EVENT_WATCH,

	/**
	 * Classic Controller key event
	 *
	 * Button events of the classic controller are reported via this
	 * interface and not via the core-interface (which only reports
	 * core-buttons).
	 * Valid buttons include: LEFT, RIGHT, UP, DOWN, PLUS, MINUS, HOME, X,
	 * Y, A, B, TR, TL, ZR, ZL.
	 * Payload type is struct xwii_event_key.
	 */
	XWII_EVENT_CLASSIC_CONTROLLER_KEY,

	/**
	 * Classic Controller movement event
	 *
	 * Movement of analog sticks are reported via this event. The payload
	 * is a struct xwii_event_abs and the first two array elements contain
	 * the absolute x and y position of both analog sticks.
	 * The x value of the third array element contains the absolute position
	 * of the TL trigger. The y value contains the absolute position for the
	 * TR trigger. Note that many classic controllers do not have analog
	 * TL/TR triggers, in which case these read 0 or MAX (63). The digital
	 * TL/TR buttons are always reported correctly.
	 */
	XWII_EVENT_CLASSIC_CONTROLLER_MOVE,

	/**
	 * Nunchuk key event
	 *
	 * Button events of the nunchuk controller are reported via this
	 * interface and not via the core-interface (which only reports
	 * core-buttons).
	 * Valid buttons include: C, Z
	 * Payload type is struct xwii_event_key.
	 */
	XWII_EVENT_NUNCHUK_KEY,

	/**
	 * Nunchuk movement event
	 *
	 * Movement events of the nunchuk controller are reported via this
	 * interface. Payload is of type struct xwii_event_abs. The first array
	 * element contains the x/y positions of the analog stick. The second
	 * array element contains the accelerometer information.
	 */
	XWII_EVENT_NUNCHUK_MOVE,

	/**
	 * Drums key event
	 *
	 * Button events for drums controllers. Valid buttons are PLUS and MINUS
	 * for the +/- buttons on the center-bar.
	 * Payload type is struct xwii_event_key.
	 */
	XWII_EVENT_DRUMS_KEY,

	/**
	 * Drums movement event
	 *
	 * Movement and pressure events for drums controllers. Payload is of
	 * type struct xwii_event_abs. The indices are describe as
	 * enum xwii_drums_abs and each of them contains the corresponding
	 * stick-movement or drum-pressure values.
	 */
	XWII_EVENT_DRUMS_MOVE,

	/**
	 * Guitar key event
	 *
	 * Button events for guitar controllers. Valid buttons are HOME and PLUS
	 * for the StarPower/Home button and the + button. Furthermore, you get
	 * FRET_FAR_UP, FRET_UP, FRET_MID, FRET_LOW, FRET_FAR_LOW for fret
	 * activity and STRUM_BAR_UP and STRUM_BAR_LOW for the strum bar.
	 * Payload type is struct xwii_event_key.
	 */
	XWII_EVENT_GUITAR_KEY,

	/**
	 * Guitar movement event
	 *
	 * Movement information for guitar controllers. Payload is of type
	 * struct xwii_event_abs. The first element contains X and Y direction
	 * of the analog stick. The second element contains whammy-bar movement
	 * information as x-value. The third element contains fret-bar absolute
	 * positioning information as x-value.
	 */
	XWII_EVENT_GUITAR_MOVE,

	/**
	 * Removal Event
	 *
	 * This event is sent whenever the device was removed. No payload is
	 * provided. Non-hotplug aware applications may discard this event.
	 *
	 * This is only returned if you explicitly watched for hotplug events.
	 * See xwii_iface_watch().
	 */
	XWII_EVENT_GONE,

	/**
	 * Number of available event types
	 *
	 * The value of this constant may increase on each new library revision.
	 * It is not guaranteed to stay constant. However, it may never shrink.
	 */
	XWII_EVENT_NUM
};

/**
 * Key Event Identifiers
 *
 * For each key found on a supported device, a separate key identifier is
 * defined. Note that a device may have a specific key (for instance: HOME) on
 * the main device and on an extension device. An application can detect which
 * key was pressed examining the event-type field.
 * Some devices report common keys as both, extension and core events. In this
 * case the kernel is required to filter these and you should report it as a
 * bug. A single physical key-press should never be reported twice, even on two
 * different interfaces.
 *
 * Most of the key-names should be self-explanatory.
 */
enum xwii_event_keys {
	XWII_KEY_LEFT,
	XWII_KEY_RIGHT,
	XWII_KEY_UP,
	XWII_KEY_DOWN,
	XWII_KEY_A,
	XWII_KEY_B,
	XWII_KEY_PLUS,
	XWII_KEY_MINUS,
	XWII_KEY_HOME,
	XWII_KEY_ONE,
	XWII_KEY_TWO,
	XWII_KEY_X,
	XWII_KEY_Y,
	XWII_KEY_TL,
	XWII_KEY_TR,
	XWII_KEY_ZL,
	XWII_KEY_ZR,

	/**
	 * Left thumb button
	 *
	 * This is reported if the left analog stick is pressed. Not all analog
	 * sticks support this. The Wii-U Pro Controller is one of few devices
	 * that report this event.
	 */
	XWII_KEY_THUMBL,

	/**
	 * Right thumb button
	 *
	 * This is reported if the right analog stick is pressed. Not all analog
	 * sticks support this. The Wii-U Pro Controller is one of few devices
	 * that report this event.
	 */
	XWII_KEY_THUMBR,

	/**
	 * Extra C button
	 *
	 * This button is not part of the standard action pad but reported by
	 * extension controllers like the Nunchuk. It is supposed to extend the
	 * standard A and B buttons.
	 */
	XWII_KEY_C,

	/**
	 * Extra Z button
	 *
	 * This button is not part of the standard action pad but reported by
	 * extension controllers like the Nunchuk. It is supposed to extend the
	 * standard X and Y buttons.
	 */
	XWII_KEY_Z,

	/**
	 * Guitar Strum-bar-up event
	 *
	 * Emitted by guitars if the strum-bar is moved up.
	 */
	XWII_KEY_STRUM_BAR_UP,

	/**
	 * Guitar Strum-bar-down event
	 *
	 * Emitted by guitars if the strum-bar is moved down.
	 */
	XWII_KEY_STRUM_BAR_DOWN,

	/**
	 * Guitar Fret-Far-Up event
	 *
	 * Emitted by guitars if the upper-most fret-bar is pressed.
	 */
	XWII_KEY_FRET_FAR_UP,

	/**
	 * Guitar Fret-Up event
	 *
	 * Emitted by guitars if the second-upper fret-bar is pressed.
	 */
	XWII_KEY_FRET_UP,

	/**
	 * Guitar Fret-Mid event
	 *
	 * Emitted by guitars if the mid fret-bar is pressed.
	 */
	XWII_KEY_FRET_MID,

	/**
	 * Guitar Fret-Low event
	 *
	 * Emitted by guitars if the second-lowest fret-bar is pressed.
	 */
	XWII_KEY_FRET_LOW,

	/**
	 * Guitar Fret-Far-Low event
	 *
	 * Emitted by guitars if the lower-most fret-bar is pressed.
	 */
	XWII_KEY_FRET_FAR_LOW,

	/**
	 * Number of key identifiers
	 *
	 * This defines the number of available key-identifiers. It is not
	 * guaranteed to stay constant and may change when new identifiers are
	 * added. However, it will never shrink.
	 */
	XWII_KEY_NUM
};

/**
 * Key Event Payload
 *
 * A key-event always uses this payload.
 */
struct xwii_event_key {
	/** key identifier defined as enum xwii_event_keys */
	unsigned int code;
	/** key state copied from kernel (0: up, 1: down, 2: auto-repeat) */
	unsigned int state;
};

/**
 * Absolute Motion Payload
 *
 * This payload is used for absolute motion events. The meaning of the fields
 * depends on the event-type.
 */
struct xwii_event_abs {
	int32_t x;
	int32_t y;
	int32_t z;
};

/**
 * Absolute Drum-Motion Indices
 *
 * A drum-payload can contain a lot of different absolute motion events all in
 * a single object. Depending on the event-type, the array offsets for
 * absolute-motion events are different. This enum describes the indices used
 * for drums.
 *
 * Note that these mimic the kernel API. If new drums with more tom-toms or
 * cymbals are supported, the corresponding TOM and CYMBAL values will be added.
 * That's also why there is no TOM_MID, but only TOM_FAR_RIGHT so far.
 */
enum xwii_drums_abs {
	/** Control pad motion. X and Y direction available. */
	XWII_DRUMS_ABS_PAD,
	/** Cymbal pressure, just X direction. */
	XWII_DRUMS_ABS_CYMBAL_LEFT,
	/** Cymbal pressure, just X direction. */
	XWII_DRUMS_ABS_CYMBAL_RIGHT,
	/** Mid-left tom pressure, just X direction. */
	XWII_DRUMS_ABS_TOM_LEFT,
	/** Mid-right tom pressure, just X direction. */
	XWII_DRUMS_ABS_TOM_RIGHT,
	/** Right-most tom pressure, just X direction. */
	XWII_DRUMS_ABS_TOM_FAR_RIGHT,
	/** Bass pressure, just X direction. */
	XWII_DRUMS_ABS_BASS,
	/** Hi-Hat pressure, just X direction. */
	XWII_DRUMS_ABS_HI_HAT,
	/** Number of drums payloads, may get increased for new ones. */
	XWII_DRUMS_ABS_NUM,
};

/** Number of ABS values in an xwii_event_union */
#define XWII_ABS_NUM 8

/**
 * Event Payload
 *
 * Payload of event objects.
 */
union xwii_event_union {
	/** key event payload */
	struct xwii_event_key key;
	/** absolute motion event payload */
	struct xwii_event_abs abs[XWII_ABS_NUM];
	/** reserved; do not use! */
	uint8_t reserved[128];
};

/**
 * Event Object
 *
 * Every event is reported via this structure.
 * Note that even though this object reserves some space, it may grow in the
 * future. It is not guaranteed to stay at this size. That's why functions
 * dealing with it always accept an additional size argument, which is used
 * for backwards-compatibility to not write beyond object-boundaries.
 */
struct xwii_event {
	/** timestamp when this event was generated (copied from kernel) */
	struct timeval time;
	/** event type ref xwii_event_types */
	unsigned int type;

	/** data payload */
	union xwii_event_union v;
};

/**
 * Test whether an IR event is valid
 *
 * If you receive an IR event, you can use this function on the first 4
 * absolute motion payloads. It returns true iff the given slot currently tracks
 * a valid IR source. false is returned if the slot is invalid and currently
 * disabled (due to missing IR sources).
 */
static inline bool xwii_event_ir_is_valid(const struct xwii_event_abs *abs)
{
	return abs->x != 1023 || abs->y != 1023;
}

/** @} */

/**
 * @defgroup device Device Interface
 * Communication between applications and devices
 *
 * The device interface provides a way to communicate with a connected remote
 * device. It reads events from the device and provides them to the application.
 * But it also allows applications to send events to devices.
 *
 * Note that devices cannot be connected or searched for with this API. Instead,
 * you should use your standard bluetooth tools to perform a bluetooth inquiry
 * and connect devices. You do the same with bluetooth keyboards and mice, don't
 * you?
 *
 * If you want to enumerate connected devices and monitor the system for hotplug
 * events, you should use the @ref monitor "monitor interface" or use libudev
 * directly.
 *
 * The device interface is split up into different sub-interfaces. Each of them
 * is related to specific hardware available on the remote device. If some
 * hardware is not present, the interfaces will not be provided to the
 * application and will return -ENODEV.
 *
 * Interfaces must be opened via xwii_iface_open() before you can use them. Once
 * opened, they return events via the event stream which is accessed via
 * xwii_iface_dispatch(). Furthermore, outgoing events can now be sent via the
 * different helper functions.
 * Some interfaces are static and don't need to be opened. You notice it if no
 * XWII_IFACE_* constant is provided.
 *
 * Once you are done with an interface, you should close it via
 * xwii_iface_close(). The kernel can deactivate unused hardware to safe energy.
 * If you keep them open, the kernel keeps them powered up.
 *
 * @{
 */

/**
 * Device Object
 *
 * This object describes the communication with a single device. That is, you
 * create one for each device you use. All sub-interfaces are opened on this
 * object.
 */
struct xwii_iface;

/**
 * Interfaces
 *
 * Each constant describes a single interface. These are bit-masks that can be
 * binary-ORed. If an interface does not provide such a constant, it is static
 * and can be used without opening/closing it.
 */
enum xwii_iface_type {
	/** Core interface */
	XWII_IFACE_CORE			= 0x000001,
	/** Accelerometer interface */
	XWII_IFACE_ACCEL		= 0x000002,
	/** IR interface */
	XWII_IFACE_IR			= 0x000004,

	/** MotionPlus extension interface */
	XWII_IFACE_MOTION_PLUS		= 0x000100,
	/** Nunchuk extension interface */
	XWII_IFACE_NUNCHUK		= 0x000200,
	/** ClassicController extension interface */
	XWII_IFACE_CLASSIC_CONTROLLER	= 0x000400,
	/** BalanceBoard extension interface */
	XWII_IFACE_BALANCE_BOARD	= 0x000800,
	/** ProController extension interface */
	XWII_IFACE_PRO_CONTROLLER	= 0x001000,
	/** Drums extension interface */
	XWII_IFACE_DRUMS		= 0x002000,
	/** Guitar extension interface */
	XWII_IFACE_GUITAR		= 0x004000,

	/** Special flag ORed with all valid interfaces */
	XWII_IFACE_ALL			= XWII_IFACE_CORE |
					  XWII_IFACE_ACCEL |
					  XWII_IFACE_IR |
					  XWII_IFACE_MOTION_PLUS |
					  XWII_IFACE_NUNCHUK |
					  XWII_IFACE_CLASSIC_CONTROLLER |
					  XWII_IFACE_BALANCE_BOARD |
					  XWII_IFACE_PRO_CONTROLLER |
					  XWII_IFACE_DRUMS |
					  XWII_IFACE_GUITAR,
	/** Special flag which causes the interfaces to be opened writable */
	XWII_IFACE_WRITABLE		= 0x010000,
};

/**
 * Return name of a given interface
 *
 * @param[in] iface A single interface of type @ref xwii_iface_type
 *
 * Returns the name of a single given interface. If the interface is invalid,
 * NULL is returned. The returned names are the same as the XWII_NAME_*
 * constants of the kernel ABI.
 *
 * @returns constant string if @p iface is known or NULL if unknown.
 */
const char *xwii_get_iface_name(unsigned int iface);

/**
 * LEDs
 *
 * One constant for each Player-LED.
 */
enum xwii_led {
	XWII_LED1 = 1,
	XWII_LED2 = 2,
	XWII_LED3 = 3,
	XWII_LED4 = 4,
};

/**
 * Create enum xwii_led constants during runtime
 *
 * The argument is a number starting with 1. So XWII_LED([num]) produces the
 * same value as the constant XWII_LED[num] defined in enum xwii_led.
 */
#define XWII_LED(num) (XWII_LED1 + (num) - 1)

/**
 * Create new device object from syspath path
 *
 * @param[out] dev Pointer to new opaque device is stored here
 * @param[in] syspath Sysfs path to root device node
 *
 * Creates a new device object. No interfaces on the device are opened by
 * default. @p syspath must be a valid path to a wiimote device, either
 * retrieved via a @ref monitor "monitor object" or via udev. It must point to
 * the hid device, which is normally /sys/bus/hid/devices/[dev].
 *
 * If this function fails, @p dev is not touched at all (and not cleared!). A
 * new object always has an initial ref-count of 1.
 *
 * @returns 0 on success, negative error code on failure
 */
int xwii_iface_new(struct xwii_iface **dev, const char *syspath);

/**
 * Increase ref-count by 1
 *
 * @param[in] dev Valid device object
 */
void xwii_iface_ref(struct xwii_iface *dev);

/**
 * Decrease ref-count by 1
 *
 * @param[in] dev Valid device object
 *
 * If the ref-count drops below 1, the object is destroyed immediately. All
 * open interfaces are automatically closed and all allocated objects released
 * when the object is destroyed.
 */
void xwii_iface_unref(struct xwii_iface *dev);

/**
 * Return device syspath
 *
 * @param[in] dev Valid device object
 *
 * This returns the sysfs path of the underlying device. It is not neccesarily
 * the same as the one during xwii_iface_new(). However, it is guaranteed to
 * point at the same device (symlinks may be resolved).
 *
 * @returns NULL on failure, otherwise a constant device syspath is returned.
 */
const char *xwii_iface_get_syspath(struct xwii_iface *dev);

/**
 * Return file-descriptor
 *
 * @param[in] dev Valid device object
 *
 * Return the file-descriptor used by this device. If multiple file-descriptors
 * are used internally, they are multi-plexed through an epoll descriptor.
 * Therefore, this always returns the same single file-descriptor. You need to
 * watch this for readable-events (POLLIN/EPOLLIN) and call
 * xwii_iface_dispatch() whenever it is readable.
 *
 * This function always returns a valid file-descriptor.
 */
int xwii_iface_get_fd(struct xwii_iface *dev);

/**
 * Watch device for hotplug events
 *
 * @param[in] dev Valid device object
 * @param[in] watch Whether to watch for hotplug events or not
 *
 * Toggle whether hotplug events should be reported or not. By default, no
 * hotplug events are reported so this is off.
 *
 * Note that this requires a separate udev-monitor for each device. Therefore,
 * if your application uses its own udev-monitor, you should instead integrate
 * the hotplug-detection into your udev-monitor.
 *
 * @returns 0 on success, negative error code on failure
 */
int xwii_iface_watch(struct xwii_iface *dev, bool watch);

/**
 * Open interfaces on this device
 *
 * @param[in] dev Valid device object
 * @param[in] ifaces Bitmask of interfaces of type enum xwii_iface_type
 *
 * Open all the requested interfaces. If @ref XWII_IFACE_WRITABLE is also set,
 * the interfaces are opened with write-access. Note that interfaces that are
 * already opened are ignored and not touched.
 * If _any_ interface fails to open, this function still tries to open the other
 * requested interfaces and then returns the error afterwards. Hence, if this
 * function fails, you should use xwii_iface_opened() to get a bitmask of opened
 * interfaces and see which failed (if that is of interest).
 *
 * Note that interfaces may be closed automatically during runtime if the
 * kernel removes the interface or on error conditions. You always get an
 * @ref XWII_EVENT_WATCH event which you should react on. This is returned
 * regardless whether xwii_iface_watch() was enabled or not.
 *
 * @returns 0 on success, negative error code on failure.
 */
int xwii_iface_open(struct xwii_iface *dev, unsigned int ifaces);

/**
 * Close interfaces on this device
 *
 * @param[in] dev Valid device object
 * @param[in] ifaces Bitmask of interfaces of type enum xwii_iface_type
 *
 * Close the requested interfaces. This never fails.
 */
void xwii_iface_close(struct xwii_iface *dev, unsigned int ifaces);

/**
 * Return bitmask of opened interfaces
 *
 * @param[in] dev Valid device object
 *
 * Returns a bitmask of opened interfaces. Interfaces may be closed due to
 * error-conditions at any time. However, interfaces are never opened
 * automatically.
 *
 * You will get notified whenever this bitmask changes, except on explicit
 * calls to xwii_iface_open() and xwii_iface_close(). See the
 * @ref XWII_EVENT_WATCH event for more information.
 */
unsigned int xwii_iface_opened(struct xwii_iface *dev);

/**
 * Return bitmask of available interfaces
 *
 * @param[in] dev Valid device object
 *
 * Return a bitmask of available devices. These devices can be opened and are
 * guaranteed to be present on the hardware at this time. If you watch your
 * device for hotplug events (see xwii_iface_watch()) you will get notified
 * whenever this bitmask changes. See the @ref XWII_EVENT_WATCH event for more
 * information.
 */
unsigned int xwii_iface_available(struct xwii_iface *dev);

/**
 * Read incoming event-queue
 *
 * @param[in] dev Valid device object
 * @param[out] ev Pointer where to store a new event or NULL
 *
 * You should call this whenever the file-descriptor returned by
 * xwii_iface_get_fd() is reported as being readable. This function will perform
 * all non-blocking outstanding tasks and then return.
 *
 * This function always performs any background tasks and outgoing event-writes
 * if they don't block. It returns an error if they fail.
 * If @p ev is NULL, this function returns 0 on success after this has been
 * done.
 *
 * If @p ev is non-NULL, this function then tries to read a single incoming
 * event. If no event is available, it returns -EAGAIN and you should watch the
 * file-desciptor again until it is readable. Otherwise, you should call this
 * function in a row as long as it returns 0. It stores the event in @p ev which
 * you can then handle in your application.
 *
 * @returns 0 on success, -EAGAIN if no event can be read and @p ev is non-NULL
 * and a negative error-code on failure
 */
XWII__DEPRECATED
int xwii_iface_poll(struct xwii_iface *dev, struct xwii_event *ev);

/**
 * Read incoming event-queue
 *
 * @param[in] dev Valid device object
 * @param[out] ev Pointer where to store a new event or NULL
 * @param[in] size Size of @p ev if @p ev is non-NULL
 *
 * You should call this whenever the file-descriptor returned by
 * xwii_iface_get_fd() is reported as being readable. This function will perform
 * all non-blocking outstanding tasks and then return.
 *
 * This function always performs any background tasks and outgoing event-writes
 * if they don't block. It returns an error if they fail.
 * If @p ev is NULL, this function returns 0 on success after this has been
 * done.
 *
 * If @p ev is non-NULL, this function then tries to read a single incoming
 * event. If no event is available, it returns -EAGAIN and you should watch the
 * file-desciptor again until it is readable. Otherwise, you should call this
 * function in a row as long as it returns 0. It stores the event in @p ev which
 * you can then handle in your application.
 *
 * This function is the successor or xwii_iface_poll(). It takes an additional
 * @p size argument to provide backwards compatibility.
 *
 * @returns 0 on success, -EAGAIN if no event can be read and @p ev is non-NULL
 * and a negative error-code on failure
 */
int xwii_iface_dispatch(struct xwii_iface *dev, struct xwii_event *ev,
			size_t size);

/**
 * Toggle rumble motor
 *
 * @param[in] dev Valid device object
 * @param[in] on New rumble motor state
 *
 * Toggle the rumble motor. This requires the core-interface to be opened in
 * writable mode.
 *
 * @returns 0 on success, negative error code on failure.
 */
int xwii_iface_rumble(struct xwii_iface *dev, bool on);

/**
 * Read LED state
 *
 * @param[in] dev Valid device object
 * @param[in] led LED constant defined in enum xwii_led
 * @param[out] state Pointer where state should be written to
 *
 * Reads the current LED state of the given LED. @p state will be either true or
 * false depending on whether the LED is on or off.
 *
 * LEDs are a static interface that does not have to be opened first.
 *
 * @returns 0 on success, negative error code on failure
 */
int xwii_iface_get_led(struct xwii_iface *dev, unsigned int led, bool *state);

/**
 * Set LED state
 *
 * @param[in] dev Valid device object
 * @param[in] led LED constant defined in enum xwii_led
 * @param[in] state State to set on the LED
 *
 * Changes the current LED state of the given LED. This has immediate effect.
 *
 * LEDs are a static interface that does not have to be opened first.
 *
 * @returns 0 on success, negative error code on failure
 */
int xwii_iface_set_led(struct xwii_iface *dev, unsigned int led, bool state);

/**
 * Read battery state
 *
 * @param[in] dev Valid device object
 * @param[out] capacity Pointer where state should be written to
 *
 * Reads the current battery capacity and write it into @p capacity. This is
 * a value between 0 and 100, which describes the current capacity in per-cent.
 *
 * Batteries are a static interface that does not have to be opened first.
 *
 * @returns 0 on success, negative error code on failure
 */
int xwii_iface_get_battery(struct xwii_iface *dev, uint8_t *capacity);

/**
 * Read device type
 *
 * @param[in] dev Valid device object
 * @param[out] devtype Pointer where the device type should be stored
 *
 * Reads the current device-type, allocates a string and stores a pointer to
 * the string in @p devtype. You must free it via free() after you are done.
 *
 * This is a static interface that does not have to be opened first.
 *
 * @returns 0 on success, negative error code on failure
 */
int xwii_iface_get_devtype(struct xwii_iface *dev, char **devtype);

/**
 * Read extension type
 *
 * @param[in] dev Valid device object
 * @param[out] extension Pointer where the extension type should be stored
 *
 * Reads the current extension type, allocates a string and stores a pointer
 * to the string in @p extension. You must free it via free() after you are
 * done.
 *
 * This is a static interface that does not have to be opened first.
 *
 * @returns 0 on success, negative error code on failure
 */
int xwii_iface_get_extension(struct xwii_iface *dev, char **extension);

/**
 * Set MP normalization and calibration
 *
 * @param[in] dev Valid device object
 * @param[in] x x-value to use or 0
 * @param[in] y y-value to use or 0
 * @param[in] z z-value to use or 0
 * @param[in] factor factor-value to use or 0
 *
 * Set MP-normalization and calibration values. The Motion-Plus sensor is very
 * sensitive and may return really crappy values. This interfaces allows to
 * apply 3 absolute offsets x, y and z which are subtracted from any MP data
 * before it is returned to the application. That is, if you set these values
 * to 0, this has no effect (which is also the initial state).
 *
 * The calibration factor @p factor is used to perform runtime calibration. If
 * it is 0 (the initial state), no runtime calibration is performed. Otherwise,
 * the factor is used to re-calibrate the zero-point of MP data depending on MP
 * input. This is an angoing calibration which modifies the internal state of
 * the x, y and z values.
 */
void xwii_iface_set_mp_normalization(struct xwii_iface *dev, int32_t x,
				     int32_t y, int32_t z, int32_t factor);

/**
 * Read MP normalization and calibration
 *
 * @param[in] dev Valid device object
 * @param[out] x Pointer where to store x-value or NULL
 * @param[out] y Pointer where to store y-value or NULL
 * @param[out] z Pointer where to store z-value or NULL
 * @param[out] factor Pointer where to store factor-value or NULL
 *
 * Reads the MP normalization and calibration values. Please see
 * xwii_iface_set_mp_normalization() how this is handled.
 *
 * Note that if the calibration factor is not 0, the normalization values may
 * change depending on incoming MP data. Therefore, the data read via this
 * function may differ from the values that you wrote to previously. However,
 * apart from applied calibration, these value are the same as were set
 * previously via xwii_iface_set_mp_normalization() and you can feed them back
 * in later.
 */
void xwii_iface_get_mp_normalization(struct xwii_iface *dev, int32_t *x,
				     int32_t *y, int32_t *z, int32_t *factor);

/** @} */

/**
 * @defgroup monitor Device Monitor
 * Monitor system for new wiimote devices.
 *
 * This monitor can be used to enumerate all connected wiimote devices and also
 * monitoring the system for hotplugged wiimote devices.
 * This is a simple wrapper around libudev and should only be used if your
 * application does not use udev on its own.
 * See the implementation of the monitor to integrate wiimote-monitoring into
 * your own udev routines.
 *
 * @{
 */

/**
 * Monitor object
 *
 * Each object describes a separate monitor. A single monitor must not be
 * used from multiple threads without locking. Different monitors are
 * independent of each other and can be used simultaneously.
 */
struct xwii_monitor;

/**
 * Create a new monitor
 *
 * Creates a new monitor and returns a pointer to the opaque object. NULL is
 * returned on failure.
 *
 * @param[in] poll True if this monitor should watch for hotplug events
 * @param[in] direct True if kernel uevents should be used instead of udevd
 *
 * A monitor always provides all devices that are available on a system. If
 * @p poll is true, the monitor also sets up a system-monitor to watch the
 * system for new hotplug events so new devices can be detected.
 *
 * A new monitor always has a ref-count of 1.
 */
struct xwii_monitor *xwii_monitor_new(bool poll, bool direct);

/**
 * Increase monitor ref-count by 1
 *
 * @param[in] mon Valid monitor object
 */
void xwii_monitor_ref(struct xwii_monitor *mon);

/**
 * Decrease monitor ref-count by 1
 *
 * @param[in] mon Valid monitor object
 *
 * If the ref-count drops below 1, the object is destroyed immediately.
 */
void xwii_monitor_unref(struct xwii_monitor *mon);

/**
 * Return internal fd
 *
 * @param[in] monitor A valid monitor object
 * @param[in] blocking True to set the monitor in blocking mode
 *
 * Returns the file-descriptor used by this monitor. If @p blocking is true,
 * the FD is set into blocking mode. If false, it is set into non-blocking mode.
 * Only one file-descriptor exists, that is, this function always returns the
 * same descriptor.
 *
 * This returns -1 if this monitor was not created with a hotplug-monitor. So
 * you need this function only if you want to watch the system for hotplug
 * events. Whenever this descriptor is readable, you should call
 * xwii_monitor_poll() to read new incoming events.
 */
int xwii_monitor_get_fd(struct xwii_monitor *monitor, bool blocking);

/**
 * Read incoming events
 *
 * @param[in] monitor A valid monitor object
 *
 * This returns a single device-name on each call. A device-name is actually
 * an absolute sysfs path to the device's root-node. This is normally a path
 * to /sys/bus/hid/devices/[dev]/. You can use this path to create a new
 * struct xwii_iface object.
 *
 * After a monitor was created, this function returns all currently available
 * devices. After all devices have been returned, this function returns NULL
 * _once_. After that, this function polls the monitor for hotplug events and
 * returns hotplugged devices, if the monitor was opened to watch the system for
 * hotplug events.
 * Use xwii_monitor_get_fd() to get notified when a new event is available. If
 * the fd is in non-blocking mode, this function never blocks but returns NULL
 * if no new event is available.
 *
 * The returned string must be freed with free() by the caller.
 */
char *xwii_monitor_poll(struct xwii_monitor *monitor);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* XWII_XWIIMOTE_H */

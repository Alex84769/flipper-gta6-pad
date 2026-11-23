/*
 * GTA VI Pad pro Flipper Zero
 *
 * Flipper se po USB tvari jako kabelovy ovladac Xbox 360 (XInput).
 * Tlacitko Zpet funguje jako SHIFT:
 *
 *   Bez shiftu:  sipky = leva packa (chuze), OK = A
 *   Se shiftem:  nahoru = RT, dolu = LT, doprava = Y, doleva = B, OK = X
 *
 *   Konec aplikace: 3x rychle zmackni Zpet (bez jinych tlacitek).
 *
 * USB cast je prevzata z "USB Game Controller" od crapbass
 * (https://github.com/expected-ingot/flipper-xinput), licence GPL-3.0.
 */

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>
#include <usb.h>
#include <usb_hid.h>
#include <stdlib.h>
#include <string.h>
#include <gui/gui.h>
#include <input/input.h>

/* generuje se automaticky z obrazku ve slozce images */
#include <gta6_pad_icons.h>

/* Ukonceni: 3x rychle tukni na Zpet */
#define EXIT_TAPS 3 // kolik tuknuti
#define TAP_MAX_MS 300 // max. delka jednoho tuknuti
#define TAP_GAP_MS 500 // max. pauza mezi tuknutimi

/* ======================================================================
 *  ROZLOZENI TLACITEK – tady si ho muzes kdykoli zmenit
 * ====================================================================== */

typedef enum {
    RoleNone = 0,
    RoleStickUp,
    RoleStickDown,
    RoleStickLeft,
    RoleStickRight,
    RoleA,
    RoleB,
    RoleX,
    RoleY,
    RoleLB,
    RoleRB,
    RoleLT,
    RoleRT,
    RoleView, /* v GTA VI otevira mapu */
    RoleMenu, /* pauza */
    RoleL3,
    RoleR3,
    RoleCount,
} PadRole;

/* Bez shiftu */
static const PadRole map_normal[InputKeyMAX] = {
    [InputKeyUp] = RoleStickUp,
    [InputKeyDown] = RoleStickDown,
    [InputKeyLeft] = RoleStickLeft,
    [InputKeyRight] = RoleStickRight,
    [InputKeyOk] = RoleA,
};

/* Se shiftem (drzis Zpet) */
static const PadRole map_shift[InputKeyMAX] = {
    [InputKeyUp] = RoleRT,
    [InputKeyDown] = RoleLT,
    [InputKeyLeft] = RoleB,
    [InputKeyRight] = RoleY,
    [InputKeyOk] = RoleX,
};

/* Popisky na displeji Flipperu */
static const char* const role_label[RoleCount] = {
    [RoleNone] = "",
    [RoleStickUp] = "^",
    [RoleStickDown] = "v",
    [RoleStickLeft] = "<",
    [RoleStickRight] = ">",
    [RoleA] = "A",
    [RoleB] = "B",
    [RoleX] = "X",
    [RoleY] = "Y",
    [RoleLB] = "LB",
    [RoleRB] = "RB",
    [RoleLT] = "LT",
    [RoleRT] = "RT",
    [RoleView] = "MAPA",
    [RoleMenu] = "MENU",
    [RoleL3] = "L3",
    [RoleR3] = "R3",
};

/* ======================================================================
 *  XInput (Xbox 360) – USB cast
 * ====================================================================== */

#define XBOX_SURFACE_EP_IN 0x81 // IN Endpoint 1

enum {
    XB_DPAD_UP = 1 << 0,
    XB_DPAD_DOWN = 1 << 1,
    XB_DPAD_LEFT = 1 << 2,
    XB_DPAD_RIGHT = 1 << 3,
    XB_START = 1 << 4, // Menu
    XB_BACK = 1 << 5, // View
    XB_L3 = 1 << 6,
    XB_R3 = 1 << 7,
    XB_LB = 1 << 8,
    XB_RB = 1 << 9,
    XB_LOGO = 1 << 10,
    XB_UNUSED = 1 << 11,
    XB_A = 1 << 12,
    XB_B = 1 << 13,
    XB_X = 1 << 14,
    XB_Y = 1 << 15,
};

struct xbox_control_surface {
    uint8_t message_type; // 0x00
    uint8_t length; // 20
    uint16_t buttons;
    uint8_t left_trigger; // 0-255
    uint8_t right_trigger;
    int16_t left_x;
    int16_t left_y;
    int16_t right_x;
    int16_t right_y;
    uint8_t reserved[6];
} FURI_PACKED;

static struct xbox_control_surface current_surface;

struct usb_unknown0_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t u1, u2, u3, u4;
    uint8_t bEndpointAddress;
    uint8_t bMaxDataSize;
    uint8_t u5, u6, u7, u8, u9;
    uint8_t bEndpointAddress2;
    uint8_t bMaxDataSize2;
    uint8_t u10, u11;
};

struct usb_unknown1_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t u1, u2, u3, u4;
    uint8_t bEndpointAddress;
    uint8_t bMaxDataSize;
    uint8_t u5;
    uint8_t bEndpointAddress2;
    uint8_t bMaxDataSize2;
    uint8_t u6;
    uint8_t bEndpointAddress3;
    uint8_t bMaxDataSize3;
    uint8_t u7, u8, u9, u10, u11, u12;
    uint8_t bEndpointAddress4;
    uint8_t bMaxDataSize4;
    uint8_t u13, u14, u15, u16, u17;
};

struct usb_unknown2_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t u1, u2, u3, u4;
    uint8_t bEndpointAddress;
    uint8_t bMaxDataSize;
    uint8_t u5;
};

struct usb_unknown3_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t u1, u2, u3, u4;
};

struct XboxIntf0Descriptor {
    struct usb_interface_descriptor hid;
    struct usb_unknown0_descriptor unknown;
    struct usb_endpoint_descriptor hid_ep_in;
    struct usb_endpoint_descriptor hid_ep_out;
};

struct XboxIntf1Descriptor {
    struct usb_interface_descriptor hid;
    struct usb_unknown1_descriptor unknown;
    struct usb_endpoint_descriptor hid_ep_in_2;
    struct usb_endpoint_descriptor hid_ep_out_2;
    struct usb_endpoint_descriptor hid_ep_in_3;
    struct usb_endpoint_descriptor hid_ep_out_3;
};

struct XboxIntf2Descriptor {
    struct usb_interface_descriptor hid;
    struct usb_unknown2_descriptor unknown;
    struct usb_endpoint_descriptor hid_ep_in_4;
};

struct XboxIntf3Descriptor {
    struct usb_interface_descriptor hid;
    struct usb_unknown3_descriptor unknown;
};

struct XboxConfigDescriptor {
    struct usb_config_descriptor config;
    struct XboxIntf0Descriptor intf_0;
    struct XboxIntf1Descriptor intf_1;
    struct XboxIntf2Descriptor intf_2;
    struct XboxIntf3Descriptor intf_3;
} FURI_PACKED;

static struct usb_device_descriptor xbox_device_desc = {
    .bLength = 0x12,
    .bDescriptorType = 0x01,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0xFF,
    .bDeviceSubClass = 0xFF,
    .bDeviceProtocol = 0xFF,
    .bMaxPacketSize0 = 0x08,
    .idVendor = 0x045E, // Microsoft
    .idProduct = 0x028E, // Xbox 360 Controller
    .bcdDevice = 0x0114,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

static const struct XboxConfigDescriptor xbox_cfg_desc = {
    .config =
        {
            .bLength = 9,
            .bDescriptorType = 0x02,
            .wTotalLength = 153,
            .bNumInterfaces = 4,
            .bConfigurationValue = 1,
            .iConfiguration = 0,
            .bmAttributes = 0b10100000,
            .bMaxPower = USB_CFG_POWER_MA(500),
        },
    .intf_0 =
        {
            .hid =
                {
                    .bLength = 9,
                    .bDescriptorType = 0x04,
                    .bInterfaceNumber = 0,
                    .bAlternateSetting = 0,
                    .bNumEndpoints = 2,
                    .bInterfaceClass = 0xFF,
                    .bInterfaceSubClass = 0x5D,
                    .bInterfaceProtocol = 0x01,
                    .iInterface = 0,
                },
            .unknown =
                {
                    .bLength = 17,
                    .bDescriptorType = 0x21,
                    .u1 = 0x00,
                    .u2 = 0x01,
                    .u3 = 0x01,
                    .u4 = 0x25,
                    .bEndpointAddress = 0x81,
                    .bMaxDataSize = 20,
                    .u5 = 0x00,
                    .u6 = 0x00,
                    .u7 = 0x00,
                    .u8 = 0x00,
                    .u9 = 0x13,
                    .bEndpointAddress2 = 0x01,
                    .bMaxDataSize2 = 8,
                    .u10 = 0x00,
                    .u11 = 0x00,
                },
            .hid_ep_in =
                {
                    .bLength = 7,
                    .bDescriptorType = 0x05,
                    .bEndpointAddress = 0x81,
                    .bmAttributes = 0x03,
                    .wMaxPacketSize = 32,
                    .bInterval = 4,
                },
            .hid_ep_out =
                {
                    .bLength = 7,
                    .bDescriptorType = 0x05,
                    .bEndpointAddress = 0x01,
                    .bmAttributes = 0x03,
                    .wMaxPacketSize = 32,
                    .bInterval = 8,
                },
        },
    .intf_1 =
        {
            .hid =
                {
                    .bLength = 9,
                    .bDescriptorType = 0x04,
                    .bInterfaceNumber = 1,
                    .bAlternateSetting = 0,
                    .bNumEndpoints = 4,
                    .bInterfaceClass = 0xFF,
                    .bInterfaceSubClass = 0x5D,
                    .bInterfaceProtocol = 0x03,
                    .iInterface = 0,
                },
            .unknown =
                {
                    .bLength = 27,
                    .bDescriptorType = 0x21,
                    .u1 = 0x00,
                    .u2 = 0x01,
                    .u3 = 0x01,
                    .u4 = 0x01,
                    .bEndpointAddress = 0x82,
                    .bMaxDataSize = 64,
                    .u5 = 0x01,
                    .bEndpointAddress2 = 0x02,
                    .bMaxDataSize2 = 32,
                    .u6 = 0x16,
                    .bEndpointAddress3 = 0x83,
                    .bMaxDataSize3 = 0,
                    .u7 = 0x00,
                    .u8 = 0x00,
                    .u9 = 0x00,
                    .u10 = 0x00,
                    .u11 = 0x00,
                    .u12 = 0x16,
                    .bEndpointAddress4 = 0x03,
                    .bMaxDataSize4 = 0,
                    .u13 = 0x00,
                    .u14 = 0x00,
                    .u15 = 0x00,
                    .u16 = 0x00,
                    .u17 = 0x00,
                },
            .hid_ep_in_2 =
                {
                    .bLength = 7,
                    .bDescriptorType = 0x05,
                    .bEndpointAddress = 0x82,
                    .bmAttributes = 0x03,
                    .wMaxPacketSize = 32,
                    .bInterval = 2,
                },
            .hid_ep_out_2 =
                {
                    .bLength = 7,
                    .bDescriptorType = 0x05,
                    .bEndpointAddress = 0x02,
                    .bmAttributes = 0x03,
                    .wMaxPacketSize = 32,
                    .bInterval = 4,
                },
            .hid_ep_in_3 =
                {
                    .bLength = 7,
                    .bDescriptorType = 0x05,
                    .bEndpointAddress = 0x83,
                    .bmAttributes = 0x03,
                    .wMaxPacketSize = 32,
                    .bInterval = 64,
                },
            .hid_ep_out_3 =
                {
                    .bLength = 7,
                    .bDescriptorType = 0x05,
                    .bEndpointAddress = 0x03,
                    .bmAttributes = 0x03,
                    .wMaxPacketSize = 32,
                    .bInterval = 16,
                },
        },
    .intf_2 =
        {
            .hid =
                {
                    .bLength = 9,
                    .bDescriptorType = 0x04,
                    .bInterfaceNumber = 2,
                    .bAlternateSetting = 0,
                    .bNumEndpoints = 1,
                    .bInterfaceClass = 0xFF,
                    .bInterfaceSubClass = 0x5D,
                    .bInterfaceProtocol = 0x02,
                    .iInterface = 0,
                },
            .unknown =
                {
                    .bLength = 9,
                    .bDescriptorType = 0x21,
                    .u1 = 0x00,
                    .u2 = 0x01,
                    .u3 = 0x01,
                    .u4 = 0x22,
                    .bEndpointAddress = 0x84,
                    .bMaxDataSize = 7,
                    .u5 = 0x00,
                },
            .hid_ep_in_4 =
                {
                    .bLength = 7,
                    .bDescriptorType = 0x05,
                    .bEndpointAddress = 0x84,
                    .bmAttributes = 0x03,
                    .wMaxPacketSize = 32,
                    .bInterval = 16,
                },
        },
    .intf_3 =
        {
            .hid =
                {
                    .bLength = 9,
                    .bDescriptorType = 0x04,
                    .bInterfaceNumber = 3,
                    .bAlternateSetting = 0,
                    .bNumEndpoints = 0,
                    .bInterfaceClass = 0xFF,
                    .bInterfaceSubClass = 0xFD,
                    .bInterfaceProtocol = 0x13,
                    .iInterface = 4,
                },
            .unknown =
                {
                    .bLength = 6,
                    .bDescriptorType = 0x41,
                    .u1 = 0x00,
                    .u2 = 0x01,
                    .u3 = 0x01,
                    .u4 = 0x03,
                },
        },
};

static bool boot_protocol = false;
static void* usb_xbox_strings[3] = {NULL, NULL, NULL}; // vyrobce, produkt, seriove cislo
static usbd_device* usb_dev;
static FuriSemaphore* hid_semaphore = NULL;
static volatile bool hid_connected = false;
static volatile bool hid_configured = false; // pocitac uz endpoint nastavil

static void* hid_set_string_descr(const char* str) {
    furi_assert(str);

    size_t len = strlen(str);
    struct usb_string_descriptor* dev_str_desc = malloc(len * 2 + 2);
    dev_str_desc->bLength = len * 2 + 2;
    dev_str_desc->bDescriptorType = USB_DTYPE_STRING;
    for(size_t i = 0; i < len; i++)
        dev_str_desc->wString[i] = str[i];

    return dev_str_desc;
}

static void hid_txrx_ep_callback(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(dev);
    UNUSED(ep);
    if(event == usbd_evt_eptx) {
        furi_semaphore_release(hid_semaphore);
    }
}

static usbd_respond hid_ep_config(usbd_device* dev, uint8_t cfg) {
    switch(cfg) {
    case 0:
        hid_configured = false;
        usbd_ep_deconfig(dev, XBOX_SURFACE_EP_IN);
        usbd_reg_endpoint(dev, XBOX_SURFACE_EP_IN, 0);
        return usbd_ack;
    case 1:
        usbd_ep_config(dev, XBOX_SURFACE_EP_IN, USB_EPTYPE_INTERRUPT, 32);
        usbd_reg_endpoint(dev, XBOX_SURFACE_EP_IN, hid_txrx_ep_callback);
        boot_protocol = false;
        /* po (znovu)nastaveni zacit s volnym semaforem */
        if(hid_semaphore != NULL) furi_semaphore_release(hid_semaphore);
        hid_configured = true;
        return usbd_ack;
    default:
        return usbd_fail;
    }
}

static usbd_respond hid_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    UNUSED(callback);
    if(((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) ==
           (USB_REQ_INTERFACE | USB_REQ_CLASS) &&
       req->wIndex == 0) {
        switch(req->bRequest) {
        case USB_HID_SETIDLE:
            return usbd_ack;
        case USB_HID_GETREPORT:
            return usbd_ack;
        case USB_HID_SETPROTOCOL:
            if(req->wValue == 0)
                boot_protocol = true;
            else if(req->wValue == 1)
                boot_protocol = false;
            else
                return usbd_fail;
            return usbd_ack;
        default:
            return usbd_fail;
        }
    }
    if(((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) ==
           (USB_REQ_INTERFACE | USB_REQ_STANDARD) &&
       req->wIndex == 0 && req->bRequest == USB_STD_GET_DESCRIPTOR) {
        switch(req->wValue >> 8) {
        case USB_DTYPE_HID:
            dev->status.data_ptr = (uint8_t*)&(xbox_cfg_desc.intf_0.hid);
            dev->status.data_count = sizeof(xbox_cfg_desc.intf_0.hid);
            return usbd_ack;
        default:
            return usbd_fail;
        }
    }
    return usbd_fail;
}

static void hid_deinit(usbd_device* dev) {
    /* uklidit vsechno, co ukazuje do aplikace, aby po jejim zavreni nic nespadlo */
    hid_configured = false;
    hid_connected = false;
    usbd_ep_deconfig(dev, XBOX_SURFACE_EP_IN);
    usbd_reg_endpoint(dev, XBOX_SURFACE_EP_IN, NULL);
    usbd_reg_config(dev, NULL);
    usbd_reg_control(dev, NULL);

    free(usb_xbox_strings[0]);
    free(usb_xbox_strings[1]);
    free(usb_xbox_strings[2]);
    usb_xbox_strings[0] = NULL;
    usb_xbox_strings[1] = NULL;
    usb_xbox_strings[2] = NULL;
}

static void hid_on_wakeup(usbd_device* dev) {
    UNUSED(dev);
    hid_connected = true;
}

static void hid_on_suspend(usbd_device* dev) {
    UNUSED(dev);
    if(hid_connected) {
        hid_connected = false;
        hid_configured = false;
        furi_semaphore_release(hid_semaphore);
    }
}

static bool hid_send_report(void) {
    if((hid_semaphore == NULL) || !hid_connected || !hid_configured) return false;
    FuriStatus status = furi_semaphore_acquire(hid_semaphore, 16);
    if(status == FuriStatusErrorTimeout) {
        return false;
    }
    furi_check(status == FuriStatusOk);
    if(hid_connected == false) {
        return false;
    }
    if(boot_protocol != true) {
        usbd_ep_write(usb_dev, XBOX_SURFACE_EP_IN, &current_surface, sizeof(current_surface));
    }
    return true;
}

static void hid_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx);

static FuriHalUsbInterface usb_xbox = {
    .init = hid_init,
    .deinit = hid_deinit,
    .wakeup = hid_on_wakeup,
    .suspend = hid_on_suspend,

    .dev_descr = (struct usb_device_descriptor*)&xbox_device_desc,

    .str_manuf_descr = NULL,
    .str_prod_descr = NULL,
    .str_serial_descr = NULL,

    .cfg_descr = (void*)&xbox_cfg_desc,
};

static void hid_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    UNUSED(intf);
    UNUSED(ctx);
    if(hid_semaphore == NULL) hid_semaphore = furi_semaphore_alloc(1, 1);
    usb_dev = dev;

    if(usb_xbox_strings[0] == NULL) usb_xbox_strings[0] = hid_set_string_descr("Microsoft Corporation");
    if(usb_xbox_strings[1] == NULL) usb_xbox_strings[1] = hid_set_string_descr("Controller");
    if(usb_xbox_strings[2] == NULL) usb_xbox_strings[2] = hid_set_string_descr("08FEC93");
    usb_xbox.str_manuf_descr = usb_xbox_strings[0];
    usb_xbox.str_prod_descr = usb_xbox_strings[1];
    usb_xbox.str_serial_descr = usb_xbox_strings[2];

    usbd_reg_config(dev, hid_ep_config);
    usbd_reg_control(dev, hid_control);
    usbd_connect(dev, true);
}

/* ======================================================================
 *  Logika tlacitek
 * ====================================================================== */

typedef struct {
    PadRole active[InputKeyMAX]; // co prave dela kazde fyzicke tlacitko
    bool shift; // drzi se Zpet
    bool shift_used; // behem drzeni Zpet se zmacklo jine tlacitko
    uint32_t shift_tick; // kdy se Zpet zmacklo
    uint8_t tap_count; // kolik rychlych tuknuti na Zpet za sebou
    uint32_t last_tap_tick; // kdy bylo posledni tuknuti
} PadState;

static PadState pad;

static void pad_update_surface(void) {
    bool on[RoleCount];
    memset(on, 0, sizeof(on));
    for(int k = 0; k < (int)InputKeyMAX; k++) {
        on[pad.active[k]] = true;
    }

    int16_t x = 0;
    int16_t y = 0;
    if(on[RoleStickRight] && !on[RoleStickLeft]) x = 32767;
    if(on[RoleStickLeft] && !on[RoleStickRight]) x = -32767;
    if(on[RoleStickUp] && !on[RoleStickDown]) y = 32767;
    if(on[RoleStickDown] && !on[RoleStickUp]) y = -32767;

    uint16_t buttons = 0;
    if(on[RoleA]) buttons |= XB_A;
    if(on[RoleB]) buttons |= XB_B;
    if(on[RoleX]) buttons |= XB_X;
    if(on[RoleY]) buttons |= XB_Y;
    if(on[RoleLB]) buttons |= XB_LB;
    if(on[RoleRB]) buttons |= XB_RB;
    if(on[RoleView]) buttons |= XB_BACK;
    if(on[RoleMenu]) buttons |= XB_START;
    if(on[RoleL3]) buttons |= XB_L3;
    if(on[RoleR3]) buttons |= XB_R3;

    current_surface.message_type = 0x00;
    current_surface.length = sizeof(current_surface); // 20
    current_surface.buttons = buttons;
    current_surface.left_x = x;
    current_surface.left_y = y;
    current_surface.right_x = 0;
    current_surface.right_y = 0;
    current_surface.left_trigger = on[RoleLT] ? 255 : 0;
    current_surface.right_trigger = on[RoleRT] ? 255 : 0;
}

/* Drzi se ted nejake jine tlacitko nez Zpet? */
static bool pad_any_key_active(void) {
    for(int k = 0; k < (int)InputKeyMAX; k++) {
        if(k != InputKeyBack && pad.active[k] != RoleNone) return true;
    }
    return false;
}

/* Vraci false, kdyz se ma aplikace ukoncit */
static bool pad_handle_input(const InputEvent* event) {
    if(event->key == InputKeyBack) {
        if(event->type == InputTypePress) {
            pad.shift = true;
            /* kdyz uz drzis jine tlacitko, neni to tuknuti pro ukonceni */
            pad.shift_used = pad_any_key_active();
            pad.shift_tick = furi_get_tick();
        } else if(event->type == InputTypeRelease) {
            pad.shift = false;
            /* na Flipperu je 1 tick = 1 ms */
            uint32_t now = furi_get_tick();
            bool is_tap = !pad.shift_used && (now - pad.shift_tick) <= TAP_MAX_MS;
            if(!is_tap) {
                /* Zpet se pouzilo jako shift nebo se drzelo dlouho */
                pad.tap_count = 0;
            } else {
                if(pad.tap_count > 0 && (now - pad.last_tap_tick) > TAP_GAP_MS) {
                    pad.tap_count = 0; // moc dlouha pauza, pocitame znovu
                }
                pad.tap_count++;
                pad.last_tap_tick = now;
                if(pad.tap_count >= EXIT_TAPS) {
                    return false;
                }
            }
        }
        return true;
    }

    if(event->key >= InputKeyMAX) return true;

    if(event->type == InputTypePress) {
        /* jakekoli jine tlacitko prerusi pocitani tuknuti na Zpet */
        pad.tap_count = 0;
        if(pad.shift) pad.shift_used = true;
        /* funkce tlacitka se urci v okamziku stisku a drzi do pusteni */
        pad.active[event->key] = pad.shift ? map_shift[event->key] : map_normal[event->key];
    } else if(event->type == InputTypeRelease) {
        pad.active[event->key] = RoleNone;
    }
    return true;
}

/* ======================================================================
 *  Displej
 * ====================================================================== */

#define KEY_W 24
#define KEY_H 13

static void draw_key(Canvas* canvas, int32_t x, int32_t y, InputKey key) {
    PadRole held = pad.active[key];
    PadRole role = held != RoleNone ? held : (pad.shift ? map_shift[key] : map_normal[key]);
    const char* label = role_label[role];

    if(held != RoleNone) {
        canvas_draw_rbox(canvas, x, y, KEY_W, KEY_H, 2);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, KEY_W, KEY_H, 2);
    }
    canvas_draw_str_aligned(canvas, x + KEY_W / 2, y + KEY_H / 2 + 1, AlignCenter, AlignCenter, label);
    canvas_set_color(canvas, ColorBlack);
}

static void app_draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(!hid_connected) {
        canvas_draw_icon(canvas, 5, 20, &I_usb);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 58, 30, "Pripojuji...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 58, 42, "Zapoj USB");
        return;
    }

    /* krizova klavesnice vlevo */
    canvas_set_font(canvas, FontSecondary);
    draw_key(canvas, 24, 13, InputKeyUp);
    draw_key(canvas, 0, 28, InputKeyLeft);
    draw_key(canvas, 24, 28, InputKeyOk);
    draw_key(canvas, 48, 28, InputKeyRight);
    draw_key(canvas, 24, 43, InputKeyDown);

    /* pravy sloupec */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 102, 2, AlignCenter, AlignTop, "GTA VI");

    canvas_set_font(canvas, FontSecondary);
    if(pad.shift) {
        canvas_draw_rbox(canvas, 78, 16, 48, 13, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 102, 23, AlignCenter, AlignCenter, "SHIFT");
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_rframe(canvas, 78, 16, 48, 13, 2);
        canvas_draw_str_aligned(canvas, 102, 23, AlignCenter, AlignCenter, "Zpet=shift");
    }

    canvas_draw_str_aligned(canvas, 102, 40, AlignCenter, AlignCenter, "3x Zpet");
    canvas_draw_str_aligned(canvas, 102, 51, AlignCenter, AlignCenter, "= konec");
}

static void app_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

/* ======================================================================
 *  Hlavni funkce
 * ====================================================================== */

int32_t gta6_pad_app(void* p) {
    UNUSED(p);

    memset(&pad, 0, sizeof(pad));
    memset(&current_surface, 0, sizeof(current_surface));
    pad_update_surface();

    FuriMessageQueue* event_queue = furi_message_queue_alloc(16, sizeof(InputEvent));
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, app_draw_callback, NULL);
    view_port_input_callback_set(view_port, app_input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    /* zapamatovat si puvodni USB rezim (normalne propojeni s pocitacem) */
    FuriHalUsbInterface* usb_mode_prev = furi_hal_usb_get_config();
    furi_check(furi_hal_usb_set_config(&usb_xbox, NULL));

    InputEvent input_event;
    bool running = true;

    while(running) {
        if(furi_message_queue_get(event_queue, &input_event, 50) == FuriStatusOk) {
            running = pad_handle_input(&input_event);
            pad_update_surface();
        }
        /* posilame stav pravidelne, aby se nic "nezaseklo" */
        hid_send_report();
        view_port_update(view_port);
    }

    /* pustit vsechna tlacitka */
    memset(&pad, 0, sizeof(pad));
    pad_update_surface();
    hid_send_report();

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_record_close(RECORD_GUI);

    /* vratit puvodni USB rezim – bez restartu Flipperu */
    furi_check(furi_hal_usb_set_config(usb_mode_prev, NULL));

    if(hid_semaphore != NULL) {
        furi_semaphore_free(hid_semaphore);
        hid_semaphore = NULL;
    }

    return 0;
}

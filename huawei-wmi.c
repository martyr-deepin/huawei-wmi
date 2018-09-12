/*
 * Huawei WMI hotkey driver
 *
 * Copyright(C) 2018 deepin.
 * Copyright(C) 2018-2018 xiabin <snyh@snyh.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>
#include <linux/dmi.h>
#include <linux/acpi.h>

#define	HUAWEI_WMI_FILE	"huawei-wmi"

MODULE_AUTHOR("xiabin <snyh@snyh.org>");
MODULE_DESCRIPTION("Huawei PC WMI Hotkey Driver");
MODULE_LICENSE("GPL");

#define HUAWEI_WMI_EVENT_GUID	"ABBC0F5C-8EA1-11D1-A000-C90629100000"

MODULE_ALIAS("wmi:"HUAWEI_WMI_EVENT_GUID);

static const struct key_entry huawei_wmi_keymap[] = {
  // xkb use 190+8 instead of KEY_MICMUTE for mapping XF86AudioMicMute
  { KE_KEY, 0x287, { 190 } }, //KEY_MICMUTE } },
  { KE_KEY, 0x289, { KEY_WLAN } },
  { KE_KEY, 0x28a, { KEY_CONTROLPANEL } },
  { KE_KEY, 0x281, { KEY_BRIGHTNESSDOWN} },
  { KE_KEY, 0x282, { KEY_BRIGHTNESSUP}},
  { KE_END, 0 },
};

static struct input_dev *huawei_wmi_input_dev = 0;

static void huawei_wmi_notify(u32 value, void *context);

static int huawei_wmi_input_setup(void)
{
    acpi_status status;
    int err;

    huawei_wmi_input_dev = input_allocate_device();

    if (!huawei_wmi_input_dev)
        return -ENOMEM;

    huawei_wmi_input_dev->name = "HUAWEI WMI hotkeys";
    huawei_wmi_input_dev->phys = "wmi/input0";
    huawei_wmi_input_dev->id.bustype = BUS_HOST;

    err = sparse_keymap_setup(huawei_wmi_input_dev, huawei_wmi_keymap, NULL);
    if (err)
        goto err_free_dev;

    status = wmi_install_notify_handler(HUAWEI_WMI_EVENT_GUID,
                                      huawei_wmi_notify, NULL);
    if (ACPI_FAILURE(status)) {
      err = -EIO;
      goto err_free_dev;
    }

    err = input_register_device(huawei_wmi_input_dev);
    if (err)
        goto err_uninstall_notifier;

    return 0;

err_uninstall_notifier:
    wmi_remove_notify_handler(HUAWEI_WMI_EVENT_GUID);
err_free_dev:
    input_free_device(huawei_wmi_input_dev);
    return err;
}


static const struct dmi_system_id huawei_whitelist[] __initconst = {
    {
        .ident = "HUAWEI",
        .matches = {
            DMI_MATCH(DMI_SYS_VENDOR, "HUAWEI"),
        },
    },
    {}
};

static void huawei_wmi_notify(u32 value, void *context)
{
  struct acpi_buffer response = { ACPI_ALLOCATE_BUFFER, NULL };
  union acpi_object *obj;
  acpi_status status;
  int code;

  status = wmi_get_event_data(value, &response);
  if (status != AE_OK) {
    pr_err("bad event status 0x%x\n", status);
    return;
  }

  obj = (union acpi_object *)response.pointer;

  if (obj && obj->type == ACPI_TYPE_INTEGER) {
    code = obj->integer.value;
    /* switch (code) { */
    /* case 0x287: */
    /*   pr_info("RECEVED KEY 0x%x(KEY_MICMUTE)pressed\n", code); */
    /* case 0x289: */
    /*   pr_info("RECEVED KEY 0x%x(WIFI TOGGLE)pressed\n", code); */
    /* case 0x28a: */
    /*   pr_info("RECEVED KEY 0x%x(Huawei Manager)pressed\n", code); */
    /*   break; */
    /* default: */
    /*   pr_info("RECEVED Unknown KEY 0x%x pressed\n", code); */
    /* } */
    sparse_keymap_report_event(huawei_wmi_input_dev, code, 1, true);
  } else {
    pr_info("Received unknown events %p pressed\n", obj);
  }
  kfree(obj);
}

static int __init huawei_wmi_init(void)
{
  if (!dmi_check_system(huawei_whitelist)) {
    pr_warning("This isn't a huawei laptop.\n");
    return -ENODEV;
  }

  if (!wmi_has_guid(HUAWEI_WMI_EVENT_GUID)) {
    pr_warning("No known WMI GUID found\n");
    return -ENODEV;
  }

  return huawei_wmi_input_setup();
}

static void __exit huawei_wmi_exit(void)
{
  wmi_remove_notify_handler(HUAWEI_WMI_EVENT_GUID);
  if (huawei_wmi_input_dev)
    input_unregister_device(huawei_wmi_input_dev);
}

module_init(huawei_wmi_init);
module_exit(huawei_wmi_exit);
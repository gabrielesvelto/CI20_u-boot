/*
 * Fastboot nand storage implementation
 *
 * Copyright (c) 2017 Imagination Technologies
 * Author: Chris Larsen <chris.larsen@imgtec.com>
 * Author: Dragan Cecavac <dragan.cecavac@imgtec.com>
 *
 * Large portions of this code were taken from X-Boot but those files
 * don't have attribution information.  Consequently, attribution is
 * missing in this file.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include "fastboot_storage.h"

void storage_info_dump(void)
{
	if (do_mtdparts_default()) {	// mtdparts default
		return;
	}

	if (ubi_part("boot", NULL)) {	// ubi part boot
		debug("%s, line %d:  UBI partition \"boot\" does not exist\n",
		      __FILE__, __LINE__);
		return;
	}

	dump_ubi_volume_table("boot");

	if (ubi_part("system", NULL)) {	// ubi part system
		debug("%s, line %d:  UBI partition \"system\" does not exist\n",
			__FILE__, __LINE__);
		return;
	}

	dump_ubi_volume_table("system");
}

void cmd_flash_core(USB_STATUS * status, char *par_name,
	unsigned char *Bulk_Data_Buf, int image_size)
{
	// Things get a bit tricky here.  What Fastboot calls a
	// partition, UBI calls a volume.  Before we can write the UBI
	// volume we must first select the appropriate UBI partition.
	// All of the Android partitions exist as volumes in the UBI
	// volume "system" (not to be confused with the Android
	// partition "system").
	if (do_mtdparts_default()) {	// mtdparts default
		tx_status(status, FASTBOOT_REPLY_FAIL
			  "do_mtdparts_default() failed");
		return;
	}

	if (!strcmp(par_name, FB_PARTITION_BOOT) ||
	    !strcmp(par_name, FB_PARTITION_RECOVERY)) {
		if (ubi_part("boot", NULL)) {	// ubi part boot
			debug("%s, line %d:  UBI partition \"boot\" does not "
			      "exist\n", __FILE__, __LINE__);
			tx_status(status, FASTBOOT_REPLY_FAIL
				  "UBI partition \"boot\" does not exist");
			return;
		}

		if(check_boot_magic(status, par_name, Bulk_Data_Buf))
			return;
		debug("%s, line %d: valid boot image being written to \"%s\" "
		      "partition\n", __FILE__, __LINE__, par_name);
	} else {
		if (ubi_part("system", NULL)) {	// ubi part system
			debug("%s, line %d:  UBI partition \"system\" does not "
			      "exist\n", __FILE__, __LINE__);
			tx_status(status, FASTBOOT_REPLY_FAIL
				  "UBI partition \"system\" does not exist");
			return;
		}
	}

	// Now we need to write the UBI volume within the UBI partition
	// "system" which corresponds to the Android partition.
	printf("writing %08lx bytes to %s\n", image_size, par_name);
	int ret = ubi_volume_write(par_name, Bulk_Data_Buf, image_size);

	if (ret) {
		debug("%s, line %d:  FAILflash write \"%s\" failure\n",
		      __FILE__, __LINE__, par_name);
		tx_status(status, FASTBOOT_REPLY_FAIL "flash write failure");
	} else {
		debug("%s, line %d:  OKAY writing partition \"%s\"\n",
		      __FILE__, __LINE__, par_name);
		tx_status(status, FASTBOOT_REPLY_OKAY);
	}
}

void cmd_erase_core(USB_STATUS * status, char *ptnparam, char *par_name)
{
	// Things get a bit tricky here.  What Fastboot calls a
	// partition, UBI calls a volume.  Before we can write the UBI
	// volume we must first select the appropriate UBI partition.
	// All of the Android partitions exist as volumes in the UBI
	// volume "system" (not to be confused with the Android
	// partition "system").
	if (do_mtdparts_default()) {	// mtdparts default
		tx_status(status, FASTBOOT_REPLY_FAIL
			  "do_mtdparts_default() failed");
		return;
	}

	if (ubi_part("boot", NULL)) {	// ubi part boot
		debug("%s, line %d:  UBI partition \"boot\" does not exist\n",
		      __FILE__, __LINE__);
		tx_status(status, FASTBOOT_REPLY_FAIL
			  "UBI partition \"boot\" does not exist");
		return;
	}

	if (ubi_volume_exists(par_name)) {
		// For now, this command doesn't do any work.  Erasing
		// a UBI volume isn't a prerequisite for rewriting the
		// volume.  By avoiding this work, it's hoped that the
		// life of the NAND will be extended.
		//
		// flash_erase(...);
		tx_status(status, FASTBOOT_REPLY_OKAY);
		return;
	}


	if (ubi_part("system", NULL)) {	// ubi part system
		debug("%s, line %d:  UBI partition \"system\" does not exist\n",
		      __FILE__, __LINE__);
		tx_status(status, FASTBOOT_REPLY_FAIL
			  "UBI partition \"system\" does not exist");
		return;
	}

	if (ubi_volume_exists(par_name)) {
		// For now, this command doesn't do any work.  Erasing
		// a UBI volume isn't a prerequisite for rewriting the
		// volume.  By avoiding this work, it's hoped that the
		// life of the NAND will be extended.
		//
		// flash_erase(...);
		tx_status(status, FASTBOOT_REPLY_OKAY);
	} else {
		tx_status(status, FASTBOOT_REPLY_FAIL
			  "partition %s does not exist", ptnparam);
	}
}

void cmd_getvar_partition_type(char *buf, char *ptnparam) {
	if (!strcmp(FB_PARTITION_BOOT, ptnparam) ||
		!strcmp(FB_PARTITION_RECOVERY, ptnparam)) {
		snprintf(buf, sizeof(buf), FASTBOOT_REPLY_OKAY "%s",
			 "raw");
	} else if (!strcmp(FB_PARTITION_SYSTEM, ptnparam) ||
		!strcmp(FB_PARTITION_CACHE, ptnparam) ||
		!strcmp(FB_PARTITION_USERDATA, ptnparam)) {
		snprintf(buf, sizeof(buf), FASTBOOT_REPLY_OKAY "%s",
			 "ubifs");
	} else {
		snprintf(buf, sizeof(buf), FASTBOOT_REPLY_OKAY "%s",
			 "UNKNOWN");
	}
}

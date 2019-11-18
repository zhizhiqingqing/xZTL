#include <stdlib.h>
#include <xapp.h>
#include <xapp-media.h>
#include <xapp-ztl.h>
#include <ztl-media.h>
#include <libzrocks.h>

#define ZROCKS_DEBUG 0

void *zrocks_alloc (size_t size)
{
    uint64_t phys;
    return xapp_media_dma_alloc (size, &phys);
}

void zrocks_free (void *ptr)
{
    xapp_media_dma_free (ptr);
}

int zrocks_write (void *buf, uint32_t size, uint8_t level, uint64_t *addr)
{
    // TODO:
    // add parameter for level in struct ucmd
    // create ucmd
    // populate ucmd
    // set app_md to 1
    // wait until completed == ncmd
    // populate *addr
    // return
    return 0;
}

int zrocks_read (uint64_t offset, void *buf, uint64_t size)
{
    // TODO:
    // read directly from device
    return 0;
}

int zrocks_new (uint64_t id, void *buf, uint32_t size, uint8_t level)
{
    struct xapp_io_ucmd ucmd;

    /* For now, we only support level 0 */
    if (level)
	return -1;

    ucmd.id        = id;
    ucmd.prov_type = level;
    ucmd.buf       = buf;
    ucmd.size      = size;
    ucmd.app_md    = 0;
    ucmd.status    = 0;
    ucmd.completed = 0;
    ucmd.callback  = NULL;
    ucmd.prov      = NULL;

    if (ztl()->wca->submit_fn (&ucmd))
	return -1;

    /* Wait for asynchronous command */
    while (!ucmd.completed) {
	usleep (1);
    }

    return ucmd.status;
}

int zrocks_delete (uint64_t id)
{
    uint64_t old;

    return ztl()->map->upsert_fn (id, 0, &old, 0);
}

int zrocks_read_obj (uint64_t id, uint64_t offset, void *buf, uint32_t size)
{
    struct xapp_io_mcmd cmd;
    uint64_t objsec_off, usersec_off;
    int ret;

    if (ZROCKS_DEBUG)
	log_infoa ("zrocks (read): ID %lu, off %lu, size %d\n",
							id, offset, size);

    /* TODO: Accept reads larger than 512 KB
     * Multiple media commands are necessary for larger reads */
    if (size > 128 * 4096)
	return -1;

    cmd.opcode  = XAPP_CMD_READ;
    cmd.naddr   = 1;
    cmd.synch   = 1;
    cmd.prp[0]  = (uint64_t) buf;
    cmd.nsec[0] = size / ZNS_ALIGMENT;
    cmd.status  = 0;
    if (size % ZNS_ALIGMENT != 0)
	cmd.nsec[0] += 2;

    /* This assumes a single zone offset per object */
    usersec_off = offset / ZNS_ALIGMENT;
    objsec_off  = ztl()->map->read_fn (id);

    cmd.addr[0].addr = 0;
    cmd.addr[0].g.sect = objsec_off + usersec_off;

    if (ZROCKS_DEBUG)
	log_infoa ("  objsec_off %lx, usersec_off %lu, nsec %lu\n",
    				    objsec_off, usersec_off, cmd.nsec[0]);

    ret = xapp_media_submit_io (&cmd);
    if (ret || cmd.status) {
	log_erra ("zrocks: Read failure. ID %lu, off 0x%lx, sz %d\n",
						    id, offset, size);
    } else if (offset % ZNS_ALIGMENT != 0) {
	memcpy (buf, (char *)buf + (offset % ZNS_ALIGMENT), size);
    }

    return (!ret) ? cmd.status : ret;
}

int zrocks_exit (void)
{
    return xapp_exit ();
}

int zrocks_init (void)
{
    /* Add libznd media layer */
    xapp_add_media (znd_media_register);

    /* Add the ZTL modules */
    ztl_zmd_register ();
    ztl_pro_register ();
    ztl_mpe_register ();
    ztl_map_register ();
    ztl_wca_register ();

    return xapp_init ();
}

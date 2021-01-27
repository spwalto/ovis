// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 Marvell International Ltd.
 */

/*
 *  Copyright [2020] Hewlett Packard Enterprise Development LP
 * 
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of version 2 of the GNU General Public License as published 
 * by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for 
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along with 
 * this program; if not, write to:
 * 
 *   Free Software Foundation, Inc.
 *   51 Franklin Street, Fifth Floor
 *   Boston, MA 02110-1301, USA.
 */
static int read_node(struct cpu_info *d)
{
        assert(d!=NULL);
        int rv;
        struct mc_oper_region *op = &d->mcp;
        rv = lseek(d->fd, 0, SEEK_SET);
        if (rv < 0)
               return rv;
        rv = read(d->fd, op, sizeof(*op));
        if (rv < sizeof(*op))
                return rv;
        if (CMD_STATUS_READY(op->cmd_status) == 0)
                return 0;
        if (CMD_VERSION(op->cmd_status) > 0)
                d->throttling_available =  1;
        else
                d->throttling_available =  0;
        return 1;
}


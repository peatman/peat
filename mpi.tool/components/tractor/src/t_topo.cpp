/*
 * This file is part of PEAT (Performance Estimation & Analysing Toolkit).
 * Copyright (C) 2026 by Alex Knyazev
 *
 * PEAT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PEAT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PEAT.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "tractor.h"
#include <hwloc.h>
static void concatenate_children(hwloc_topology_t topo, hwloc_obj_t obj, int depth, std::string &storage)
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
    char local[64];
    unsigned i;
    if (obj == NULL) {
        debug_printf("NULL TOPO object received\n");
        return;
    }
    if (obj->type == HWLOC_OBJ_PACKAGE) {
        sprintf(local, ":%d", obj->logical_index);
        storage += local;
    }
    if (obj->type == HWLOC_OBJ_CORE) {
        sprintf(local, ",%d", obj->logical_index);
        storage += local;
    }

    if (obj->type == HWLOC_OBJ_PU) {
        //		sprintf(local, ";%d", obj->logical_index);
        //		strcat(storage, local);
        if (obj->os_index != (unsigned)-1) {
            sprintf(local, "-%d", obj->os_index);
            storage += local;
        }
    }
    for (i = 0; i < obj->arity; i++) {
        concatenate_children(topo, obj->children[i], depth + 1, storage);
    }
    __DEBUG_EPILOG_TRACTOR_LAYER__
}
void tractor::get_topo()
{
    __DEBUG_PROLOG_TRACTOR_LAYER__
#ifndef I_MPI_BUILD
    try {
        hwloc_topology_t topo;
        hwloc_topology_init(&topo);
#if HWLOC_API_VERSION < 0x00020000
        hwloc_topology_set_flags(topo, HWLOC_TOPOLOGY_FLAG_WHOLE_SYSTEM | HWLOC_TOPOLOGY_FLAG_WHOLE_IO);
#endif
        hwloc_topology_load(topo);
        concatenate_children(topo, hwloc_get_root_obj(topo), 0, node_topology);
        hwloc_topology_destroy(topo);
    } catch (...) {
        fprintf(stderr, "Acquiring Topology Failed\n");
    }
#endif
    __DEBUG_EPILOG_TRACTOR_LAYER__
}

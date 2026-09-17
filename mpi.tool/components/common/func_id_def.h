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

#ifndef __FUNC_ID_DEF_H__
#define __FUNC_ID_DEF_H__

#include "func_id.h"
#define COMMA ,
#define PROPSEP ,
#define perl_hint(x)

const Func_ID::MPIFuncs Func_ID::mpi_funcs_static[] = {
#include "mpi_func.inc"
};
#endif //__FUNC_ID_DEF_H__

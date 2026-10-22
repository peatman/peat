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

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QtDebug>
#include <QtGlobal>

#include <QHeaderView>
#include <QProgressDialog>
#include <sstream>
#include <stdexcept>

#include "../common/common_types.h"
#include "elements.h"
#include "globals.h"
#ifdef _OPENMP
#include <omp.h>
#endif

#if 0
 // debugging function to be placed here
#endif

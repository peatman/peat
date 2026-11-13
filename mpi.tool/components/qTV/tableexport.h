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

#ifndef __TABLEEXPORT_H__
#define __TABLEEXPORT_H__

class QTableWidget;
class QTreeWidget;

// Adds a right-click menu to a table or a tree: copy the selected rows (Ctrl+C)
// or everything to the clipboard as tab separated text, and export to a CSV file.
namespace Table_Export
{
void install(QTableWidget *table);
void install(QTreeWidget *tree);
} // namespace Table_Export

#endif // __TABLEEXPORT_H__

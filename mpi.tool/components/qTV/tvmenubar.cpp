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

#include "tvmenubar.h"
#if 1
void TVMenuBar::keyPressEvent(QKeyEvent *e)
{
    switch (e->type()) {
    case QEvent::KeyPress:
        switch (e->key()) {
        case Qt::Key_Alt:
            return;
            /*
            case Qt::Key_Control:
                return;
            case Qt::Key_Shift:
                return;
             */
        }
    }
    QMenuBar::keyPressEvent(e);
}

void TVMenuBar::keyReleaseEvent(QKeyEvent *e)
{
    switch (e->type()) {
    case QEvent::KeyRelease:
        switch (e->key()) {
        case Qt::Key_Alt:
            return;
            /*
             case Qt::Key_Control:
                return;
            case Qt::Key_Shift:
                return;*/
        }
    }
    QMenuBar::keyPressEvent(e);
}
#endif